cmake_minimum_required(VERSION 3.14)
project(union_find_viz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch raylib
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(union_find_viz
    main.cpp
    union_find.cpp
    dem_parser.cpp
    decoder_stepper.cpp
    renderer.cpp
    ui.cpp
)

target_include_directories(union_find_viz PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(union_find_viz PRIVATE raylib)

if(APPLE)
    target_link_libraries(union_find_viz PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
    )
endif()
