
cmake_minimum_required(VERSION 3.10)
project(MyVideoPlayerPOC LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0)

add_executable(MyVideoPlayerPOC main.cpp)
target_include_directories(MyVideoPlayerPOC PRIVATE ${GST_INCLUDE_DIRS})
target_link_libraries(MyVideoPlayerPOC PRIVATE ${GST_LIBRARIES})
target_compile_options(MyVideoPlayerPOC PRIVATE ${GST_CFLAGS_OTHER})
