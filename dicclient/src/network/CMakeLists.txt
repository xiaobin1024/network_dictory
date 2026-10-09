cmake_minimum_required(VERSION 3.16)

project(network VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

#包含路径，找到core模块的头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR})

#创建静态库
add_library(network STATIC

  #network.cpp
  #network.h
  network_manager.cpp
  network_manager.h
  connection_manager.h connection_manager.cpp
  message_queue_manager.h message_queue_manager.cpp
  reconnection_manager.h reconnection_manager.cpp
)
#连接Qt库并设置包含目录，使用PUBLIC确保依赖传递
target_link_libraries(network PUBLIC
    protocol
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network)

#设置包含目录
target_include_directories(network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

#设置目标属性确保MOC确认处理
set_target_properties(network PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
#设置编译器特定选项
if(MSVC)
    target_compile_options(network PRIVATE /W4 /permissive-)
else()
    target_compile_options(network PRIVATE -Wall -Wextra -pedantic)
endif()
#添加Qt模块定义
target_compile_definitions(network PUBLIC QT_NETWORK_LIB)
