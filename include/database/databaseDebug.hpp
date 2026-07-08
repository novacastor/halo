#pragma once
#include "database/database.hpp"
#include "engine/config.hpp"

namespace Engine {
    void print_inverted_index(Database &db);
    void print_db_size(const std::string &db_path);
}