#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "webgpu.h"
#include "wgpu.h"

#if defined(WGPU_TARGET_MACOS)
    #include <Foundation/Foundation.h>
    #include <QuartzCore/CAMetalLayer.h>
#endif

#include <GLFW/glfw3.h>
#if defined(WGPU_TARGET_MACOS)
    #define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(WGPU_TARGET_LINUX_X11)
    #define GLFW_EXPOSE_NATIVE_X11
#elif defined(WGPU_TARGET_LINUX_WAYLAND)
    #define GLFW_EXPOSE_NATIVE_WAYLAND
#elif defined(WGPU_TARGET_WINDOWS)
    #define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

#include <array>

#define LOG_PREFIX "[WGPU]"

struct RenderContext {
    WGPUInstance instance;
    WGPUSurface surface;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurfaceConfiguration config;
};

static void handle_request_adapter(WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter,
                                   char const* message,
                                   void* userdata) {
    if (status == WGPURequestAdapterStatus_Success) {
        auto context = (RenderContext*)userdata;
        context->adapter = adapter;
    } else {
        printf(LOG_PREFIX " request_adapter status=%#.8x message=%s\n", status, message);
    }
}

static void handle_request_device(WGPURequestDeviceStatus status,
                                  WGPUDevice device,
                                  char const* message,
                                  void* userdata) {
    if (status == WGPURequestDeviceStatus_Success) {
        auto context = (RenderContext*)userdata;
        context->device = device;
    } else {
        printf(LOG_PREFIX " request_device status=%#.8x message=%s\n", status, message);
    }
}

static void handle_glfw_key(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
        auto context = (RenderContext*)glfwGetWindowUserPointer(window);
        if (!context || !context->instance) {
            return;
        }

        WGPUGlobalReport report;
        wgpuGenerateReport(context->instance, &report);

        // Add breakpoint here to check out report.
        int _{};
    }
}

static void handle_glfw_framebuffer_size(GLFWwindow* window, int width, int height) {
    if (width == 0 && height == 0) {
        return;
    }

    auto context = (RenderContext*)glfwGetWindowUserPointer(window);
    if (!context) {
        return;
    }

    context->config.width = width;
    context->config.height = height;

    wgpuSurfaceConfigure(context->surface, &context->config);
}

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

int main(int argc, char* argv[]) {
#if defined(WGPU_TARGET_LINUX_WAYLAND)
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif
    assert(glfwInit());

    RenderContext context = {};
    context.instance = wgpuCreateInstance(nullptr);
    assert(context.instance);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "wgpu-native + glfw", nullptr, nullptr);
    assert(window);

    glfwSetWindowUserPointer(window, &context);
    glfwSetKeyCallback(window, handle_glfw_key);
    glfwSetFramebufferSizeCallback(window, handle_glfw_framebuffer_size);

#if defined(WGPU_TARGET_MACOS)
    {
        id metal_layer = nullptr;
        NSWindow* ns_window = glfwGetCocoaWindow(window);
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];
        context.surface =
            wgpuInstanceCreateSurface(context.instance,
                                      &(const WGPUSurfaceDescriptor){
                                          .nextInChain =
                                              (const WGPUChainedStruct*)&(const WGPUSurfaceDescriptorFromMetalLayer){
                                                  .chain =
                                                      (const WGPUChainedStruct){
                                                          .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
                                                      },
                                                  .layer = metal_layer,
                                              },
                                      });
        assert(context.surface);
    }
#elif defined(WGPU_TARGET_LINUX_X11)
    {
        Display* x11_display = glfwGetX11Display();
        Window x11_window = glfwGetX11Window(window);
        context.surface =
            wgpuInstanceCreateSurface(context.instance,
                                      &(const WGPUSurfaceDescriptor){
                                          .nextInChain =
                                              (const WGPUChainedStruct*)&(const WGPUSurfaceDescriptorFromXlibWindow){
                                                  .chain =
                                                      (const WGPUChainedStruct){
                                                          .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
                                                      },
                                                  .display = x11_display,
                                                  .window = x11_window,
                                              },
                                      });
        assert(context.surface);
    }
