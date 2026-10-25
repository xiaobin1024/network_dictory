cmake_minimum_required(VERSION 3.16)

project(protocol VERSION 0.1 LANGUAGES CXX)

# 设置自动处理
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 查找Qt包 - 修改：只查找Core模块，移除Widgets
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

# ---------- 定位 shared/（协议定义，服务端与客户端共用同一份） ----------
# 多来源探测 + 环境变量 DICT_SHARED_DIR 兜底，与 tests 里定位 vcpkg 的做法一致。
# 为什么要兜底：本机为绕开 Ninja 对非 ASCII 路径的问题，会用 junction
# （C:\wbclient -> 真实工程目录）来构建，而 `C:\wbclient\..` 会被解析成
# `C:\` 而不是真实父目录，靠相对路径找不到 shared/。
set(SHARED_DIR "")
if(DEFINED ENV{DICT_SHARED_DIR} AND EXISTS "$ENV{DICT_SHARED_DIR}/protocol_frame.hpp")
    set(SHARED_DIR "$ENV{DICT_SHARED_DIR}")
else()
    foreach(_candidate IN ITEMS
            "${CMAKE_SOURCE_DIR}/../shared"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../shared")
        if(EXISTS "${_candidate}/protocol_frame.hpp")
            get_filename_component(SHARED_DIR "${_candidate}" ABSOLUTE)
            break()
        endif()
    endforeach()
endif()
if(NOT SHARED_DIR)
    message(FATAL_ERROR
        "找不到 shared/protocol_frame.hpp。请设置环境变量 DICT_SHARED_DIR 指向仓库根的 shared 目录。")
endif()
message(STATUS "Using shared protocol header: ${SHARED_DIR}")
include_directories(${SHARED_DIR})

#包含路径，找到模块的头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})


# 创建静态库
add_library(protocol STATIC
  message.h

  app_config.h
  utils.h
  utils.cpp

  #message_dispatcher.h message_dispatcher.cpp
)

# 链接Qt库并设置包含目录 - 修改：使用PUBLIC确保依赖传递
target_link_libraries(protocol PUBLIC
    Qt${QT_VERSION_MAJOR}::Core

)

# 设置包含目录
target_include_directories(protocol PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${SHARED_DIR}>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性确保MOC正确处理
set_target_properties(protocol PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置编译器特定选项
if(MSVC)
    target_compile_options(protocol PRIVATE /W4 /permissive-)
else()
    target_compile_options(protocol PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加Qt模块的定义
target_compile_definitions(protocol PUBLIC
    QT_CORE_LIB
)
