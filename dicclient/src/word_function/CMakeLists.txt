
cmake_minimum_required(VERSION 3.16)

project(word_function LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ========== vcpkg 工具链自动检测 ==========
set(VCPKG_ROOT "")

if(EXISTS "${CMAKE_SOURCE_DIR}/src/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/src/vcpkg")
elseif(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
endif()

if(VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")

    #显式添加 vcpkg installed 路径，确保 find_package 能找到
    list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/x64-windows/share")

    message(STATUS "Using vcpkg: ${VCPKG_ROOT}")
else()
    message(FATAL_ERROR "vcpkg not found. Run: git submodule update --init")
endif()
# =========================================

# 查找 xlnt（vcpkg 会自动处理）
find_package(xlnt CONFIG REQUIRED)

# 如果需要Widgets模块，添加它
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets TextToSpeech PrintSupport)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets TextToSpeech PrintSupport)


add_library(word_function STATIC
    word_data.h
    word_search.h word_search.cpp
    word_collect.h word_collect.cpp
    word_vocabulary.h word_vocabulary.cpp
    export_manager.h export_manager.cpp
)

# 链接依赖库
target_link_libraries(word_function PUBLIC
    # 如果依赖其他内部库
    network
    protocol
    ui
    dispatcher
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::TextToSpeech
    xlnt::xlnt
    Qt${QT_VERSION_MAJOR}::PrintSupport
)

# 设置包含目录
target_include_directories(word_function PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
set_target_properties(word_function PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置编译器选项
if(MSVC)
    target_compile_options(word_function PRIVATE /W4 /permissive-)
else()
    target_compile_options(word_function PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加Qt模块定义
target_compile_definitions(word_function PUBLIC
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_CORE_LIB>
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_WIDGETS_LIB>
)
