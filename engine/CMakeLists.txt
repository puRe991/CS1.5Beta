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
    src/assets/bsp.cpp
    src/assets/mdl.cpp
)

target_include_directories(cs15engine PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(cs15engine PRIVATE ${SDL2_LIBRARIES} OpenGL::GL)

# Standalone debug tool: renders one frame of a BSP map to a BMP file.
add_executable(mapshot
    src/tools/mapshot.cpp
    src/assets/bsp.cpp
    src/assets/wad.cpp
)
target_include_directories(mapshot PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(mapshot PRIVATE ${SDL2_LIBRARIES} OpenGL::GL)

# Standalone debug tool: renders one frame of an MDL character model to a BMP file.
add_executable(modelshot
    src/tools/modelshot.cpp
    src/assets/mdl.cpp
)
target_include_directories(modelshot PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(modelshot PRIVATE ${SDL2_LIBRARIES} OpenGL::GL)

# Debug tool: loads a list of .mdl files and reports parse pass/fail per file.
add_executable(mdlbatchtest
    src/tools/mdlbatchtest.cpp
    src/assets/mdl.cpp
)
target_include_directories(mdlbatchtest PRIVATE src)

# CS:GO-inspired main menu: Play/Watch/Inventory/Store, fictive-currency
# case-opening economy.
add_executable(csmenu
    src/menu_main.cpp
    src/ui/ui.cpp
    src/inventory.cpp
    src/assets/mdl.cpp
)
target_include_directories(csmenu PRIVATE ${SDL2_INCLUDE_DIRS} src)
target_link_libraries(csmenu PRIVATE ${SDL2_LIBRARIES} OpenGL::GL)
