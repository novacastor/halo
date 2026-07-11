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
    src/engine/log.cpp

    src/crawler/crawler.cpp    
    
    src/search/queryEngine.cpp
    
    src/indexing/indexerPipeline.cpp
    
    src/database/databaseCore.cpp
    src/database/databaseDocuments.cpp
    src/database/databaseFilesystem.cpp
    src/database/databaseDebug.cpp
    src/database/databaseSearch.cpp

    src/ui/ui.cpp

    src/watcher/fileWatcher.cpp
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

# 1. Install the main binary
install(TARGETS search_engine DESTINATION bin)

install(FILES halo_search.desktop DESTINATION share/applications)

# 3. Install an icon (Optional, but recommended)
# If you don't have an icon yet, create an empty file called 'icon.png' in an 'assets' folder,
# or simply delete these next three lines for now so it doesn't crash.
install(FILES assets/icon.png DESTINATION share/icons/hicolor/256x256/apps RENAME halo-icon.png)