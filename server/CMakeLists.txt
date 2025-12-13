cmake_minimum_required(VERSION 3.16)
if(POLICY CMP0167)
    # 使用传统 FindBoost 模式
    cmake_policy(SET CMP0167 OLD)
endif()

# Boost 设置（优先 Homebrew 位置，使用 FindBoost；仅头文件）
set(Boost_ROOT "/opt/homebrew/opt/boost")
set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost REQUIRED)  # header-only，用 Boost::boost 目标

# SQLite
find_package(SQLite3 REQUIRED)

# 收集所有 cpp/h 文件（自动跟踪新文件变更）
file(GLOB SERVER_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/core/*.cpp
    src/db/*.cpp
    src/handlers/*.cpp
    src/dag/*.cpp
    src/runner/*.cpp
    src/execution/*.cpp
    src/scheduler/*.cpp
    src/worker/*.cpp
)

file(GLOB SERVER_HEADERS CONFIGURE_DEPENDS
    src/*.h
    src/core/*.h
    src/db/*.h
    src/handlers/*.h
    src/dag/*.h
    src/runner/*.h
    src/execution/*.h
    src/scheduler/*.h
    src/worker/*.h
)

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/httplib
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann_json
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/db
    ${Boost_INCLUDE_DIRS}
    ${SQLite3_INCLUDE_DIRS}
)

# 创建可执行文件
add_executable(taskhub_server
    ${SERVER_SOURCES}
    ${SERVER_HEADERS}
)

# 链接 Boost/SQLite
target_link_libraries(taskhub_server
    PRIVATE
    Boost::boost
    SQLite::SQLite3
)

# 拷贝配置文件到运行目录
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config/default_config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)
#

file(GLOB TASKHUB_MIGRATIONS "${CMAKE_CURRENT_SOURCE_DIR}/migrations/*.sql")
foreach(mig ${TASKHUB_MIGRATIONS})
    get_filename_component(fname "${mig}" NAME)
    configure_file(${mig}
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/migrations/${fname}"
        COPYONLY)
endforeach()
