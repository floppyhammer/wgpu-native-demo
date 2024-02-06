#ifndef COMMON_H
#define COMMON_H

#ifdef EMSCRIPTEN
    #include <webgpu/webgpu.h>
#else
    #include "webgpu.h"
    #include "wgpu.h"
#endif

WGPUShaderModule load_shader_module(WGPUDevice device, const char* name);

#endif // COMMON_H
