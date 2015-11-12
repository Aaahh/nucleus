/**
 * (c) 2015 Alexandro Sanchez Bach. All rights reserved.
 * Released under GPL v2 license. Read LICENSE for more details.
 */

#include "direct3d11_backend.h"
#include "nucleus/logger/logger.h"
#include "nucleus/graphics/backend/direct3d11/direct3d11.h"

#include "nucleus/graphics/backend/direct3d11/direct3d11_command_buffer.h"
#include "nucleus/graphics/backend/direct3d11/direct3d11_command_queue.h"
#include "nucleus/graphics/backend/direct3d11/direct3d11_heap.h"
#include "nucleus/graphics/backend/direct3d11/direct3d11_target.h"
#include "nucleus/graphics/backend/direct3d11/direct3d11_texture.h"

#include <vector>

namespace gfx {

Direct3D11Backend::Direct3D11Backend() : IBackend() {
}

Direct3D11Backend::~Direct3D11Backend() {
}

bool Direct3D11Backend::initialize(const BackendParameters& params) {
    if (!initializeDirect3D11()) {
        logger.warning(LOG_GRAPHICS, "Direct3D11Backend::initialize: Could load Direct3D11 dynamic library");
        return false;
    }

    IDXGIFactory4* factory;
    if (FAILED(_CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)))) {
        logger.warning(LOG_GRAPHICS, "Direct3D11Backend::initialize: CreateDXGIFactory1 failed");
        return false;
    }

    UINT createDeviceFlags;
#ifdef NUCLEUS_BUILD_DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL featureLevel;
    if (FAILED(_D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, 0, createDeviceFlags, 0, 0, D3D11_SDK_VERSION, &device, &featureLevel, &deviceContext))) {
        logger.warning(LOG_GRAPHICS, "Direct3D11Backend::initialize: D3D11CreateDevice failed");
        return false;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = params.width;
    swapChainDesc.BufferDesc.Height = params.height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.OutputWindow = params.hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Windowed = TRUE;

    if (FAILED(factory->CreateSwapChain(device, &swapChainDesc, &swapChain))) {
        logger.error(LOG_GRAPHICS, "Direct3D11Backend::initialize: factory->CreateSwapChain failed");
        return false;
    }

    // Get render target buffers from swap chain
    swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainRenderBuffer[0]));
    swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainRenderBuffer[1]));

    parameters = params;
    return true;
}

ICommandQueue* Direct3D11Backend::createCommandQueue() {
    auto* commandQueue = new Direct3D11CommandQueue();

    if (!commandQueue->initialize(device)) {
        logger.error(LOG_GRAPHICS, "OpenGLBackend::createCommandQueue: Could not initialize OpenGLCommandQueue");
        return nullptr;
    }
    return commandQueue;
}

ICommandBuffer* Direct3D11Backend::createCommandBuffer() {
    auto* commandBuffer = new Direct3D11CommandBuffer();
    return commandBuffer;
}

IHeap* Direct3D11Backend::createHeap(const HeapDesc& desc) {
    auto* heap = new Direct3D11Heap();

    D3D11_DESCRIPTOR_HEAP_DESC d3dDesc = {};
    d3dDesc.NumDescriptors = desc.size;
    d3dDesc.Flags = D3D11_DESCRIPTOR_HEAP_FLAG_NONE;

    switch (desc.type) {
    case HEAP_TYPE_CT:
        d3dDesc.Type = D3D11_DESCRIPTOR_HEAP_TYPE_RTV; break;
    case HEAP_TYPE_DST:
        d3dDesc.Type = D3D11_DESCRIPTOR_HEAP_TYPE_DSV; break;
    default:
        logger.error(LOG_GRAPHICS, "Unimplemented descriptor heap type");
    }

    if (FAILED(device->CreateDescriptorHeap(&d3dDesc, IID_PPV_ARGS(&heap->heap)))) {
        logger.error(LOG_GRAPHICS, "Direct3D11Backend::createHeap: device->CreateDescriptorHeap failed");
        return nullptr;
    }

    return heap;
}

IColorTarget* Direct3D11Backend::createColorTarget(ITexture* texture) {
    auto d3dTexture = static_cast<Direct3D11Texture*>(texture);
    if (!d3dTexture) {
        logger.error(LOG_GRAPHICS, "Direct3D11Backend::createColorTarget: Invalid texture specified");
        return nullptr;
    }

    auto* target = new Direct3D11ColorTarget();
    device->CreateRenderTargetView(d3dTexture->resource, NULL, target->handle);
    return target;
}

IDepthStencilTarget* Direct3D11Backend::createDepthStencilTarget(ITexture* texture) {
    return nullptr;
}

void Direct3D11Backend::createPipeline() {
}

void Direct3D11Backend::createShader() {
}

ITexture* Direct3D11Backend::createTexture(const TextureDesc& desc) {
    auto* texture = new Direct3D11Texture();

    // Create resource description
    D3D11_RESOURCE_DESC d3dDesc = {};
    d3dDesc.Alignment = desc.alignment;
    d3dDesc.Height = desc.height;
    d3dDesc.Width = desc.width;
    d3dDesc.MipLevels = desc.mipmapLevels;

    // Pick appropriate format
    switch (desc.format) {
    case TEXTURE_FORMAT_R8G8B8A8:
        d3dDesc.Format = DXGI_FORMAT_R8G8B8A8_UINT; break;
    default:
        logger.error(LOG_GRAPHICS, "Unimplemented texture format");
    }

    D3D11_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D11_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D11_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D11_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    if (FAILED(device->CreateCommittedResource(&heapProps, D3D11_HEAP_FLAG_NONE, &d3dDesc, D3D11_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture->resource)))) {
        logger.error(LOG_GRAPHICS, "Direct3D11Backend::createTexture: device->CreateCommittedResource failed");
        return nullptr;
    }

    return texture;
}

bool Direct3D11Backend::doSwapBuffers() {
    if (FAILED(swapChain->Present(0, 0))) {
        logger.error(LOG_GRAPHICS, "Direct3D11Backend::doSwapBuffers: swapChain->Present failed");
        return false;
    }
    std::swap(screenBackBuffer, screenFrontBuffer);
    return true;
}

}  // namespace gfx