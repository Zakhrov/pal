##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

cmake_minimum_required(VERSION 3.5)
project(PAL LANGUAGES C CXX)

### CMake Includes #####################################################################################################
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake/Modules)
include(CMakeDependentOption)
include(AmdCmakeHelper)

### Cached Project Options #############################################################################################
# CMAKE-TODO: To support multiple configurations for Visual Studio we can't use CMAKE_BUILD_TYPE
# We would have to override the options using cmake generator expressions and possibly include an
# option to disable overrides such as PAL_DISABLE_INTERNAL_OPTION_OVERRIDES

option(PAL_BUILD_GPUOPEN "Build GPUOpen developer driver support?" OFF)

option(PAL_DBG_COMMAND_COMMENTS "Command with comments" OFF)

option(PAL_DEVELOPER_BUILD "Enable developer build" OFF)

option(PAL_ENABLE_PRINTS_ASSERTS "Enable print assertions?" ${CMAKE_BUILD_TYPE_DEBUG})
cmake_dependent_option(PAL_MEMTRACK "Enable PAL memory tracker?" ${CMAKE_BUILD_TYPE_DEBUG} "PAL_ENABLE_PRINTS_ASSERTS" OFF)

option(PAL_BUILD_CORE "Build PAL Core?" ON)
option(PAL_BUILD_GPUUTIL "Build PAL GPU Util?" ON)
cmake_dependent_option(PAL_BUILD_LAYERS "Build PAL Layers?" ON "PAL_BUILD_GPUUTIL" OFF)

option(PAL_BUILD_DBG_OVERLAY "Build PAL Debug Overlay?" ON)

option(PAL_BUILD_GPU_PROFILER "Build PAL GPU Profiler?" ON)

option(PAL_BUILD_CMD_BUFFER_LOGGER "Build PAL Command Buffer Logger?" ${CMAKE_BUILD_TYPE_DEBUG})
option(PAL_BUILD_INTERFACE_LOGGER  "Build PAL Interface Logger?"      ${CMAKE_BUILD_TYPE_DEBUG})
option(PAL_BUILD_PM4_INSTRUMENTOR  "Build PAL PM4 Instrumentor?"      ${CMAKE_BUILD_TYPE_DEBUG})

