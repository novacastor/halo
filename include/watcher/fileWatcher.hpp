#pragma once

#include "engine/log.hpp"
#include "indexing/workQueue.hpp"
#include "indexing/indexerPipeline.hpp"
#include "database/database.hpp"
#include <sys/inotify.h>
#include <filesystem>
#include <vector>
#include <thread>
#include <unordered_map>

namespace Engine {
    struct FileEvent {
        int wd;
        uint32_t mask;
        std::string name;
    };
    class FileWatcher{
    public:
        FileWatcher(Database &db, IndexerPipeline &p) : db(db), pipeline(p) {}
        ~FileWatcher();

        bool init();
        bool add_watchers(const std::vector<std::string> &directory_list);
        void remove_watchers();
    
    private:
        bool add_watcher(const std::string &dir);
        void add_events_to_queue();
        void handle_events();
        void handle_new_directory_event(const FileEvent &event);
        void handle_file_change_event(const FileEvent &event);
        void handle_file_delete_event(const FileEvent &event);
        void handle_directory_delete_event(const FileEvent &event);

        std::thread events_thread;
        std::thread handler_thread;
        std::atomic<bool> stop_requested{false};
        int inotify_fd;
        WorkQueue<FileEvent> event_queue;
        std::unordered_map<int, std::string> watch_descriptors;

        IndexerPipeline &pipeline;
        Database &db;
    };
}

//Finally it worked