#pragma once

#include <cstdlib> 
#include <string>
#include <iostream>


namespace Engine {
    inline std::string get_home_directory() {
        if (const char* home_env = std::getenv("HOME")) {
           return std::string(home_env); 
        } else {
            std::cerr << "Warning: Could not find HOME directory, defaulting to root.\n";
            return "/"; 
        }
    }
    struct Config{
        std::string database = "search_engine.db";
        std::string root_directory = get_home_directory();
    };
}