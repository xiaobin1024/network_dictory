cmake_minimum_required(VERSION 3.16)

# 1. 强制加载vcpkg工具链（必须在project前）
set(VCPKG_ROOT "C:/Users/86152/Desktop/gettest/vcpkg")
set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE STRING "Vcpkg toolchain file" FORCE)

# 查找Qt包 - 修改：添加Core模块
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test)

project(MessageTest LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 3. 手动指定GTest的所有路径（包含manual-link里的main库）
set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed/x64-windows")
set(GTEST_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include")
set(GTEST_LIB_DIR "${VCPKG_INSTALLED_DIR}/lib")
set(GTEST_MAIN_LIB_DIR "${VCPKG_INSTALLED_DIR}/lib/manual-link") #main库所在目录

# 4. 手动指定GTest的库文件
set(GTEST_LIBRARY "${GTEST_LIB_DIR}/gtest.lib")
set(GTEST_MAIN_LIBRARY "${GTEST_MAIN_LIB_DIR}/gtest_main.lib") # 明确指向main库

# 5. 验证库文件是否存在（避免路径错误）
if (NOT EXISTS "${GTEST_LIBRARY}")
    message(FATAL_ERROR "找不到gtest.lib：${GTEST_LIBRARY}")
endif()
if (NOT EXISTS "${GTEST_MAIN_LIBRARY}")
    message(FATAL_ERROR "找不到gtest_main.lib：${GTEST_MAIN_LIBRARY}")
endif()

# 6. 引入头文件和库路径
include_directories("${GTEST_INCLUDE_DIR}")
link_directories("${GTEST_LIB_DIR}" "${GTEST_MAIN_LIB_DIR}") # 同时包含lib和manual-link


# 包含路径 - 让测试能找到头文件
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 创建测试可执行文件
add_executable(tests
    #tst_messagetest.cpp
    #tst_core_messagetest.cpp
    #tst_core_app_configtest.cpp
    #tst_core_utilstest.cpp
    #tst_network.cpp
    #tst_network_network_managetest.cpp
    #tst_network_network_manager_nextconnecttest.cpp
    #tst_ui_base_test.cpp
    #tst_ui_word_card_test.cpp
    #tst_ui_interactive_wordcard.cpp
    #tst_ui_search_base.cpp
    #tst_ui_searchhistory_widget.cpp
    #tst_page_stack.cpp
    #tst_page_factory.cpp
    #tst_page_manager.cpp
    #tst_user_session.cpp
    #tst_dispatcher.cpp
    #tst_system_sidebar.cpp
    #tst_composite_module.cpp
    #tst_login_page.cpp
    #tst_register_page.cpp
    tst_page_controller.cpp

    # 后续添加其他测试文件
)


# 7. 直接链接具体的库文件（绕过find_package的组件依赖问题）
target_link_libraries(tests PRIVATE
    network
    protocol
    ui
    page_manager
    dispatcher
    Qt${QT_VERSION_MAJOR}::Core  #添加Qt Core模块链接
    Qt${QT_VERSION_MAJOR}::Test
    "${GTEST_LIBRARY}"
    "${GTEST_MAIN_LIBRARY}" # 手动链接main库
    "${GTEST_LIB_DIR}/gmock.lib" #如果用到gmock
)

# 启用测试
enable_testing()

# 设置输出目录
set_target_properties(tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/tests
)
