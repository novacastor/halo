#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace TestSupport {
    class TemporaryDirectory {
    public:
        TemporaryDirectory() {
            static std::atomic<unsigned long long> next_id{0};
            path_ = std::filesystem::temp_directory_path() /
                ("search_engine_test_" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                    std::to_string(next_id.fetch_add(1)));
            std::filesystem::create_directories(path_);
        }

        ~TemporaryDirectory() {
            std::error_code error;
            std::filesystem::remove_all(path_, error);
        }

        const std::filesystem::path& path() const { return path_; }

        std::filesystem::path write_file(const std::string& name, const std::string& contents) const {
            const auto file_path = path_ / name;
            std::filesystem::create_directories(file_path.parent_path());
            std::ofstream file(file_path, std::ios::binary);
            file << contents;
            return file_path;
        }

    private:
        std::filesystem::path path_;
    };

    template <typename Predicate>
    bool wait_until(Predicate predicate, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return predicate();
    }
}
