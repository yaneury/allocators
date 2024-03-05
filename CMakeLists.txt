cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CXX_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

# gcc does not compile double-word atomic operations into the appropriate
# assembly instruction by default. This flag is added to support that operation.
# So far, the Registry struct used by the Page allocator depends on the
# availability of double-word atomic instructions. Though more classes may end
# up needing it as well. Relatedly, we need to link against libatomic to ensure
# we get the implementation of these atomic operations. Reference:
# https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html#index-mcx16
add_compile_options(-mcx16)
set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")

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

# Enable incomplete features for Ranges support in clang@15
add_definitions(-DLIBCXX_ENABLE_INCOMPLETE_FEATURES=ON)

option(DMT_BUILD_TESTS "Set to ON to build tests" OFF)
option(DMT_BUILD_SANDBOX "Set to ON to build sandbox" OFF)
option(DMT_DEBUG "Set to ON to enable debug messages" OFF)

if(DMT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(DMT_BUILD_SANDBOX)
  add_subdirectory(sandbox)
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
