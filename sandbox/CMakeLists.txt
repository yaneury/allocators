project(sandbox LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

# Link to allocators library
target_link_libraries(${PROJECT_NAME} PRIVATE allocators)
