#include "watcher/fileWatcher.hpp"
#include "crawler/crawler.hpp"
#include "engine/fileTime.hpp"
#include <unistd.h>
#include <filesystem>
#include <cerrno>
#include <chrono>
#include <thread>

#define MAX_EVENTS 1024 
#define LEN_NAME 128
#define EVENT_SIZE  ( sizeof (inotify_event) )

namespace Engine {
    FileWatcher::~FileWatcher() {
        stop_requested.store(true);

        if(inotify_fd >= 0) {
            close(inotify_fd);
        }
        event_queue.push({-1, 0, ""});

        if(events_thread.joinable()) events_thread.join();
        if(handler_thread.joinable()) handler_thread.join();
    }
    bool FileWatcher::init() {
        inotify_fd = inotify_init1(IN_NONBLOCK);

        if(inotify_fd < 0) {
            LOG_ERROR("inotify initialization failed.");
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
        {
            std::lock_guard<std::mutex> lock(watch_descriptors_mutex);
            watch_descriptors[wd] = dir;
        }
        return true;
    }

    std::string FileWatcher::get_watch_path(const FileEvent &event) const {
        std::lock_guard<std::mutex> lock(watch_descriptors_mutex);
        auto it = watch_descriptors.find(event.wd);
        if(it == watch_descriptors.end()) return {};
        return (std::filesystem::path(it->second) / event.name).string();
    }

    bool FileWatcher::add_watchers(const std::vector<std::string> &directory_list) {
        for(auto const &dir: directory_list) {
            if(!add_watcher(dir)) return false;
        }

        return true;
    }

    void FileWatcher::remove_watchers() {
        std::lock_guard<std::mutex> lock(watch_descriptors_mutex);
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
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
                LOG_ERROR("Failed to read inotify events. ");
                continue;
            }

            while(i < length) {
                inotify_event *event = (inotify_event *) &buffer[i];
                if(event->len) event_queue.push({event->wd, event->mask, event->name});
                i += EVENT_SIZE + event->len;
            }
        }
    }

    void FileWatcher::handle_new_directory_event(const FileEvent &event) {
        std::string path = get_watch_path(event);
        if(path.empty()) return;
        const std::string name = std::filesystem::path(path).filename().string();
        if (Engine::FOLDER_BLACKLIST.find(name) != Engine::FOLDER_BLACKLIST.end() ||
            Engine::GLOBAL_FOLDER_BLACKLIST.find(name) != Engine::GLOBAL_FOLDER_BLACKLIST.end()) return;
        add_watcher(path);
        db.add_directory(path);
    }

    void FileWatcher::handle_file_change_event(const FileEvent &event) {
        std::string path = get_watch_path(event);
        if(path.empty()) return;
        std::string ext = std::filesystem::path(event.name).extension().string();

        Engine::Crawler crawler;
        if (!crawler.check_file(event.name, ext)) {
            db.delete_file(path);
            db.delete_document(path);
            return;
        }
        
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(path, ec);
        if(ec) {
            LOG_ERROR("Couldn't stat file (likely deleted before processing): " + path);
            return;
        }

        long long mtime = Engine::file_time_to_unix_nanoseconds(ftime);

        db.insert_file(event.name, ext, path);
        if(Engine::EXTENSION_WHITELIST.find(ext) != Engine::EXTENSION_WHITELIST.end()) {
            pipeline.add_job_to_queue({path, mtime});
        }
    }

    void FileWatcher::handle_file_delete_event(const FileEvent &event) {
        std::string path = get_watch_path(event);
        if(path.empty()) return;

        db.delete_file(path);
        db.delete_document(path);

        LOG_INFO("Reindexed file: " + path);
    }

    void FileWatcher::handle_directory_delete_event(const FileEvent &event) {
        std::string path = get_watch_path(event);
        if(path.empty()) return;
        db.delete_directory(path);
        db.delete_documents_under_directory(path);
    }


    void FileWatcher::handle_events() {
        while(!stop_requested.load()) {
            FileEvent event = event_queue.pop();

            if(event.wd == -1) break;
            std::string path = get_watch_path(event);
            if(path.empty()) continue;
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
