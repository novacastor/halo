cmake_minimum_required(VERSION 3.20)
project(LocalSearchEngine LANGUAGES CXX)

# Set modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate dependencies via vcpkg
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(
    include
)

# Define executable
add_executable(
    search_engine 
    src/main.cpp
    src/crawler.cpp    
    src/tokenizer.cpp
    src/database.cpp
    src/threadpool.cpp
    src/concurrentQueue.cpp
    src/queryEngine.cpp
    src/pipeline.cpp
)

# Link everything
target_link_libraries(
    search_engine
PRIVATE
   unofficial::sqlite3::sqlite3
    imgui::imgui
    glfw
    SQLite3::SQLite3
)
