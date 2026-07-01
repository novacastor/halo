cmake_minimum_required(VERSION 3.20)
project(LocalSearchEngine LANGUAGES CXX)

# Set modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate dependencies via vcpkg
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

include_directories(
    include
)

# Define executable
add_executable(
    search_engine 
    src/main.cpp
    src/crawler.cpp    
)

# Link everything
target_link_libraries(search_engine
    PRIVATE
       unofficial::sqlite3::sqlite3
        imgui::imgui
        glfw
)
