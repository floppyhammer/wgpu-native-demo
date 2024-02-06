#include <array>
#include <cassert>
#include <cstdio>
#include <iostream>

#include "../common.h"
#include "emscripten.h"
#include "emscripten/html5.h"
#include "emscripten/html5_webgpu.h"
#include "webgpu/webgpu.h"

#if defined(__EMSCRIPTEN__)
    #include <GLFW/glfw3.h>
#endif

//--------------------------------------------------
// defines
//--------------------------------------------------
#define SHADER_SOURCE(...) #__VA_ARGS__

struct AppState {
    // canvas
    struct {
        const char* name;
        uint32_t width, height;
    } canvas;

    // glfw
    struct {
        GLFWwindow* window;
    } glfw;

    // wgpu
    struct {
        WGPUInstance instance;
        WGPUDevice device;
        WGPUQueue queue;
        WGPUSwapChain swapchain;
        WGPURenderPipeline pipeline;
    } wgpu;

    // resources
    struct {
        WGPUBuffer vbuffer, ibuffer, ubuffer;
        WGPUBindGroup bindgroup;
    } res;
};

// state
static AppState state;

//--------------------------------------------------
// callbacks and functions
//--------------------------------------------------

// callbacks
static int resize(int, const EmscriptenUiEvent*, void*);
static void draw();

// helper functions
static WGPUSwapChain create_swapchain();

//--------------------------------------------------
// Vertex and fragment shaders
//--------------------------------------------------

static const char* wgsl_triangle = SHADER_SOURCE(
    // attribute/uniform decls

    struct VertexIn {
        @ location(0) aPos : vec2<f32>,
        @ location(1) aCol : vec3<f32>,
    };
    struct VertexOut {
        @ location(0) vCol : vec3<f32>,
        @ builtin(position) Position : vec4<f32>,
    };
    struct Rotation {
        @ location(0) degs : f32,
    };
    @ group(0) @ binding(0) var<uniform> uRot : Rotation;

    // vertex shader

    @ vertex
    fn vs_main(input : VertexIn) -> VertexOut {
        var rads : f32 = radians(uRot.degs);
        var cosA : f32 = cos(rads);
        var sinA : f32 = sin(rads);
        var rot : mat3x3<f32> = mat3x3<f32>(
            vec3<f32>( cosA, sinA, 0.0),
            vec3<f32>(-sinA, cosA, 0.0),
            vec3<f32>( 0.0,  0.0,  1.0));
        var output : VertexOut;
        output.Position = vec4<f32>(rot * vec3<f32>(input.aPos, 1.0), 1.0);
        output.vCol = input.aCol;
        return output;
    }

    // fragment shader

    @ fragment
    fn fs_main(@ location(0) vCol : vec3<f32>) -> @ location(0) vec4<f32> {
        return vec4<f32>(vCol, 1.0);
    }
);

//--------------------------------------------------
// Main
//--------------------------------------------------

