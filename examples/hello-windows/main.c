#include "webgpu-headers/webgpu.h"
#include "wgpu.h"

#include "framework.h"
#include "unused.h"
#include <stdio.h>
#include <stdlib.h>

#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_LINUX_X11 2
#define WGPU_TARGET_WINDOWS 3
#define WGPU_TARGET_LINUX_WAYLAND 4

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <GLFW/glfw3.h>
#if WGPU_TARGET == WGPU_TARGET_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#elif WGPU_TARGET == WGPU_TARGET_LINUX_X11
#define GLFW_EXPOSE_NATIVE_X11
#elif WGPU_TARGET == WGPU_TARGET_LINUX_WAYLAND
#define GLFW_EXPOSE_NATIVE_WAYLAND
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

WGPUInstance instance = NULL;

static void handle_device_lost(WGPUDeviceLostReason reason, char const *message,
                               void *userdata) {
  UNUSED(userdata);

  printf("DEVICE LOST (%d): %s\n", reason, message);
}

static void handle_uncaptured_error(WGPUErrorType type, char const *message,
                                    void *userdata) {
  UNUSED(userdata);

  printf("UNCAPTURED ERROR (%d): %s\n", type, message);
}

static void handleGlfwKey(GLFWwindow *window, int key, int scancode, int action,
                          int mods) {
  UNUSED(window);
  UNUSED(scancode);
  UNUSED(mods);

  if (key == GLFW_KEY_R && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    WGPUGlobalReport report;
    wgpuGenerateReport(instance, &report);
    printGlobalReport(report);
  }
}

typedef struct ViewportDesc {
    GLFWwindow *window;
    WGPUColor background;
    WGPUSurface surface;
} ViewportDesc;

typedef struct Viewport {
    ViewportDesc desc;
    WGPUSwapChainDescriptor config;
    WGPUSwapChain swapChain;
    bool closed;
} Viewport;

ViewportDesc ViewportDesc_new(GLFWwindow *window, WGPUColor background, WGPUInstance instance) {
    Display *x11_display = glfwGetX11Display();
    Window x11_window = glfwGetX11Window(window);
    WGPUSurface surface = wgpuInstanceCreateSurface(
            instance,
            &(WGPUSurfaceDescriptor){
                    .label = NULL,
                    .nextInChain =
                    (const WGPUChainedStruct *)&(
                            WGPUSurfaceDescriptorFromXlibWindow){
                                    .chain =
                                    (WGPUChainedStruct){
                                            .next = NULL,
                                            .sType = WGPUSType_SurfaceDescriptorFromXlibWindow,
                                    },
                                    .display = x11_display,
                                    .window = x11_window,
                            },
            });

    return (ViewportDesc) {
        .window = window,
        .background = background,
        .surface = surface
    };
};

Viewport ViewportDesc_build(const ViewportDesc *desc, WGPUAdapter adapter, WGPUDevice device) {
    int32_t width, height;
    glfwGetWindowSize(desc->window, &width, &height);

    WGPUTextureFormat swapChainFormat = wgpuSurfaceGetPreferredFormat(desc->surface, adapter);
    WGPUSwapChainDescriptor config = {
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = swapChainFormat,
            .width = width,
            .height = height,
            .presentMode = WGPUPresentMode_Fifo,
    };
    WGPUSwapChain swapChain = wgpuDeviceCreateSwapChain(device, desc->surface, &config);

    return (Viewport) {
        .desc = *desc,
        .config = config,
        .swapChain = swapChain
    };
}

void Viewport_resize(Viewport *viewport, WGPUDevice device, int32_t width, int32_t height) {
    if (viewport->config.width == width && viewport->config.height == height) {
        return;
    }

    viewport->config.width = width;
    viewport->config.height = height;
    viewport->swapChain = wgpuDeviceCreateSwapChain(device, viewport->desc.surface, &viewport->config);
}

WGPUTextureView Viewport_get_current_texture(Viewport *viewport) {
    return wgpuSwapChainGetCurrentTextureView(viewport->swapChain);
}

double frac(uint32_t index, uint32_t max) {
    return ((double)index / (double)max);
}

bool allShouldClose(size_t viewportCount, Viewport viewports[]) {
    bool shouldClose = true;
    for (Viewport *viewport = viewports; viewport < viewports + viewportCount; ++viewport) {
        if (!viewport->closed && glfwWindowShouldClose(viewport->desc.window)) {
            glfwDestroyWindow(viewport->desc.window);
            viewport->closed = true;
        }

        shouldClose &= viewport->closed;
    }
    return shouldClose;
}

