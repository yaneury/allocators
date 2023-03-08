cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)

project(
  dmt
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

# Enable incomplete features for Ranges support in clang@15
add_definitions(-DLIBCXX_ENABLE_INCOMPLETE_FEATURES=ON)

option(DMT_BUILD_TESTS "Set to ON to build tests" OFF)
option(DMT_DEBUG "Set to ON to enable debug messages" OFF)

if(DMT_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(DMT_DEBUG)
  # Enable logging if building with -D DMT_DEBUG
  target_compile_definitions(dmt INTERFACE -DDMT_DEBUG)

  FetchContent_Declare(
    plog
    GIT_REPOSITORY https://github.com/SergiusTheBest/plog.git
    GIT_TAG master)

  FetchContent_MakeAvailable(plog)

  target_link_libraries(${PROJECT_NAME} INTERFACE plog)

endif()