option(PAL_BUILD_GFX  "Build PAL with Graphics support?" ON)
cmake_dependent_option(PAL_BUILD_GFX6 "Build PAL with GFX6 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(PAL_BUILD_GFX9 "Build PAL with GFX9 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(PAL_BUILD_VEGA20 "Build PAL with Vega20 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(CHIP_HDR_VEGA20 "Build PAL chip with Vega20 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(PAL_BUILD_RAVEN2 "Build PAL with Raven2 support?" ON "PAL_BUILD_GFX" OFF)
cmake_dependent_option(CHIP_HDR_RAVEN2 "Build PAL chip with Raven2 support?" ON "PAL_BUILD_GFX" OFF)

option(PAL_BUILD_GFX10 "Build PAL with GFX10 support?" OFF)

option(PAL_BUILD_OSS  "Build PAL with Operating System support?" ON)
cmake_dependent_option(PAL_BUILD_OSS1   "Build PAL with OSS1?"   ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS2   "Build PAL with OSS2?"   ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS2_4 "Build PAL with OSS2_4?" ON "PAL_BUILD_OSS" OFF)
cmake_dependent_option(PAL_BUILD_OSS4   "Build PAL with OSS4?"   ON "PAL_BUILD_OSS" OFF)

option(PAL_BUILD_WAYLAND "Build PAL with WAYLAND support?" OFF)

# PAL Client Options ###############################################################################
# Use Vulkan as the default client.
set(PAL_CLIENT "VULKAN" CACHE STRING "Client interfacing with PAL.")

# Read the latest version from palLib.h
if(DEFINED PAL_CLIENT_INTERFACE_MAJOR_VERSION)
    message(STATUS "Client configured PAL_INTERFACE_MAJOR_VERSION as " ${PAL_CLIENT_INTERFACE_MAJOR_VERSION})
else()
    file(STRINGS inc/core/palLib.h PAL_MAJOR_VERSION REGEX "^#define PAL_INTERFACE_MAJOR_VERSION [0-9]+")

    if(PAL_MAJOR_VERSION STREQUAL "")
        message(STATUS "Failed to find PAL_INTERFACE_MAJOR_VERSION")
    else()
        string(REGEX REPLACE "^#define PAL_INTERFACE_MAJOR_VERSION " "" PAL_MAJOR_VERSION ${PAL_MAJOR_VERSION})
        message(STATUS "Detected PAL_INTERFACE_MAJOR_VERSION is " ${PAL_MAJOR_VERSION})
    endif()
    set(PAL_CLIENT_INTERFACE_MAJOR_VERSION ${PAL_MAJOR_VERSION})
endif()

# Option Overrides #################################################################################
# These are options that override PAL subproject options.  As these overrides are managed and force
# set by PAL, mark_as_advanced is used to hide them from the CMake GUI.

# ADDRLIB
set(PAL_ADDR_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/addrlib CACHE PATH "Specify the path to the ADDRLIB project.")
set(ADDR_OPTIONS_MARK_ADVANCED ON)

set(ADDR_SI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with SI support." FORCE)
set(ADDR_CI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with CI support." FORCE)
set(ADDR_VI_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with VI support." FORCE)

set(ADDR_SI_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx6/chip CACHE PATH "PAL override for ADDRLIB SI/CI/VI register chip headers." FORCE)

if(PAL_BUILD_GFX9)
    set(ADDR_GFX9_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with GFX9 support." FORCE)
    set(ADDR_GFX9_CHIP_DIR ${PROJECT_SOURCE_DIR}/src/core/hw/gfxip/gfx9/chip CACHE PATH "PAL override for ADDRLIB GFX9 register chip headers." FORCE)
    set(ADDR_VEGA12_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with Vega12 support." FORCE)

    if(PAL_BUILD_VEGA20)
        set(ADDR_VEGA20_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with Vega20 support." FORCE)
        set(CHIP_HDR_VEGA20 ON CACHE BOOL "PAL override to build chip register header with Vega20 support." FORCE)
    else()
        set(ADDR_VEGA20_BUILD OFF CACHE BOOL "PAL override to build ADDRLIB with Vega20 support." FORCE)
	set(CHIP_HDR_VEGA20 OFF CACHE BOOL "PAL override to build chip register header with Vega20 support." FORCE)
    endif()
    if(PAL_BUILD_RAVEN2)
        set(ADDR_RAVEN2_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with Raven2 support." FORCE)
        set(CHIP_HDR_RAVEN2 ON CACHE BOOL "PAL override to build chip register header with Raven2 support." FORCE)
    else()
        set(ADDR_RAVEN2_BUILD OFF CACHE BOOL "PAL override to build ADDRLIB with Vega20 support." FORCE)
	set(CHIP_HDR_RAVEN2 OFF CACHE BOOL "PAL override to build chip register header with Raven2 support." FORCE)
    endif()
    if(PAL_BUILD_GFX10)
        set(ADDR_GFX10_BUILD ON CACHE BOOL "PAL override to build ADDRLIB with GFX10 support." FORCE)
    endif()
endif()

# METROHASH
set(PAL_METROHASH_PATH ${PROJECT_SOURCE_DIR}/../MetroHash CACHE PATH "Specify the path to the MetroHash project.")

# CWPACK
set(PAL_CWPACK_PATH ${PROJECT_SOURCE_DIR}/../CWPack CACHE PATH "Specify the path to the CWPack project.")

# VAM
set(PAL_VAM_PATH ${PROJECT_SOURCE_DIR}/src/core/imported/vam CACHE PATH "Specify the path to the VAM project.")

# GPUOPEN
if(PAL_BUILD_GPUOPEN)
    set(PAL_GPUOPEN_PATH ${PROJECT_SOURCE_DIR}/shared/gpuopen CACHE PATH "Specify the path to the GPUOPEN_PATH project.")
    set(GPUOPEN_BUILD_METROHASH OFF CACHE BOOL "PAL override to build GPUOpen without the Metrohash library since PAL has its own." FORCE)
    set(METROHASH_PATH ${PAL_METROHASH_PATH}/src CACHE PATH "Specify the path to the MetroHash module.")
    set(GPUOPEN_BUILD_SERVER_HELPERS ON CACHE BOOL "PAL override to build GPUOpen with server helper classes." FORCE)
    set(GPUOPEN_BUILD_STANDARD_DRIVER_PROTOCOLS ON CACHE BOOL "PAL override to build GPUOpen with support for the standard driver protocols." FORCE)
endif()

option(PAL_ENABLE_DEVDRIVER_USAGE "Enables developer driver suppport." ON)

### Generator Dependencies #############################################################################################
if(${CMAKE_VERSION} VERSION_GREATER "3.12")
    # If we have a newer cmake, the new FindPython scripts are much more reliable
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON_CMD ${Python3_EXECUTABLE})
else()
    find_package(PythonInterp 3)
    if(NOT PYTHONINTERP_FOUND)
        if(UNIX)
            message(FATAL_ERROR "Python 3 is needed to generate some source files.")
        endif()
    endif()
    set(PYTHON_CMD ${PYTHON_EXECUTABLE})
endif() # cmake version

find_package(Perl 5)
if(NOT PERL_FOUND)
    if(UNIX)
        message(FATAL_ERROR "Perl 5 is needed to generate some source files.")
    endif()
endif()
set(PERL_CMD ${PERL_EXECUTABLE})

### Add Subdirectories #################################################################################################
add_subdirectory(src)
