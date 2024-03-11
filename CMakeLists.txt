cmake_minimum_required(VERSION 3.23.5)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

project(
  allocators
  VERSION 0.0.1
  LANGUAGES CXX)

include(GNUInstallDirs)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(
  ${PROJECT_NAME}
  INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# Copy include files to the install include directory.
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/${PROJECT_NAME}/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

# Import dep on named-template-parameters
include(FetchContent)

FetchContent_Declare(
  named-template-parameters
  GIT_REPOSITORY https://github.com/yaneury/named-template-parameters.git
  GIT_TAG main)

FetchContent_MakeAvailable(named-template-parameters)

target_link_libraries(${PROJECT_NAME} INTERFACE ntp)

FetchContent_Declare(
  Result
  GIT_REPOSITORY https://github.com/bitwizeshift/result.git
  GIT_TAG master)

FetchContent_MakeAvailable(Result)

target_link_libraries(${PROJECT_NAME} INTERFACE Result)

FetchContent_Declare(
  magic_enum
  GIT_REPOSITORY https://github.com/Neargye/magic_enum.git
  GIT_TAG v0.8.2)

FetchContent_MakeAvailable(magic_enum)

target_link_libraries(${PROJECT_NAME} INTERFACE magic_enum)

option(ALLOCATORS_BUILD_TESTS "Set to ON to build tests" OFF)
option(ALLOCATORS_BUILD_SANDBOX "Set to ON to build sandbox" OFF)
option(ALLOCATORS_DEBUG "Set to ON to enable debug messages" OFF)

if(ALLOCATORS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ALLOCATORS_BUILD_SANDBOX)
  add_subdirectory(sandbox)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${PROJECT_NAME} INTERFACE -DDEBUG)

  FetchContent_Declare(
    plog
    GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
    GIT_TAG master)

  FetchContent_MakeAvailable(plog)

  target_link_libraries(${PROJECT_NAME} INTERFACE plog)
endif()
