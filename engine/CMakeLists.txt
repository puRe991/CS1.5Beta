cmake_minimum_required(VERSION 3.16)
project(cs15engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(cs15engine
    src/main.cpp
    src/camera.cpp
    src/assets/pak.cpp
    src/assets/wad.cpp
)

target_include_directories(cs15engine PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(cs15engine PRIVATE ${SDL2_LIBRARIES} OpenGL::GL)
