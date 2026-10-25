cmake_minimum_required(VERSION 3.16)

project(network_dictionary_copy VERSION 1.0.0 LANGUAGES CXX)

# 在最开始设置编码相关选项
if(MSVC)
    # 设置源码和执行字符集都为UTF-8
    # 注意：必须用 COMPILE_LANGUAGE 覆盖 C 与 CXX。
    # 原先写作 $<C_COMPILER_ID:MSVC> 只对 C 源文件生效，C++ 源文件
    # （本项目绝大部分代码）实际拿不到 /utf-8，导致 UTF-8 中文字符串
    # 被按系统代码页（GBK）误读而乱码。
    add_compile_options("$<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>")
    # 确保Windows API使用Unicode版本
    add_definitions(-DUNICODE -D_UNICODE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt6组件 - 修改：添加Core模块
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Core)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含路径
include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(src/protocol)
add_subdirectory(src/network)
add_subdirectory(src/ui)
# page_manager 模块未编入主工程：界面导航实际由 PageController + QStackedWidget
# 实现，该模块属于历史遗留的自研页面栈方案，保留源码但不再参与构建。
#add_subdirectory(src/page_manager)
add_subdirectory(src/user_session)
add_subdirectory(src/dispatcher)
add_subdirectory(src/word_function)
add_subdirectory(src/main_app)


set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT main_app)

# ============================================================================
# 单元测试
# 每个测试文件编译为独立可执行文件并注册到 CTest，
# 可通过 ctest --output-on-failure 一次跑完全部用例。
# 默认关闭，避免影响正常构建；CI 中显式传 -DBUILD_TESTING=ON 打开。
# ============================================================================
option(BUILD_TESTING "Build unit tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(src/tests)
endif()

if(CMAKE_GENERATOR STREQUAL "Ninja")
    add_custom_target(run_main_app
        COMMAND ${CMAKE_COMMAND} -E echo "Starting main_app..."
        DEPENDS main_app
        COMMENT "Run main_app target"
    )
endif()


