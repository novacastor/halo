#pragma once

#include <chrono>
#include <filesystem>

namespace Engine {
    inline long long file_time_to_unix_nanoseconds(std::filesystem::file_time_type file_time) {
        const auto system_time = std::filesystem::file_time_type::clock::to_sys(file_time);
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            system_time.time_since_epoch()).count();
    }
}
