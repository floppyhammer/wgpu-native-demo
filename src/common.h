#ifndef COMMON_H
#define COMMON_H

#ifdef EMSCRIPTEN
    #include <webgpu/webgpu.h>
#else
    #include "webgpu.h"
    #include "wgpu.h"
#endif

WGPUShaderModule load_shader_module(WGPUDevice device, const char* name);

WGPUShaderModule create_shader(WGPUDevice device, const char* code, const char* label);

WGPUBuffer create_buffer(WGPUDevice device,
                         WGPUQueue queue,
                         size_t size,
                         WGPUBufferUsage usage,
                         const void* data = nullptr);

#endif // COMMON_H
