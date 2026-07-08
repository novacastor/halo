cmake_minimum_required(VERSION 3.20)
project(LocalSearchEngine LANGUAGES CXX)

# Set modern C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Locate dependencies via vcpkg
find_package(OpenGL REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(
    include
)

# Define executable
add_executable(
    search_engine 

    src/engine/main.cpp
    src/engine/engineApp.cpp

    src/crawler/crawler.cpp    
    
    src/search/queryEngine.cpp
    
    src/indexing/indexerPipeline.cpp
    
    src/database/databaseCore.cpp
    src/database/databaseDocuments.cpp
    src/database/databaseFilesystem.cpp
    src/database/databaseDebug.cpp
    src/database/databaseSearch.cpp

    src/ui/ui.cpp
)

target_include_directories(
    search_engine 
PRIVATE 
    ${OPENGL_INCLUDE_DIR}
)

# Link everything
target_link_libraries(
    search_engine
PRIVATE
    imgui::imgui
    glfw
    SQLite3::SQLite3
    OpenGL::GL
)
