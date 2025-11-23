cmake_minimum_required(VERSION 3.16)
project(multi_cam_stream LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 공통 include 경로
include_directories(${CMAKE_SOURCE_DIR}/common)

add_subdirectory(server/cam_grabber)
add_subdirectory(client/qt_viewer)