#pragma once
#include <string>
#include <mutex>
#include <fstream>

namespace Engine {
    enum class LogLevel {Debug, Info, Warn, Error};

    class Log {
    public:
        static void init(const std::string &path);
        static void write(LogLevel level, const std::string &msg);
        static void shutdown();

    private:
        static std::ofstream file;
        static std::mutex log_mutex;
    };
}

#define LOG_DEBUG(msg) Engine::Log::write(Engine::LogLevel::Debug, msg)
#define LOG_INFO(msg)  Engine::Log::write(Engine::LogLevel::Info,  msg)
#define LOG_WARN(msg)  Engine::Log::write(Engine::LogLevel::Warn,  msg)
#define LOG_ERROR(msg) Engine::Log::write(Engine::LogLevel::Error, msg)