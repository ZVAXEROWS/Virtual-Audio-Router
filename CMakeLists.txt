cmake_minimum_required(VERSION 3.25)

project(VirtualAudioRouter
    VERSION 0.1.0
    DESCRIPTION "Virtual Audio Router — route one audio source to multiple outputs"
    LANGUAGES CXX
)

# ===========================================================================
# Global C++ standard
# ===========================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ===========================================================================
# Build type defaults
# ===========================================================================

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    message(STATUS "VAR: Build type defaulted to Debug")
endif()

# ===========================================================================
# Output directories — put all binaries in one place for easy development
# ===========================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ===========================================================================
# Version header generation
# ===========================================================================

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/var/version.h"
)

# ===========================================================================
# Windows-specific settings
# ===========================================================================

if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
        _WIN32_WINNT=0x0A00   # Windows 10+
    )
    # WASAPI / COM libraries (linked by engine target)
    set(VAR_WINDOWS_LIBS
        ole32        # COM fundamentals (CoInitializeEx, CoCreateInstance)
        oleaut32     # Automation (PROPVARIANT)
        uuid         # Interface GUIDs
        winmm        # Legacy audio (mmsystem)
        mmdevapi     # IMMDeviceEnumerator, IMMDevice, IAudioClient
        ksuser       # KSDATAFORMAT GUIDs
    )
endif()

# ===========================================================================
# Compiler warnings
# ===========================================================================

if(MSVC)
    add_compile_options(
        /W4
        /WX         # Treat warnings as errors
        /permissive-
        /Zc:__cplusplus
        /wd4100     # unreferenced formal parameter (common in stubs)
        /wd4505     # unreferenced local function
    )
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter
    )
endif()

# ===========================================================================
# pybind11 — via FetchContent (no external tooling required)
# ===========================================================================

include(FetchContent)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.13.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)

# ===========================================================================
# GoogleTest — for unit tests
# ===========================================================================

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
# Prevent GTest from overriding MSVC runtime settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ===========================================================================
# Subdirectories
# ===========================================================================

add_subdirectory(engine)
add_subdirectory(bindings)
add_subdirectory(tests)

# ===========================================================================
# Summary
# ===========================================================================

message(STATUS "")
message(STATUS "=== Virtual Audio Router v${PROJECT_VERSION} ===")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Generator   : ${CMAKE_GENERATOR}")
message(STATUS "  Output dir  : ${CMAKE_BINARY_DIR}/bin")
message(STATUS "")