int main() {
  initializeLog();

  if (!glfwInit()) {
    printf("Cannot initialize glfw");
    return 1;
  }

  instance = wgpuCreateInstance(&(WGPUInstanceDescriptor) {
    .nextInChain = NULL,
  });

  const uint32_t windowSize = 128;
  const uint32_t windowPadding = 16;
  const uint32_t windowOffset = windowSize + windowPadding;
  const size_t rows = 4;
  const size_t columns = 4;
  const size_t viewportCount = rows * columns;
  ViewportDesc viewportDescs[viewportCount];
  Viewport viewports[viewportCount];

#if WGPU_TARGET == WGPU_TARGET_MACOS
  {
    id metal_layer = NULL;
    NSWindow *ns_window = glfwGetCocoaWindow(window);
    [ns_window.contentView setWantsLayer:YES];
    metal_layer = [CAMetalLayer layer];
    [ns_window.contentView setLayer:metal_layer];
    surface = wgpuInstanceCreateSurface(
        instance,
        &(WGPUSurfaceDescriptor){
            .label = NULL,
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    WGPUSurfaceDescriptorFromMetalLayer){
                    .chain =
                        (WGPUChainedStruct){
                            .next = NULL,
                            .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
                        },
                    .layer = metal_layer,
                },
        });
  }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_X11
  {
    for (size_t row = 0; row < rows; ++row) {
        for (size_t column = 0; column < columns; ++column) {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            char title[50];
            int32_t titleLength = sprintf(title, "wgpu with glfw %d,%d", row, column);
            GLFWwindow *window = glfwCreateWindow(windowSize, windowSize, title, NULL, NULL);
            glfwSetWindowPos(window, windowPadding + windowOffset * row, windowPadding + windowOffset * column);

            WGPUColor background = {
                    .a = 1.0,
                    .r = frac(row, rows),
                    .g = 0.5 - frac(row * column, rows * columns) * 0.5,
                    .b = frac(column, columns)
            };
            viewportDescs[row * rows + column] = ViewportDesc_new(window, background, instance);
        };
    }
  }
#elif WGPU_TARGET == WGPU_TARGET_LINUX_WAYLAND
  {
    struct wl_display *wayland_display = glfwGetWaylandDisplay();
    struct wl_surface *wayland_surface = glfwGetWaylandWindow(window);
    surface = wgpuInstanceCreateSurface(
        instance,
        &(WGPUSurfaceDescriptor){
            .label = NULL,
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    WGPUSurfaceDescriptorFromWaylandSurface){
                    .chain =
                        (WGPUChainedStruct){
                            .next = NULL,
                            .sType =
                                WGPUSType_SurfaceDescriptorFromWaylandSurface,
                        },
                    .display = wayland_display,
                    .surface = wayland_surface,
                },
        });
  }
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
  {
    HWND hwnd = glfwGetWin32Window(window);
    HINSTANCE hinstance = GetModuleHandle(NULL);
    surface = wgpuInstanceCreateSurface(
        instance,
        &(WGPUSurfaceDescriptor){
            .label = NULL,
            .nextInChain =
                (const WGPUChainedStruct *)&(
                    WGPUSurfaceDescriptorFromWindowsHWND){
                    .chain =
                        (WGPUChainedStruct){
                            .next = NULL,
                            .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                        },
                    .hinstance = hinstance,
                    .hwnd = hwnd,
                },
        });
  }
#else
#error "Unsupported WGPU_TARGET"
#endif

  WGPUAdapter adapter;
  wgpuInstanceRequestAdapter(instance,
                             &(WGPURequestAdapterOptions){
                                 .nextInChain = NULL,
                                 .compatibleSurface = viewportDescs[0].surface,
                             },
                             request_adapter_callback, (void *)&adapter);

  printAdapterFeatures(adapter);

  WGPUDevice device;
  wgpuAdapterRequestDevice(adapter,
                           &(WGPUDeviceDescriptor){
                               .nextInChain = NULL,
                               .label = "Device",
                               .requiredLimits =
                                   &(WGPURequiredLimits){
                                       .nextInChain = NULL,
                                       .limits =
                                           (WGPULimits){
                                               .maxBindGroups = 1,
                                           },
                                   },
                               .defaultQueue =
                                   (WGPUQueueDescriptor){
                                       .nextInChain = NULL,
                                       .label = NULL,
                                   },
                           },
                           request_device_callback, (void *)&device);

    {
        for (size_t id = 0; id < viewportCount; ++id) {
            viewports[id] = ViewportDesc_build(&viewportDescs[id], device, adapter);
        }
    }

  wgpuDeviceSetUncapturedErrorCallback(device, handle_uncaptured_error, NULL);
  wgpuDeviceSetDeviceLostCallback(device, handle_device_lost, NULL);

