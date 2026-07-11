#include "watcher/fileWatcher.hpp"
#include "crawler/crawler.hpp"
#include <unistd.h>
#include <filesystem>

#define MAX_EVENTS 1024 
#define LEN_NAME 128
#define EVENT_SIZE  ( sizeof (inotify_event) )

namespace Engine {
    FileWatcher::~FileWatcher() {
        stop_requested.store(true);

        if(inotify_fd > 0) close(inotify_fd);
        event_queue.push({-1, 0, ""});

        if(events_thread.joinable()) events_thread.join();
        if(handler_thread.joinable()) handler_thread.join();
    }
    bool FileWatcher::init() {
        inotify_fd = inotify_init();

        if(inotify_fd < 0) {
            LOG_ERROR("Inofity Init Failed");
            return false;
        }

        events_thread = std::thread(&FileWatcher::add_events_to_queue, this);
        handler_thread = std::thread(&FileWatcher::handle_events, this);

        LOG_INFO("FileWatcher subsystem initialized.");
        return true;
    }
    bool FileWatcher::add_watcher(const std::string &dir) {
        int wd = inotify_add_watch(inotify_fd, dir.c_str(), IN_CREATE | IN_MODIFY | IN_DELETE);

        if(wd == -1) {
            LOG_ERROR("Couldn't watch directory: " + dir);
            return false;
        }
        watch_descriptors[wd] = dir;
        return true;
    }

    bool FileWatcher::add_watchers(const std::vector<std::string> &directory_list) {
        for(auto const &dir: directory_list) {
            if(!add_watcher(dir)) return false;
        }

        return true;
    }

    void FileWatcher::remove_watchers() {
        for(auto const &it: watch_descriptors) {
            inotify_rm_watch(inotify_fd, it.first);
        }
        watch_descriptors.clear();
    }

    void FileWatcher::add_events_to_queue() {
        size_t BUF_LEN = MAX_EVENTS * (EVENT_SIZE + LEN_NAME);
        std::vector<char> buffer(BUF_LEN);
        while(!stop_requested.load()) {
            int i = 0, length;
            length = read(inotify_fd, buffer.data(), buffer.size());

            if(length < 0) {
                if(stop_requested.load()) break;
                LOG_ERROR("Failed to read inotify events. ");
                continue;
            }

            while(i < length) {
                inotify_event *event = (inotify_event *) &buffer[i];
                std::filesystem::path p(event->name);
                std::string ext = p.extension().string();
                if(event->len) event_queue.push({event->wd, event->mask, event->name});
                i += EVENT_SIZE + event->len;
            }
        }
    }

    void FileWatcher::handle_new_directory_event(const FileEvent &event) {
        std::string path = watch_descriptors[event.wd] + "/" + event.name;
        add_watcher(path);
        db.add_directory(path);
    }

    void FileWatcher::handle_file_change_event(const FileEvent &event) {
        std::string path = watch_descriptors[event.wd] + "/" + event.name;
        std::string ext = std::filesystem::path(event.name).extension().string();
        
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(path, ec);
        if(ec) {
            LOG_ERROR("Couldn't stat file (likely deleted before processing): " + path);
            return;
        }

        long long mtime = std::chrono::duration_cast<std::chrono::seconds>(
            std::filesystem::file_time_type::clock::to_sys(ftime).time_since_epoch()
        ).count();

        db.insert_file(event.name, ext, path);
        if(Engine::EXTENSION_WHITELIST.find(ext) != Engine::EXTENSION_WHITELIST.end()) {
            pipeline.add_job_to_queue({path, mtime});
        }
    }

    void FileWatcher::handle_file_delete_event(const FileEvent &event) {
        std::string path = watch_descriptors[event.wd] + "/" + event.name;
        std::string ext = std::filesystem::path(event.name).extension().string();

        db.delete_file(path);
        if(EXTENSION_WHITELIST.find(ext) != EXTENSION_WHITELIST.end()) db.delete_document(path);

        LOG_INFO("Reindexed file: " + path);
    }

    void FileWatcher::handle_directory_delete_event(const FileEvent &event) {
        std::string path = watch_descriptors[event.wd] + "/" + event.name;
        db.delete_directory(path);
        db.delete_documents_under_directory(path);
    }


    void FileWatcher::handle_events() {
        while(!stop_requested.load()) {
            FileEvent event = event_queue.pop();

            if(event.wd == -1) break;
            std::string path = watch_descriptors[event.wd] + "/" + event.name;
            std::string ext = std::filesystem::path(event.name).extension().string();

            if ( event.mask & IN_CREATE) {
                if (event.mask & IN_ISDIR) {
                    LOG_INFO("New directory created: " + path);
                    handle_new_directory_event(event);
                } else {
                    if(Engine::EXTENSION_WHITELIST.find(ext) != Engine::EXTENSION_WHITELIST.end()) {
                        LOG_INFO("New file created: " + path);
                    }
                    handle_file_change_event(event);
                }
            } else if ( event.mask & IN_MODIFY) {
                if (event.mask & IN_ISDIR) { 
                    LOG_INFO("Directory modified: " + path);   
                } else {
                    if(Engine::EXTENSION_WHITELIST.find(ext) != Engine::EXTENSION_WHITELIST.end()) {
                        LOG_INFO("File modified: " + path);
                    }
                    handle_file_change_event(event);
                }
            } else if ( event.mask & IN_DELETE) {
                if (event.mask & IN_ISDIR) {
                    LOG_INFO("Directory deleted: " + path);  
                    handle_directory_delete_event(event);
                } else {
                    LOG_INFO("File deleted: " + path);
                    handle_file_delete_event(event);
                }
            }
        }
    }
}