#elif defined(WGPU_TARGET_LINUX_WAYLAND)
    {
        struct wl_display* wayland_display = glfwGetWaylandDisplay();
        struct wl_surface* wayland_surface = glfwGetWaylandWindow(window);
        context.surface = wgpuInstanceCreateSurface(
            context.instance,
            &(const WGPUSurfaceDescriptor){
                .nextInChain =
                    (const WGPUChainedStruct*)&(const WGPUSurfaceDescriptorFromWaylandSurface){
                        .chain =
                            (const WGPUChainedStruct){
                                .sType = WGPUSType_SurfaceDescriptorFromWaylandSurface,
                            },
                        .display = wayland_display,
                        .surface = wayland_surface,
                    },
            });
        assert(context.surface);
    }
#elif defined(WGPU_TARGET_WINDOWS)
    {
        HWND hwnd = glfwGetWin32Window(window);
        HINSTANCE hinstance = GetModuleHandle(nullptr);

        WGPUSurfaceDescriptorFromWindowsHWND surface_descriptor_from_windows_hwnd = {
            .chain =
                WGPUChainedStruct{
                    .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                },
            .hinstance = hinstance,
            .hwnd = hwnd,
        };

        WGPUSurfaceDescriptor surface_descriptor = {
            .nextInChain = (const WGPUChainedStruct*)&surface_descriptor_from_windows_hwnd,
        };

        context.surface = wgpuInstanceCreateSurface(context.instance, &surface_descriptor);
        assert(context.surface);
    }
#else
    #error "Unsupported WGPU_TARGET"
