project(sandbox LANGUAGES CXX)

file(GLOB SOURCES *.cpp)
add_executable(${PROJECT_NAME} ${SOURCES})

# Link to cppalloc library
target_link_libraries(${PROJECT_NAME} PRIVATE cppalloc)