//  WGPUShaderModuleDescriptor shaderSource = load_wgsl("shader.wgsl");
//  WGPUShaderModule shader = wgpuDeviceCreateShaderModule(device, &shaderSource);

//  WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(
//      device,
//      &(WGPURenderPipelineDescriptor){
//          .label = "Render pipeline",
//          .vertex =
//              (WGPUVertexState){
//                  .module = shader,
//                  .entryPoint = "vs_main",
//                  .bufferCount = 0,
//                  .buffers = NULL,
//              },
//          .primitive =
//              (WGPUPrimitiveState){
//                  .topology = WGPUPrimitiveTopology_TriangleList,
//                  .stripIndexFormat = WGPUIndexFormat_Undefined,
//                  .frontFace = WGPUFrontFace_CCW,
//                  .cullMode = WGPUCullMode_None},
//          .multisample =
//              (WGPUMultisampleState){
//                  .count = 1,
//                  .mask = ~0,
//                  .alphaToCoverageEnabled = false,
//              },
//          .fragment =
//              &(WGPUFragmentState){
//                  .module = shader,
//                  .entryPoint = "fs_main",
//                  .targetCount = 1,
//                  .targets =
//                      &(WGPUColorTargetState){
//                          .format = swapChainFormat,
//                          .blend =
//                              &(WGPUBlendState){
//                                  .color =
//                                      (WGPUBlendComponent){
//                                          .srcFactor = WGPUBlendFactor_One,
//                                          .dstFactor = WGPUBlendFactor_Zero,
//                                          .operation = WGPUBlendOperation_Add,
//                                      },
//                                  .alpha =
//                                      (WGPUBlendComponent){
//                                          .srcFactor = WGPUBlendFactor_One,
//                                          .dstFactor = WGPUBlendFactor_Zero,
//                                          .operation = WGPUBlendOperation_Add,
//                                      }},
//                          .writeMask = WGPUColorWriteMask_All},
//              },
//          .depthStencil = NULL,
//      });

//  int prevWidth = 0;
//  int prevHeight = 0;
//  glfwGetWindowSize(window, &prevWidth, &prevHeight);
//
//  WGPUSwapChain swapChain =
//      wgpuDeviceCreateSwapChain(device, surface,
//                                &(WGPUSwapChainDescriptor){
//                                    .usage = WGPUTextureUsage_RenderAttachment,
//                                    .format = swapChainFormat,
//                                    .width = prevWidth,
//                                    .height = prevHeight,
//                                    .presentMode = WGPUPresentMode_Fifo,
//                                });
//
//  glfwSetKeyCallback(window, handleGlfwKey);



  while (!allShouldClose(viewportCount, viewports)) {
      {
          for (Viewport *viewport = viewports; viewport < viewports + viewportCount; ++viewport) {
            if (viewport->closed) {
                continue;
            }

            int32_t width = 0, height = 0;
            glfwGetWindowSize(viewport->desc.window, &width, &height);

            Viewport_resize(viewport, device, width, height);

            WGPUTextureView nextTexture = Viewport_get_current_texture(viewport);

            WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(
                    device, &(WGPUCommandEncoderDescriptor){.label = "Command Encoder"});

            WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(
                    encoder, &(WGPURenderPassDescriptor){
                            .colorAttachments =
                            &(WGPURenderPassColorAttachment){
                                    .view = nextTexture,
                                    .resolveTarget = 0,
                                    .loadOp = WGPULoadOp_Clear,
                                    .storeOp = WGPUStoreOp_Store,
                                    .clearValue = viewport->desc.background,
                            },
                            .colorAttachmentCount = 1,
                            .depthStencilAttachment = NULL,
                    });
//
//            wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
//            wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);
            wgpuRenderPassEncoderEnd(renderPass);
            wgpuTextureViewDrop(nextTexture);

            WGPUQueue queue = wgpuDeviceGetQueue(device);
            WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(
                    encoder, &(WGPUCommandBufferDescriptor){.label = NULL});
            wgpuQueueSubmit(queue, 1, &cmdBuffer);
            wgpuSwapChainPresent(viewport->swapChain);
        }
      }

    glfwPollEvents();
  }

  glfwTerminate();

  return 0;
}
