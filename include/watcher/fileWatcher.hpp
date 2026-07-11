#pragma once

#include "engine/log.hpp"
#include "indexing/workQueue.hpp"
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
        ~FileWatcher();

        bool init();
        bool add_watchers(const std::vector<std::string> &directory_list);
        void remove_watchers();
    
    private:
        bool add_watcher(const std::string &dir);
        void add_events_to_queue();
        void handle_event();

        std::thread events_thread;
        std::thread handler_thread;
        std::atomic<bool> stop_requested{false};
        int inotify_fd;
        WorkQueue<FileEvent> event_queue;
        std::unordered_map<int, std::string> watch_descriptors;
    };
}