int main(int argc, const char* argv[]) {
    //-----------------
    // Init
    //-----------------
    // Initialize GLFW
    {
        if (!glfwInit()) {
            std::cerr << "Could not initialize GLFW!" << std::endl;
            return 1;
        } else {
            std::cout << "Initialized GLFW" << std::endl;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        state.glfw.window = glfwCreateWindow(640, 480, "wgpu", NULL, NULL);
        std::cout << "Created GLFW window" << std::endl;
    }

    state.canvas.name = "canvas";
    state.wgpu.instance = wgpuCreateInstance(NULL);
    assert(state.wgpu.instance && "Creating instance failed!");
    std::cout << "Created wgpu instance" << std::endl;

    state.wgpu.device = emscripten_webgpu_get_device();
    assert(state.wgpu.device && "Getting device failed!");
    std::cout << "Got webgpu device" << std::endl;

    state.wgpu.queue = wgpuDeviceGetQueue(state.wgpu.device);
    assert(state.wgpu.queue && "Getting queue failed!");

    resize(0, NULL, NULL); // set size and create swapchain
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, 0, false, resize);

    //-----------------
    // Setup pipeline
    //-----------------

#ifdef EMSCRIPTEN
    auto shader_file = "shader.wgsl";
#else
    auto shader_file = "../resources/shader.wgsl";
#endif

    WGPUShaderModule shader_module = load_shader_module(state.wgpu.device, shader_file);
    assert(shader_module && "Loading shader module failed!");

    WGPUPipelineLayoutDescriptor pipeline_layout_descriptor = {
        .label = "pipeline_layout",
    };

    WGPUPipelineLayout pipeline_layout = wgpuDeviceCreatePipelineLayout(state.wgpu.device, &pipeline_layout_descriptor);
    assert(pipeline_layout && "Creating pipeline layout failed!");

    std::array<WGPUColorTargetState, 1> color_target_states = {
        WGPUColorTargetState{
            .format = WGPUTextureFormat_BGRA8Unorm,
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

    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(state.wgpu.device, &render_pipeline_descriptor);
    assert(render_pipeline && "Creating render pipeline failed!");

    state.wgpu.pipeline = render_pipeline;

    //-----------------
    // Setup
    //-----------------

    //-----------------
    // Main loop
    //-----------------

    emscripten_set_main_loop(draw, 0, 1);

    //-----------------
    // Quit
    //-----------------

    wgpuRenderPipelineRelease(state.wgpu.pipeline);
    wgpuSwapChainRelease(state.wgpu.swapchain);
    wgpuQueueRelease(state.wgpu.queue);
    wgpuDeviceRelease(state.wgpu.device);
    wgpuInstanceRelease(state.wgpu.instance);

    return 0;
}

// draw callback
void draw() {
    // create texture view
    WGPUTextureView surface_view = wgpuSwapChainGetCurrentTextureView(state.wgpu.swapchain);

    // create command encoder
    WGPUCommandEncoder cmd_encoder = wgpuDeviceCreateCommandEncoder(state.wgpu.device, NULL);

    WGPURenderPassColorAttachment render_pass_color_attachment = {.view = surface_view,
                                                                  .loadOp = WGPULoadOp_Clear,
                                                                  .storeOp = WGPUStoreOp_Store,
                                                                  .clearValue = (WGPUColor){0.2f, 0.2f, 0.3f, 1.0f}};

    // begin render pass
    WGPURenderPassDescriptor render_pass_descriptor = {
        // color attachments
        .colorAttachmentCount = 1,
        .colorAttachments = &render_pass_color_attachment,
    };
    WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(cmd_encoder, &render_pass_descriptor);

    {
        wgpuRenderPassEncoderSetPipeline(render_pass, state.wgpu.pipeline);
        wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);
    }

    // End render pass.
    wgpuRenderPassEncoderEnd(render_pass);

    // Create command buffer.
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(cmd_encoder, NULL); // after 'end render pass'

    // Submit commands.
    wgpuQueueSubmit(state.wgpu.queue, 1, &cmd_buffer);

    // Release all.
    wgpuRenderPassEncoderRelease(render_pass);
    wgpuCommandEncoderRelease(cmd_encoder);
    wgpuCommandBufferRelease(cmd_buffer);
    wgpuTextureViewRelease(surface_view);
}

/// Resize callback
int resize(int event_type, const EmscriptenUiEvent* ui_event, void* user_data) {
    double w, h;
    emscripten_get_element_css_size(state.canvas.name, &w, &h);

    state.canvas.width = (int)w;
    state.canvas.height = (int)h;
    emscripten_set_canvas_element_size(state.canvas.name, state.canvas.width, state.canvas.height);

    if (state.wgpu.swapchain) {
        wgpuSwapChainRelease(state.wgpu.swapchain);
        state.wgpu.swapchain = NULL;
    }

    state.wgpu.swapchain = create_swapchain();

    return 1;
}

/// Helper functions
WGPUSwapChain create_swapchain() {
    WGPUSurfaceDescriptorFromCanvasHTMLSelector surface_descriptor_from_canvas_html_selector = {
        .chain =
            WGPUChainedStruct{
                .sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector,
            },
        .selector = state.canvas.name,
    };

    WGPUSurfaceDescriptor surface_descriptor = {.nextInChain =
                                                    (WGPUChainedStruct*)&surface_descriptor_from_canvas_html_selector};

    WGPUSurface surface = wgpuInstanceCreateSurface(state.wgpu.instance, &surface_descriptor);

    WGPUSwapChainDescriptor swap_chain_descriptor = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = state.canvas.width,
        .height = state.canvas.height,
        .presentMode = WGPUPresentMode_Fifo,
    };
    return wgpuDeviceCreateSwapChain(state.wgpu.device, surface, &swap_chain_descriptor);
}
