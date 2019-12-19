cmake_minimum_required(VERSION 3.15)
project(LearnFFmpeg)

set(CMAKE_CXX_STANDARD 11)

include_directories(${PROJECT_SOURCE_DIR}/include/)

#link_directories(./lib/)

add_executable(LearnFFmpeg code/encode_video.c)

target_link_libraries(
        LearnFFmpeg
        ${PROJECT_SOURCE_DIR}/lib/libavformat.so.58
        ${PROJECT_SOURCE_DIR}/lib/libavdevice.so.58
        ${PROJECT_SOURCE_DIR}/lib/libavcodec.so.58
        ${PROJECT_SOURCE_DIR}/lib/libavutil.so.56
        ${PROJECT_SOURCE_DIR}/lib/libswresample.so.3
        ${PROJECT_SOURCE_DIR}/lib/libavfilter.so.7
        ${PROJECT_SOURCE_DIR}/lib/libpostproc.so.55
        ${PROJECT_SOURCE_DIR}/lib/libswscale.so.5
        ${PROJECT_SOURCE_DIR}/lib/libx264.so.157)
#Windows
#target_link_libraries(
#        LearnFFmpeg
#        ${PROJECT_SOURCE_DIR}/lib/avcodec.lib
#        ${PROJECT_SOURCE_DIR}/lib/avdevice.lib
#        ${PROJECT_SOURCE_DIR}/lib/avfilter.lib
#        ${PROJECT_SOURCE_DIR}/lib/avformat.lib
#        ${PROJECT_SOURCE_DIR}/lib/avutil.lib
#        ${PROJECT_SOURCE_DIR}/lib/postproc.lib
#        ${PROJECT_SOURCE_DIR}/lib/swresample.lib
#        ${PROJECT_SOURCE_DIR}/lib/swscale.lib)
