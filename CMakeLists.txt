cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 0 = 正弦速度，1 = 正弦位置；可用 -DGM6020_DEMO_MODE=1 选择。
set(GM6020_DEMO_MODE "0" CACHE STRING "GM6020 sine demo mode (0=speed, 1=position)")
option(GM6020_CAN_LOOPBACK_DIAG "Build CAN1 silent internal loopback diagnostic" OFF)
option(GM6020_CURRENT_CONTROL "Use GM6020 CAN current control frames" ON)
option(GM6020_DIRECTION_DIAG "One-shot low-current direction check" OFF)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME C_project)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
if(GM6020_CAN_LOOPBACK_DIAG)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE Diagnostics/CanLoopbackDiag.c)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE GM6020_CAN_LOOPBACK_DIAG=1)
else()
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        Tasks/src/ControlTask.cpp
        Tasks/src/Gm6020.cpp
        Tasks/src/HalCallbacks.cpp
        Tasks/src/Pid.cpp
    )
endif()

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    Tasks/inc
    Diagnostics
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    SINE_DEFAULT_MODE=${GM6020_DEMO_MODE}
    GM6020_CURRENT_CONTROL=$<BOOL:${GM6020_CURRENT_CONTROL}>
    GM6020_DIRECTION_DIAG=$<BOOL:${GM6020_DIRECTION_DIAG}>
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
