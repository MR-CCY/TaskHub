cmake_minimum_required(VERSION 3.21)
project(TaskHubClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS 
        Core
        Widgets
        WebSockets
        
        Network)

# 收集所有 cpp/h 文件（开启 CONFIGURE_DEPENDS，新增文件自动触发重新配置）
file(GLOB CLIENT_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/core/*.cpp
    src/models/*.cpp
    src/ui/*.cpp
    src/net/*.cpp
    src/graph/*.cpp
    src/view/*.cpp
    src/commands/*.cpp
    src/operator/*.cpp
    src/Item/*.cpp
    src/view/items/*.cpp
    src/tasks/*.cpp
    src/editor/*.cpp
)

file(GLOB CLIENT_HEADERS CONFIGURE_DEPENDS
    src/*.h
    src/core/*.h
    src/models/*.h
    src/ui/*.h
    src/net/*.h
    src/graph/*.h
    src/view/*.h
    src/commands/*.h
    src/operator/*.h
    src/Item/*.h
    src/view/items/*.h
    src/tasks/*.h
    src/editor/*.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/view
    ${CMAKE_CURRENT_SOURCE_DIR}/src/tasks
    ${CMAKE_CURRENT_SOURCE_DIR}/src/commands
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Item
    ${CMAKE_CURRENT_SOURCE_DIR}/src/operator
)


target_link_libraries(TaskHubClient PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::WebSockets
)
