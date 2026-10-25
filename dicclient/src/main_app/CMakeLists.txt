# src/main_app/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

project(main_app LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 查找Qt包 - 添加更多必要的组件
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Gui Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Gui Network)

# 包含路径 - 让主程序能找到头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 创建主程序可执行文件 - 修改：文件名改为main.cpp
add_executable(main_app
    main.cpp

)
target_sources(main_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ../ui/icons.qrc
)
# 链接库 - 确保链接顺序正确
target_link_libraries(main_app PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    protocol
    network
    ui
    user_session
    dispatcher
    word_function
)

# 设置输出目录
set_target_properties(main_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
)

# 设置包含目录
target_include_directories(main_app PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
set_target_properties(main_app PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    WIN32_EXECUTABLE TRUE  # Windows下使用GUI子系统
)

# 设置编译器选项
if(MSVC)
    target_compile_options(main_app PRIVATE /W4 /permissive-)
    # 设置UTF-8编码
    target_compile_options(main_app PRIVATE /utf-8)
    # 确保Windows API使用Unicode版本
    target_compile_definitions(main_app PRIVATE UNICODE _UNICODE)
else()
    target_compile_options(main_app PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加Qt模块定义
target_compile_definitions(main_app PRIVATE
    QT_CORE_LIB
    QT_GUI_LIB
    QT_WIDGETS_LIB
    QT_NETWORK_LIB
)

