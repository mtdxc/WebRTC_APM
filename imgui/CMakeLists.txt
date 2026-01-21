# ImGui demo using SDL3 + OpenGL + WebRTC APM
cmake_minimum_required(VERSION 3.15)

set(SDL3_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../subprojects/SDL3-3.4.0")

find_package(OpenGL REQUIRED)

# Prefer a packaged SDL3, fall back to the bundled source tree.
find_package(SDL3 QUIET CONFIG)
if(NOT TARGET SDL3::SDL3 AND EXISTS "${SDL3_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory("${SDL3_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/sdl3")
endif()

if(TARGET SDL3::SDL3)
    set(SDL3_TARGET SDL3::SDL3)
elseif(TARGET SDL3::SDL3-static)
    set(SDL3_TARGET SDL3::SDL3-static)
elseif(TARGET SDL3::SDL3-shared)
    set(SDL3_TARGET SDL3::SDL3-shared)
else()
    message(FATAL_ERROR "SDL3 target not found; install SDL3 or keep subprojects/SDL3-3.4.0 available.")
endif()

include_directories(imgui_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

add_executable(imgui_demo
    main.cpp
    imgui.cpp
    imgui_demo.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    backends/imgui_impl_opengl3.cpp
    backends/imgui_impl_sdl3.cpp
    IconsFontAwesome6.h
)

target_link_libraries(imgui_demo PRIVATE
    ${SDL3_TARGET}
    OpenGL::GL
    webrtc_apm_wrapper
    absl::optional
)

target_include_directories(imgui_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS imgui_demo
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin
)
