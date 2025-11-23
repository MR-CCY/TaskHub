cmake_minimum_required(VERSION 3.21)
project(TaskHub)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 输出更详细构建信息（可选）
set(CMAKE_VERBOSE_MAKEFILE OFF)

# 让路径更整洁
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BUILD_TASKHUB_SERVER "Build the TaskHub server backend" ON)
option(BUILD_TASKHUB_CLIENT "Build the Qt TaskHubClient front-end" ON)

if(BUILD_TASKHUB_SERVER)
    add_subdirectory(server)
endif()

if(BUILD_TASKHUB_CLIENT)
    add_subdirectory(client)
endif()
