# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fake_pawr_client)

# Handle APP_DIR variable if set (to avoid CMake warning)
if(DEFINED APP_DIR)
    message(STATUS "APP_DIR is set to: ${APP_DIR}")
endif()

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

zephyr_library_include_directories(${ZEPHYR_BASE}/samples/bluetooth)
