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