#endif

    WGPURequestAdapterOptions request_adapter_options = {
        .compatibleSurface = context.surface,
    };

    wgpuInstanceRequestAdapter(context.instance, &request_adapter_options, handle_request_adapter, &context);
    assert(context.adapter);

    wgpuAdapterRequestDevice(context.adapter, nullptr, handle_request_device, &context);
    assert(context.device);

    WGPUQueue queue = wgpuDeviceGetQueue(context.device);
    assert(queue);

    WGPUShaderModule shader_module = load_shader_module(context.device, "../src/shader.wgsl");
    assert(shader_module);

    WGPUPipelineLayoutDescriptor pipeline_layout_descriptor = {
        .label = "pipeline_layout",
    };

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(context.device, &pipeline_layout_descriptor);
    assert(pipeline_layout);

    WGPUSurfaceCapabilities surface_capabilities{};
    wgpuSurfaceGetCapabilities(context.surface, context.adapter, &surface_capabilities);

    std::array<WGPUColorTargetState, 1> color_target_states = {
        WGPUColorTargetState{
            .format = surface_capabilities.formats[0],
            .writeMask = WGPUColorWriteMask_All,
        },
    };

    WGPUFragmentState fragment_state = {
        .module = shader_module,
        .entryPoint = "fs_main",
        .targetCount = color_target_states.size(),
        .targets = color_target_states.data(),
    };

    WGPURenderPipelineDescriptor render_pipeline_descriptor = {
        .label = "render_pipeline",
        .layout = pipeline_layout,
        .vertex =
            WGPUVertexState{
                .module = shader_module,
                .entryPoint = "vs_main",
            },
        .primitive =
            WGPUPrimitiveState{
                .topology = WGPUPrimitiveTopology_TriangleList,
            },
        .multisample =
            WGPUMultisampleState{
                .count = 1,
                .mask = 0xFFFFFFFF,
            },
        .fragment = &fragment_state,
    };

    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(context.device, &render_pipeline_descriptor);
    assert(render_pipeline);

    context.config = WGPUSurfaceConfiguration{
        .device = context.device,
        .format = surface_capabilities.formats[0],
        .usage = WGPUTextureUsage_RenderAttachment,
        .alphaMode = surface_capabilities.alphaModes[0],
        .presentMode = WGPUPresentMode_Fifo,
    };

    {
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        context.config.width = width;
        context.config.height = height;
    }

    wgpuSurfaceConfigure(context.surface, &context.config);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        WGPUSurfaceTexture surface_texture;
        wgpuSurfaceGetCurrentTexture(context.surface, &surface_texture);

        switch (surface_texture.status) {
            case WGPUSurfaceGetCurrentTextureStatus_Success:
                // All good, could check for `surface_texture.suboptimal` here.
                break;
            case WGPUSurfaceGetCurrentTextureStatus_Timeout:
            case WGPUSurfaceGetCurrentTextureStatus_Outdated:
            case WGPUSurfaceGetCurrentTextureStatus_Lost: {
                // Skip this frame, and re-configure surface.
                if (surface_texture.texture != nullptr) {
                    wgpuTextureRelease(surface_texture.texture);
                }

                int width, height;
                glfwGetWindowSize(window, &width, &height);
                if (width != 0 && height != 0) {
                    context.config.width = width;
                    context.config.height = height;
                    wgpuSurfaceConfigure(context.surface, &context.config);
                }
                continue;
            }
            case WGPUSurfaceGetCurrentTextureStatus_OutOfMemory:
            case WGPUSurfaceGetCurrentTextureStatus_DeviceLost:
            case WGPUSurfaceGetCurrentTextureStatus_Force32:
                // Fatal error
                printf(LOG_PREFIX " get_current_texture status=%#.8x\n", surface_texture.status);
                abort();
        }
        assert(surface_texture.texture);

        WGPUTextureView surface_view = wgpuTextureCreateView(surface_texture.texture, nullptr);
        assert(surface_view);

        WGPUCommandEncoderDescriptor command_encoder_descriptor = {
            .label = "command_encoder",
        };

        WGPUCommandEncoder command_encoder =
            wgpuDeviceCreateCommandEncoder(context.device, &command_encoder_descriptor);
        assert(command_encoder);

        std::array<WGPURenderPassColorAttachment, 1> render_pass_color_attachments = {
            WGPURenderPassColorAttachment{
                .view = surface_view,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue =
                    WGPUColor{
                        .r = 0.1,
                        .g = 0.1,
                        .b = 0.1,
                        .a = 1.0,
                    },
            },
        };

        WGPURenderPassDescriptor render_pass_descriptor = {
            .label = "render_pass_encoder",
            .colorAttachmentCount = render_pass_color_attachments.size(),
            .colorAttachments = render_pass_color_attachments.data(),
        };

        WGPURenderPassEncoder render_pass_encoder =
            wgpuCommandEncoderBeginRenderPass(command_encoder, &render_pass_descriptor);
        assert(render_pass_encoder);

        wgpuRenderPassEncoderSetPipeline(render_pass_encoder, render_pipeline);
        wgpuRenderPassEncoderDraw(render_pass_encoder, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(render_pass_encoder);

        WGPUCommandBufferDescriptor command_buffer_descriptor = {
            .label = "command_buffer",
        };

        WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(command_encoder, &command_buffer_descriptor);
        assert(command_buffer);

        std::array<WGPUCommandBuffer, 1> command_buffers = {command_buffer};

        wgpuQueueSubmit(queue, command_buffers.size(), command_buffers.data());
        wgpuSurfacePresent(context.surface);

        wgpuCommandBufferRelease(command_buffer);
        wgpuRenderPassEncoderRelease(render_pass_encoder);
        wgpuCommandEncoderRelease(command_encoder);
        wgpuTextureViewRelease(surface_view);
        wgpuTextureRelease(surface_texture.texture);
    }

    wgpuRenderPipelineRelease(render_pipeline);
    wgpuPipelineLayoutRelease(pipeline_layout);
    wgpuShaderModuleRelease(shader_module);
    wgpuSurfaceCapabilitiesFreeMembers(surface_capabilities);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(context.device);
    wgpuAdapterRelease(context.adapter);
    wgpuSurfaceRelease(context.surface);
    glfwDestroyWindow(window);
    wgpuInstanceRelease(context.instance);
    glfwTerminate();

    return 0;
}
