cmake_minimum_required(VERSION 3.16)

project(L2Trader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable UndefinedBehaviorSanitizer
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Option to enable AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcast-align -Wcast-qual -Wconversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wmissing-include-dirs -Wnon-virtual-dtor")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnull-dereference -Wold-style-cast -Woverloaded-virtual")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wshadow -Wsign-conversion -Wunused")

# Enable sanitizers if requested
if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -fno-sanitize-recover=all")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
endif()

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# Disable exceptions - must define QT_NO_EXCEPTIONS for Qt headers to match
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
#add_compile_definitions(QT_NO_EXCEPTIONS)

# Enable export of compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option to enable/disable GUI
option(ENABLE_GUI "Enable GUI support" ON)

if(ENABLE_GUI)
    # Find Qt6 GUI packages
    # Note: Using qcustomplot instead of Qt Charts
    find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
    add_compile_definitions(GUI_ENABLED)
else()
    # When building for TUI, we depend oon ncurses
    find_packages(Curses REQUIRED)
endif()

# Option to build tests
option(BUILD_TESTS "Build tests" ON)

# Find Qt6 coore packages
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Test Concurrent)

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

# Enable stack trace symbol resolution using C++23 stacktrace
# No longer need -rdynamic flag as C++23 handles this internally
# Library stdc++_libbacktrace is needed for stacktrace support
if(UNIX AND NOT APPLE)
    link_libraries(stdc++_libbacktrace)
endif()

# Add subdirectories
add_subdirectory(Src)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
