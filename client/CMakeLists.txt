cmake_minimum_required(VERSION 3.21)
project(TaskHubClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
        Core
        Widgets
        Network)

# 收集所有 cpp/h 文件
file(GLOB CLIENT_SOURCES
    src/*.cpp
    src/core/*.cpp
    src/models/*.cpp
    src/ui/*.cpp
)

file(GLOB CLIENT_HEADERS
    src/*.h
    src/core/*.h
    src/models/*.h
    src/ui/*.h
)

file(GLOB SCLIENT_UI
    src/*.ui
    src/core/*.ui
    src/models/*.ui
    src/ui/*.ui
)
# 创建可执行文件
add_executable(TaskHubClient
    ${CLIENT_SOURCES}
    ${CLIENT_HEADERS}
    ${SCLIENT_UI}
)

target_include_directories(TaskHubClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)


target_link_libraries(TaskHubClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)
