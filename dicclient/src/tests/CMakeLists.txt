cmake_minimum_required(VERSION 3.16)

# ============================================================================
# 客户端单元测试（GoogleTest）
#
# 设计要点：
#  1. 不再硬编码 vcpkg 绝对路径，改为多来源探测 + 环境变量 VCPKG_ROOT 兜底，
#     保证在 CI（GitHub Actions）与本地都能找到 gtest。
#  2. 每个测试文件编译为独立可执行文件，避免一个文件的符号冲突拖垮全部用例。
#  3. 链接主工程的静态库（network/protocol/ui/dispatcher 等），
#     测试直接覆盖真实业务代码而不是复制一份。
#  4. 通过 add_test() 注册到 CTest，ctest --output-on-failure 可一次跑完。
#  5. 不依赖 page_manager：该模块未编入主工程（导航实际由 PageController +
#     QStackedWidget 实现），避免链接失败。
# ============================================================================

# ---------- 定位 vcpkg / gtest ----------
if(NOT DEFINED VCPKG_ROOT OR VCPKG_ROOT STREQUAL "")
    # 依次尝试：环境变量 -> 常见安装位置
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/vcpkg")
    elseif(EXISTS "C:/Users/86152/Desktop/gettest/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "C:/Users/86152/Desktop/gettest/vcpkg")
    endif()
endif()

if(VCPKG_ROOT AND EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    # 工具链文件必须在 project() 之前设置
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file" FORCE)
endif()

project(client_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------- 查找 Qt ----------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Test Widgets Network TextToSpeech)

# ---------- 定位 gtest ----------
set(GTEST_INSTALLED_DIR "${VCPKG_ROOT}/installed/x64-windows")
set(GTEST_INCLUDE_DIR "${GTEST_INSTALLED_DIR}/include")
set(GTEST_LIB_DIR "${GTEST_INSTALLED_DIR}/lib")
set(GTEST_MAIN_LIB_DIR "${GTEST_LIB_DIR}/manual-link")
set(GTEST_LIBRARY "${GTEST_LIB_DIR}/gtest.lib")
set(GTEST_MAIN_LIBRARY "${GTEST_MAIN_LIB_DIR}/gtest_main.lib")
set(GMOCK_LIBRARY "${GTEST_LIB_DIR}/gmock.lib")

if(NOT EXISTS "${GTEST_LIBRARY}")
    # 退回到 find_package，让 vcpkg 的 CONFIG 模式接管
    find_package(GTest CONFIG REQUIRED)
    set(GTEST_LIBRARY GTest::gtest)
    set(GTEST_MAIN_LIBRARY GTest::gtest_main)
    set(GMOCK_LIBRARY GTest::gmock)
else()
    message(STATUS "Using gtest from vcpkg: ${GTEST_LIBRARY}")
endif()

# ============================================================================
# Windows 运行时 DLL 部署
#
# CTest 子进程不会继承 IDE 的 PATH，而本项目在 Windows 上的依赖
# （Qt6、vcpkg 的 gtest/gmock/xlnt 等）全部是 DLL。这里在构建后把
# 这些 DLL 复制到测试可执行文件所在目录，保证：
#   - `ctest` 直接可跑
#   - 手动双击测试 exe 也能跑
#   - CI 上同样成立，无需额外配置 PATH
# ============================================================================

# 候选 DLL 来源目录：项目自带 vcpkg、外部 vcpkg、Qt 安装目录
set(TEST_DLL_SOURCE_DIRS "")
foreach(candidate
    "${CMAKE_SOURCE_DIR}/src/vcpkg/installed/x64-windows/bin"
    "${VCPKG_ROOT}/installed/x64-windows/bin")
    if(EXISTS "${candidate}")
        list(APPEND TEST_DLL_SOURCE_DIRS "${candidate}")
    endif()
endforeach()

# 从 Qt6::Core 的导入位置反推 Qt 安装根目录，得到 plugins/platforms
get_target_property(_qt_core_location Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION)
set(TEST_QT_PLUGINS_DIR "")
if(_qt_core_location)
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    get_filename_component(_qt_root_dir "${_qt_bin_dir}" DIRECTORY)
    if(EXISTS "${_qt_root_dir}/plugins/platforms")
        set(TEST_QT_PLUGINS_DIR "${_qt_root_dir}/plugins/platforms")
    endif()
endif()

enable_testing()

# ---------- 头文件搜索路径 ----------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
if(EXISTS "${GTEST_INCLUDE_DIR}")
    include_directories(${GTEST_INCLUDE_DIR})
endif()

# ---------- 链接到主工程的静态库 ----------
# 这些库由父工程的 add_subdirectory 定义，测试直接复用真实实现
set(CLIENT_LIBS
    network
    protocol
    ui
    dispatcher
    user_session
    word_function
)

# 只保留父工程中确实定义了目标的库
set(AVAILABLE_LIBS "")
foreach(lib ${CLIENT_LIBS})
    if(TARGET ${lib})
        list(APPEND AVAILABLE_LIBS ${lib})
    else()
        message(STATUS "跳过未定义的库目标: ${lib}")
    endif()
endforeach()

# ============================================================================
# 一次性部署共享运行时依赖
#
# vcpkg 的 DLL（gtest/gmock/xlnt/fmt 等）与 Qt 平台插件对所有测试目标是
# 共享的。如果放在每个目标的 POST_BUILD 里，Ninja 并行构建时会多个进程
# 同时读写同一个 platforms 目录导致失败，因此收敛为单个 custom target，
# 各测试目标通过 add_dependencies 依赖它。
# ============================================================================
set(TEST_RUNTIME_OUT_DIR "${CMAKE_BINARY_DIR}/bin/tests")

if(NOT TARGET deploy_test_runtime)
    add_custom_target(deploy_test_runtime
        COMMENT "部署测试运行时依赖（DLL + Qt 平台插件）")

    # 1) vcpkg 目录下的 DLL
    foreach(dll_dir ${TEST_DLL_SOURCE_DIRS})
        file(GLOB _vcpkg_dlls "${dll_dir}/*.dll")
        foreach(dll_file ${_vcpkg_dlls})
            add_custom_command(TARGET deploy_test_runtime POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll_file}" "${TEST_RUNTIME_OUT_DIR}"
                VERBATIM)
        endforeach()
    endforeach()

    # 2) Qt 平台插件（GUI 测试必须有，否则 Qt 无法创建 QApplication）
    #    copy_directory 在目标已存在时报错，故先清理再复制，保证幂等。
    if(TEST_QT_PLUGINS_DIR)
        add_custom_command(TARGET deploy_test_runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${TEST_RUNTIME_OUT_DIR}/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${TEST_QT_PLUGINS_DIR}" "${TEST_RUNTIME_OUT_DIR}/platforms"
            VERBATIM)
    endif()
endif()

# ---------- 测试源文件清单 ----------
# 每个条目生成一个独立可执行文件与一条 CTest 用例
set(TEST_SOURCES
    tst_page_controller
    tst_network
    tst_messagetest
    tst_core_messagetest
    tst_core_app_configtest
    tst_core_utilstest
    tst_network_network_managetest
    tst_network_network_manager_nextconnecttest
    tst_dispatcher
    tst_user_session
    tst_composite_module
    tst_login_page
    tst_register_page
    tst_system_sidebar
    tst_ui_word_card_test
    tst_ui_interactive_wordcard
    tst_ui_search_base
    tst_ui_searchhistory_widget
)

foreach(test_name ${TEST_SOURCES})
    set(test_src "${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.cpp")
    if(NOT EXISTS "${test_src}")
        message(WARNING "跳过不存在的测试文件: ${test_name}.cpp")
        continue()
    endif()

    set(target_name "test_${test_name}")
    add_executable(${target_name} "${test_src}")

    target_link_libraries(${target_name} PRIVATE
        ${AVAILABLE_LIBS}
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Network
        "${GTEST_LIBRARY}"
        "${GTEST_MAIN_LIBRARY}"
    )
    # gmock 仅在被引用时才链接，避免无用依赖
    if(TARGET ${GMOCK_LIBRARY} OR EXISTS "${GMOCK_LIBRARY}")
        target_link_libraries(${target_name} PRIVATE "${GMOCK_LIBRARY}")
    endif()

    # 放到统一输出目录
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
    )

    # ---- Windows DLL 部署 ----
    # 1) 共享依赖（vcpkg DLL + Qt 平台插件）由 deploy_test_runtime 统一部署
    add_dependencies(${target_name} deploy_test_runtime)

    # 2) Qt 导入目标的运行时 DLL：TARGET_RUNTIME_DLLS 自动展开为文件列表
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                "$<TARGET_FILE_DIR:${target_name}>"
        COMMAND_EXPAND_LISTS
        VERBATIM)

    # 注册到 CTest：超时通过 set_tests_properties 设置
    add_test(NAME ${test_name}
             COMMAND ${target_name}
             WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests")
    set_tests_properties(${test_name} PROPERTIES TIMEOUT 60)
endforeach()

# ---------- 测试汇总输出目录 ----------
list(LENGTH TEST_SOURCES _total)
message(STATUS "已注册 ${_total} 个客户端测试用例")
