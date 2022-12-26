cmake_minimum_required(VERSION 3.18)
project(final)

set(CMAKE_BUILD_TYPE debug)
set(CMAKE_CXX_STANDARD 20)

include_directories(.)

add_executable(indoor main.cpp)
target_link_libraries(indoor deps)

add_library(deps INTERFACE)

find_package(glfw3 3.3 REQUIRED)
target_link_libraries(deps INTERFACE glfw)

find_package(OpenGL REQUIRED)
target_link_libraries(deps INTERFACE OpenGL::GL)

target_link_libraries(deps INTERFACE dl)

include(FindPkgConfig)
pkg_check_modules(GLEW REQUIRED glew)
target_link_libraries(deps INTERFACE ${GLEW_LIBRARIES})

pkg_check_modules(ASSIMP REQUIRED assimp)
target_link_libraries(deps INTERFACE ${ASSIMP_LIBRARIES})

set(IMGUI_DIR imgui)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_sources(deps
        INTERFACE
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        )
