#include "common.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

WGPUShaderModule load_shader_module(WGPUDevice device, const char* name) {
    FILE* file = nullptr;
    char* buf = nullptr;
    WGPUShaderModule shader_module = nullptr;

    do {
        file = fopen(name, "rb");
        if (!file) {
            perror("fopen");
            break;
        }

        if (fseek(file, 0, SEEK_END) != 0) {
            perror("fseek");
            break;
        }
        long length = ftell(file);
        if (length == -1) {
            perror("ftell");
            break;
        }
        if (fseek(file, 0, SEEK_SET) != 0) {
            perror("fseek");
            break;
        }

        buf = (char*)malloc(length + 1);
        assert(buf);
        fread(buf, 1, length, file);
        buf[length] = 0;

        WGPUShaderModuleWGSLDescriptor shader_module_wgsl_descriptor = {
            .chain =
                WGPUChainedStruct{
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = buf,
        };

        WGPUShaderModuleDescriptor shader_module_descriptor = {
            .nextInChain = (const WGPUChainedStruct*)&shader_module_wgsl_descriptor,
            .label = name,
        };

        shader_module = wgpuDeviceCreateShaderModule(device, &shader_module_descriptor);
    } while (false);

    if (file) {
        fclose(file);
    }
    if (buf) {
        free(buf);
    }

    return shader_module;
}
