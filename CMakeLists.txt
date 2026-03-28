cmake_minimum_required(VERSION 3.16)

project(alpha_project VERSION 0.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport Test)
include(CTest)

file(GLOB PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

file(GLOB PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(NOT PROJECT_HEADERS AND NOT PROJECT_SOURCES)
    message(FATAL_ERROR "No source files were found in include/ or src/")
endif()

add_library(alpha_project_lib OBJECT
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
    resources/resources.qrc
    ui/homescreen.ui
    ui/notes.ui
    ui/noteseditor.ui
    ui/schedule.ui
    ui/settings.ui
)

target_include_directories(alpha_project_lib
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(alpha_project_lib
    PUBLIC
        Qt6::Widgets
        Qt6::PrintSupport
)

if(APPLE)
    target_compile_definitions(alpha_project_lib PUBLIC PLATFORM_MACOS=1)
elseif(WIN32)
    target_compile_definitions(alpha_project_lib PUBLIC PLATFORM_WINDOWS=1)
elseif(UNIX)
    target_compile_definitions(alpha_project_lib PUBLIC PLATFORM_LINUX=1)
endif()

if(MSVC)
    target_compile_options(alpha_project_lib PRIVATE /W4)
else()
    target_compile_options(alpha_project_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(APP_MAIN
    "${CMAKE_CURRENT_SOURCE_DIR}/app/main.cpp"
)

set(APP_ICON_PNG
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/kmn_app_icon.png"
)

set(APP_HOME_LOGO_PNG
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/kmn_logo.PNG"
)

set(APP_ICON_ICNS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/kmn_logo.icns"
)

if(EXISTS "${APP_MAIN}")
    add_executable(alpha_project_app
        "${APP_MAIN}"
        "${APP_ICON_PNG}"
        $<TARGET_OBJECTS:alpha_project_lib>
    )
    target_link_libraries(alpha_project_app
        PRIVATE
            Qt6::Widgets
            Qt6::PrintSupport
    )
    set_target_properties(alpha_project_app PROPERTIES OUTPUT_NAME "KMN Space")

    if(APPLE)
        set_source_files_properties("${APP_ICON_PNG}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        set_source_files_properties("${APP_HOME_LOGO_PNG}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        target_sources(alpha_project_app PRIVATE "${APP_HOME_LOGO_PNG}")
        if(EXISTS "${APP_ICON_ICNS}")
            target_sources(alpha_project_app PRIVATE "${APP_ICON_ICNS}")
            set_source_files_properties("${APP_ICON_ICNS}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
        endif()
        set_target_properties(alpha_project_app PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "KMN Space"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.kmnspace.app"
            MACOSX_BUNDLE_ICON_FILE "kmn_app_icon.png"
        )
    endif()
else()
    message(STATUS "No app/main.cpp found. The library target 'alpha_project_lib' will be generated for now.")
endif()

if(BUILD_TESTING)
    add_executable(alpha_project_smoke_tests
        tests/basic_smoke_tests.cc
        $<TARGET_OBJECTS:alpha_project_lib>
    )
    target_link_libraries(alpha_project_smoke_tests
        PRIVATE
            Qt6::Widgets
            Qt6::PrintSupport
            Qt6::Test
    )
    add_test(NAME alpha_project_smoke_tests COMMAND alpha_project_smoke_tests)
    set_tests_properties(alpha_project_smoke_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
