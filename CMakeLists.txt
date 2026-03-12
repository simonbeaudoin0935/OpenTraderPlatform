cmake_minimum_required(VERSION 3.16)

project(L2Trader VERSION 1.0.0 LANGUAGES CXX)

# Extensive usage of C++23 features
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable/disable GUI
option(ENABLE_GUI "Enable GUI support" ON)

# Option to enable UndefinedBehaviorSanitizer
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

# Option to enable AddressSanitizer
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

# Option to enable LTTng UST userspace tracepoints
option(LTTNG_ENABLED "Enable LTTng UST userspace tracepoints for profiling/debugging" OFF)

# Option to build tests
option(BUILD_TESTS "Build tests" ON)

# Check for incompatible sanitizer options
if(ENABLE_UBSAN AND ENABLE_ASAN)
    message(FATAL_ERROR "Cannot enable both UBSan and ASan simultaneously. Please choose only one.")
endif()

# Mandate GCC and Clang only
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "Only GCC and Clang compilers are supported.")
endif()

# Enable strict compiler warnings, and treat warnings as errors
add_compile_options(
    # === Treat warnings as errors (except #warning directives) ===
    -Werror                  # All warnings are errors
    -Wno-error=cpp           # Except #warning directives (intentional TODOs)

    # === Tier 0: Core warnings ===
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wcast-qual
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    -Wnon-virtual-dtor
    -Wnull-dereference
    -Woverloaded-virtual
    -Wshadow
    -Wunused
    -fdiagnostics-color=always  # Enable colored compiler output
)

# Disable unused-variable errors in Release builds when NDEBUG removes assertion variables
# The ASSUME macros expand to nothing with NDEBUG, leaving their validation variables unused
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Wno-error=unused-variable)
endif()

# Additional safety and security warnings
add_compile_options(
    -Wformat-security        # Format string vulnerabilities
    -Wstrict-overflow=2      # Pointer arithmetic overflow warnings
    -Wfloat-equal            # Dangerous floating point comparisons
    -Wundef                  # Undefined macros in #if
)

# Disable some warnings that are too noisy or incompatible with Qt
add_compile_options(
    -Wno-old-style-cast      # Qt uses C-style casts extensively
    -Wno-sign-conversion     # Qt APIs often mix signed/unsigned
    -Wno-conversion          # Too noisy with Qt
    -Wno-missing-include-dirs # Can be problematic with Qt paths
)

# Security hardening flags (runtime protections)
# Note: _FORTIFY_SOURCE is not set here as it is provided by dpkg-buildpackage
# hardening flags during debian builds and by compiler defaults otherwise.
add_compile_options(
    -fstack-protector-strong # Stack buffer overflow protection
)

# Linker security hardening
add_link_options(
    -Wl,-z,relro,-z,now     # Full RELRO: GOT hardening
    -Wl,-z,noexecstack      # Non-executable stack
)

# Debug-only runtime checks
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(_GLIBCXX_ASSERTIONS)  # STL bounds checking
    message(STATUS "Debug build: Enabled _GLIBCXX_ASSERTIONS for STL bounds checking")
endif()


# Enable sanitizers if requested (GCC and Clang only)
if(ENABLE_UBSAN)
    message(STATUS "UndefinedBehaviorSanitizer enabled")
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=undefined)
endif()

if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer enabled")
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

if(LTTNG_ENABLED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LTTNG_UST REQUIRED lttng-ust)
    add_compile_definitions(LTTNG_ENABLED)
    message(STATUS "LTTng UST enabled — userspace tracepoints active")
endif()

# Enable export of compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(ENABLE_GUI)
    # Find Qt6 GUI packages
    # Note: Using qcustomplot instead of Qt Charts
    find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
    add_compile_definitions(GUI_ENABLED)
else()
    # When building for TUI, we depend on ncurses
    find_package(Curses REQUIRED)
endif()

# Find Qt6 core packages
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Concurrent)

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

# Third-party libraries
add_subdirectory(Lib/Databento)
# databento-cpp should not be subject to our strict warning flags
target_compile_options(databento PRIVATE -w)

# Add subdirectories
add_subdirectory(Src)

# Build strategies
add_subdirectory(Strategies)

if(BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(Tests)
endif()
