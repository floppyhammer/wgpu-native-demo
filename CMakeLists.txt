cmake_minimum_required(VERSION 3.22)
project(wgpu_native_demo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

if (EMSCRIPTEN)
    add_executable(wgpu_native_demo src/common.cpp src/web/main.cpp)
else ()
    add_executable(wgpu_native_demo src/common.cpp src/native/main.cpp)
endif ()

if (MSVC)
    add_compile_options(/W4)
else ()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

if (EMSCRIPTEN)
    # Add Emscripten-specific link options.
    target_link_options(wgpu_native_demo PRIVATE
            -sUSE_GLFW=3 # Use Emscripten-provided GLFW
            -sUSE_WEBGPU # Handle WebGPU symbols
            -sASYNCIFY # Required by WebGPU-C++
            --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            -sALLOW_MEMORY_GROWTH
    )

    # Generate a full web page rather than a simple WebAssembly module.
    set_target_properties(wgpu_native_demo PROPERTIES SUFFIX ".html")

    # For debugging. Enable this to obtain detailed call stack.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -gsource-map -s ASSERTIONS=2 -s SAFE_HEAP=1 -s STACK_OVERFLOW_CHECK=1 --source-map-base http://0.0.0.0:8000/")
else ()
    set(WGPU_DIR ${CMAKE_SOURCE_DIR}/third_party/wgpu)
    include_directories(${WGPU_DIR})
    target_link_directories(wgpu_native_demo PRIVATE ${WGPU_DIR})
    set(WGPU_LIBRARY wgpu_native)

    include_directories(${CMAKE_SOURCE_DIR}/third_party/glfw/include)
    # Do not include this with emscripten, it provides its own version.
    add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw)

    if (WIN32)
        add_definitions(-DWGPU_TARGET_WINDOWS)
        set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32)
    elseif (UNIX AND NOT APPLE)
        add_definitions(-DWGPU_TARGET_LINUX_X11)
        set(OS_LIBRARIES "-lm -ldl")
    elseif (APPLE)
        add_definitions(-DWGPU_TARGET_MACOS)
        set(OS_LIBRARIES "-framework CoreFoundation -framework QuartzCore -framework Metal")
    endif ()

    target_link_libraries(wgpu_native_demo glfw ${WGPU_LIBRARY} ${OS_LIBRARIES})
endif ()
