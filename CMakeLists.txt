# ==============================================================================
#  File: CMakeLists.txt
#  Project: Schrankbeleuchtung
#  Description: CMake configuration for Raspberry Pi Pico project "Schrankbeleuchtung"
#  Author: Knut Welzel
#  Email: knut.welzel@gmail.com
#  Created: 2025-06-28
#  License: MIT
# ==============================================================================

# CMake configuration for Raspberry Pi Pico project "Schrankbeleuchtung"
# This file is used to configure the build process for the project using CMake. 
cmake_minimum_required(VERSION 3.13)

# Set the project name and version
# This is the name of the project and its version.
set(CMAKE_C_STANDARD 11)

# Set the C++ standard to C++17
# This ensures that the project uses C++17 features.
set(CMAKE_CXX_STANDARD 17)

# Set the C++ standard to C++17 with extensions
# This ensures that the project uses C++17 features with extensions.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    # Set USERHOME to the user's home directory on Windows
    # This is typically the USERPROFILE environment variable.
    set(USERHOME $ENV{USERPROFILE})
else()
    # Set USERHOME to the user's home directory on Unix-like systems
    # This is typically the HOME environment variable.
    set(USERHOME $ENV{HOME})
endif()

# Set the Raspberry Pi Pico SDK version
set(sdkVersion 2.1.1)
# Set the toolchain version
# This is the version of the toolchain used for building the project.
set(toolchainVersion 14_2_Rel1)

# Set the picotool version
set(picotoolVersion 2.1.1)
# Set the path to the Raspberry Pi Pico SDK
# Set the path to the pico-vscode.cmake file in the user's home directory
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
# If the pico-vscode.cmake file exists, include it
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# Set the board type for the Raspberry Pi Pico
# This specifies the board type for the Raspberry Pi Pico project.
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Set the project name and languages
# This sets the project name to "Schrankbeleuchtung" and specifies the languages used in the project.
# The project uses C, C++, and assembly languages.
project(Schrankbeleuchtung C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(
    Schrankbeleuchtung 
    main.cpp
    cabinetLight.cpp)

# Set the program name and version
# This sets the program name to "Schrankbeleuchtung" and the version to "0
pico_set_program_name(Schrankbeleuchtung "Schrankbeleuchtung")

# This sets the program version to "0.1"
# This is used for versioning the program.  
pico_set_program_version(Schrankbeleuchtung "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(Schrankbeleuchtung 0)
pico_enable_stdio_usb(Schrankbeleuchtung 1)

# Add the standard library to the build
target_link_libraries(Schrankbeleuchtung
        pico_stdlib
        hardware_pwm
        hardware_gpio)

# Add the standard include files to the build
target_include_directories(Schrankbeleuchtung PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add the pico_cmake module to the build
# This module provides additional CMake functionality for the Raspberry Pi Pico.
pico_add_extra_outputs(Schrankbeleuchtung)
