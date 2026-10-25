cmake_minimum_required(VERSION 3.16)

project(ui LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型（如果未设置）
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 如果ui需要Widgets模块，添加它
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets TextToSpeech)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets TextToSpeech)



# 修改为静态库
add_library(ui STATIC
  base/base_widget.h
  base/base_widget.cpp
  base/connection_overlay.h
  base/connection_overlay.cpp
   base/theme_manager.h
   base/theme_manager.cpp
   icons.qrc
  word/word_card.h
  word/word_card.cpp
  word/interactive_wordcard.h
  word/interactive_wordcard.cpp
  search/search_base.h
  search/search_base.cpp
  search/searchhistory_widget.h
  search/searchhistory_widget.cpp
  system/system_sidebar.cpp
  system/system_sidebar.h
  view/login_page.cpp
  view/login_page.h
  view/register_page.h
  view/register_page.cpp
  view/page_controller.h
  view/page_controller.cpp
  view/main_page.h
  view/main_page.cpp
  view/collecte_page.h
  view/collecte_page.cpp
  view/vocabulary_page.h
  view/vocabulary_page.cpp
)

# 链接依赖库
target_link_libraries(ui PUBLIC
    # 如果依赖其他内部库
    network
    protocol
    dispatcher
    user_session
    word_function
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
     Qt${QT_VERSION_MAJOR}::TextToSpeech
)



# 设置包含目录
target_include_directories(ui PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 设置目标属性
set_target_properties(ui PROPERTIES
    AUTOMOC ON
    AUTOUIC ON
    AUTORCC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 设置编译器选项
if(MSVC)
    target_compile_options(ui PRIVATE /W4 /permissive-)
else()
    target_compile_options(ui PRIVATE -Wall -Wextra -pedantic)
endif()

# 添加Qt模块定义
target_compile_definitions(ui PUBLIC
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_CORE_LIB>
    $<$<BOOL:${QT_VERSION_MAJOR}>:QT_WIDGETS_LIB>
)
