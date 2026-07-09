#include "engine/log.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace Engine {
    std::ofstream Log::file;
    std::mutex Log::log_mutex;
    static const char* level_str(LogLevel lvl) {
        switch (lvl) {
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "?";
    }

    void Log::init(const std::string& path) {
        std::lock_guard<std::mutex> lock(log_mutex);
        file.open(path, std::ios::app);
    }

    void Log::write(LogLevel level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf{};
        localtime_r(&t, &tm_buf);

        std::ostringstream line;
        line << "[" << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << "] "
             << "[" << level_str(level) << "] " << msg;

        std::lock_guard<std::mutex> lock(log_mutex);
        std::cerr << line.str() << std::endl; 
        if (file.is_open()) file << line.str() << std::endl;
    }

    void Log::shutdown() {
        std::lock_guard<std::mutex> lock(log_mutex);
        if (file.is_open()) file.close();
    }
}