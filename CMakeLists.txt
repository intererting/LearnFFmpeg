cmake_minimum_required(VERSION 3.15)
project(LearnFFmpeg)

set(CMAKE_CXX_STANDARD 11)

include_directories(${PROJECT_SOURCE_DIR}/include/)

add_executable(LearnFFmpeg code/filtering_video.c)

target_link_libraries(
        LearnFFmpeg
        ${PROJECT_SOURCE_DIR}/lib/avcodec.lib
        ${PROJECT_SOURCE_DIR}/lib/avdevice.lib
        ${PROJECT_SOURCE_DIR}/lib/avfilter.lib
        ${PROJECT_SOURCE_DIR}/lib/avformat.lib
        ${PROJECT_SOURCE_DIR}/lib/avutil.lib
        ${PROJECT_SOURCE_DIR}/lib/postproc.lib
        ${PROJECT_SOURCE_DIR}/lib/swresample.lib
        ${PROJECT_SOURCE_DIR}/lib/swscale.lib)