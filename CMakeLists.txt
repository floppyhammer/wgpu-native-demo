cmake_minimum_required(VERSION 3.28)
project(wgpu_native_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

add_executable(wgpu_native_demo src/main.cpp)

if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(WGPU_DIR ${CMAKE_SOURCE_DIR}/third_party/wgpu)
include_directories(${WGPU_DIR})
target_link_directories(wgpu_native_demo PRIVATE ${WGPU_DIR})
set(WGPU_LIBRARY wgpu_native)

include_directories(${CMAKE_SOURCE_DIR}/third_party/glfw/include)
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)

if (WIN32)
    add_definitions(-DWGPU_TARGET_WINDOWS)
    set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DWGPU_TARGET_LINUX_X11)
    set(OS_LIBRARIES "-lm -ldl")
elseif(APPLE)
    add_definitions(-DWGPU_TARGET_MACOS)
    set(OS_LIBRARIES "-framework CoreFoundation -framework QuartzCore -framework Metal")
endif()

target_link_libraries(wgpu_native_demo glfw ${WGPU_LIBRARY} ${OS_LIBRARIES})
