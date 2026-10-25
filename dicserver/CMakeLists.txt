cmake_minimum_required(VERSION 3.12)
project(dict_server CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# 协议定义（v1 定长 / v2 变长帧）放在仓库根的 shared/，服务端与 Qt 客户端
# 引用同一份头文件，避免两端各写一份导致字段长度/字节序漂移。
#
# 两个候选路径：本地仓库里 shared/ 与 dicserver/ 同级（../shared）；
# 部署到服务器时整个工程被解到一个目录里，shared/ 就在工程根下（./shared）。
set(SHARED_DIR "")
foreach(_candidate IN ITEMS "${CMAKE_SOURCE_DIR}/../shared" "${CMAKE_SOURCE_DIR}/shared")
    if(EXISTS "${_candidate}/protocol_frame.hpp")
        set(SHARED_DIR "${_candidate}")
        break()
    endif()
endforeach()
if(NOT SHARED_DIR)
    message(FATAL_ERROR "找不到 shared/protocol_frame.hpp（已尝试 ../shared 与 ./shared）")
endif()
message(STATUS "Using shared protocol header: ${SHARED_DIR}")

add_executable(server
    src/main.cpp
    src/server.cpp
    src/database_manager.cpp
    src/threadPool.cpp
)

target_include_directories(server PRIVATE ${CMAKE_SOURCE_DIR}/src ${SHARED_DIR})
target_link_libraries(server PRIVATE SQLite::SQLite3 Threads::Threads)

# ---------------- 单元测试 ----------------
# word_cache 是 header-only 且只依赖标准库，可脱离 SQLite/网络独立验证。
# 用 ctest 跑：cmake -S . -B build && cmake --build build && ctest --test-dir build
enable_testing()

add_executable(word_cache_test test/test_word_cache.cpp)
target_include_directories(word_cache_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${SHARED_DIR})
add_test(NAME word_cache_test COMMAND word_cache_test)

add_executable(protocol_frame_test test/test_protocol_frame.cpp)
target_include_directories(protocol_frame_test PRIVATE ${CMAKE_SOURCE_DIR}/src ${SHARED_DIR})
add_test(NAME protocol_frame_test COMMAND protocol_frame_test)
