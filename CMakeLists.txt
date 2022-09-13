cmake_minimum_required(VERSION 3.23)
project(vpilot)

set(CMAKE_CXX_STANDARD 23)

find_package(OpenGL REQUIRED)

add_executable(vpilot main.cpp)

set(RAYLIB ${PROJECT_SOURCE_DIR}/raylib)
target_include_directories(vpilot PRIVATE ${RAYLIB}/include)
target_link_libraries(vpilot PRIVATE ${RAYLIB}/lib/libraylib.a)
target_link_libraries(vpilot PRIVATE OpenGL::OpenGL)