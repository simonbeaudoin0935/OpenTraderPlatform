cmake_minimum_required(VERSION 3.16)

project(L2Trader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable export of compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Widgets Charts WebEngineWidgets Test)

# Configure ccache for faster rebuilds
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
endif()

# Enable automatic handling of Qt features
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Get version information from git
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Set default values if git commands fail, using environment variables if available
if(NOT GIT_TAG)
    if(DEFINED ENV{GIT_TAG})
        set(GIT_TAG "$ENV{GIT_TAG}")
    else()
        set(GIT_TAG "unknown")
    endif()
endif()
if(NOT GIT_HASH)
    if(DEFINED ENV{GIT_HASH})
        set(GIT_HASH "$ENV{GIT_HASH}")
    else()
        set(GIT_HASH "unknown")
    endif()
endif()
if(NOT GIT_BRANCH)
    if(DEFINED ENV{GIT_BRANCH})
        set(GIT_BRANCH "$ENV{GIT_BRANCH}")
    else()
        set(GIT_BRANCH "unknown")
    endif()
endif()

# Add version defines
add_compile_definitions(
    GIT_TAG="${GIT_TAG}"
    GIT_HASH="${GIT_HASH}"
    GIT_BRANCH="${GIT_BRANCH}"
)

# Enable stack trace symbol resolution
if(UNIX AND NOT APPLE)
    add_link_options(-rdynamic)
endif()

# Option to enable/disable GUI
option(ENABLE_GUI "Enable GUI support" ON)

if(ENABLE_GUI)
    add_compile_definitions(GUI_ENABLED)
endif()

# Option to build tests
option(BUILD_TESTS "Build tests" ON)

# Option to build recorder
option(BUILD_RECORDER "Build recorder application" ON)

# Add subdirectories
add_subdirectory(src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
