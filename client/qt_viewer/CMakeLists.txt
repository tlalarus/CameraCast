# client/qt_viewer/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets)

add_executable(qt_viewer
    main.cpp
    ws_client_viewer.cpp
    ws_client_viewer.h
)

target_link_libraries(qt_viewer
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebSockets
)

target_include_directories(qt_viewer PRIVATE
    ${CMAKE_SOURCE_DIR}/common
)