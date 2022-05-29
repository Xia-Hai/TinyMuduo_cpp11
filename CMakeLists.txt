cmake_minimum_required(VERSION 3.16.1)

project(mymuduo)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#设置调试
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}-std=c++11")

aux_source_directory(. SRC_LIST)

add_library(mymuduo SHARED ${SRC_LIST})