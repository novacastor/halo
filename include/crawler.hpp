#pragma once
#include "database.hpp"
#include <string>
#include <thread>

pair<int, int> run_crawler(const std::string &target_path, Engine::Database &db);