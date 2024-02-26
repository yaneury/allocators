cmake_minimum_required(VERSION 3.15)

project(dmt-sandbox LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
add_executable(sandbox ${SOURCES})

# Link to dmt library
target_link_libraries(sandbox PRIVATE dmt)

if(DMT_DEBUG)
    add_definitions(-DDEBUG)
endif()
