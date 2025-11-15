cmake_minimum_required(VERSION 3.16)

# 收集所有 cpp/h 文件
file(GLOB SERVER_SOURCES
    src/*.cpp
    src/core/*.cpp
    src/handlers/*.cpp
)

file(GLOB SERVER_HEADERS
    src/*.h
    src/core/*.h
    src/handlers/*.h
)

# 包含 third_party 的头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 创建可执行文件
add_executable(taskhub_server
    ${SERVER_SOURCES}
    ${SERVER_HEADERS}
)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)
# future: 后续可以在这里加依赖
# target_link_libraries(taskhub_server pthread)