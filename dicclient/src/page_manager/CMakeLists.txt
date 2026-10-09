cmake_minimum_required(VERSION 3.16)

project(page_manager LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 如果page_manager需要Widgets模块，添加它
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

add_library(page_manager STATIC

  page_manager.cpp
  page_manager.h
  page_types.h
  page_stack.h page_stack.cpp
  page_factory.h page_factory.cpp
)

# 链接依赖库
target_link_libraries(page_manager PUBLIC
    # 如果依赖其他内部库
    network
    protocol
    ui
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)


# 设置包含目录
target_include_directories(page_manager PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
set_target_properties(page_manager PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置编译器选项
if(MSVC)
    target_compile_options(page_manager PRIVATE /W4 /permissive-)
else()
    target_compile_options(page_manager PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加Qt模块定义
target_compile_definitions(page_manager PUBLIC
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_CORE_LIB>
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_WIDGETS_LIB>
)
