cmake_minimum_required(VERSION 3.15)
project(libdmt CXX)

add_library(libdmt src/libdmt.cpp)
target_include_directories(libdmt PUBLIC include)

set_target_properties(libdmt PROPERTIES PUBLIC_HEADER "include/libdmt.h")
install(TARGETS libdmt)
