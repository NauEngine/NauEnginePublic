#include "nau/mgpu/bridge/dof_bridge.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <d3dcompiler.h>

#include "nau/mgpu/Graphics/GCommandList.h"
#include "nau/mgpu/Graphics/GCommandQueue.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GDeviceFactory.h"
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/gpu_profiler/d3d12_timestamp_source.h"

namespace nau::mgpu::bridge
{
    namespace gfx = nau::PEPEngine::Graphics;
    using Microsoft::WRL::ComPtr;

    namespace
    {
        void check(HRESULT hr)
        {
            if (FAILED(hr))
                throw std::runtime_error("D3D12 HRESULT " + std::to_string(static_cast<uint32_t>(hr)));
        }

        struct Handle
        {
            HANDLE value = nullptr;
            ~Handle() { if (value) CloseHandle(value); }
        };

        void wait(ID3D12Fence* fence, UINT64 value, ID3D12Device* device)
        {
            Handle event;
            event.value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!event.value) throw std::runtime_error("CreateEvent failed");
            check(fence->SetEventOnCompletion(value, event.value));
            while (fence->GetCompletedValue() < value)
            {
                const DWORD result = WaitForSingleObject(event.value, 250);
                if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT)
                    throw std::runtime_error("GPU fence wait failed");
                check(device->GetDeviceRemovedReason());
            }
            check(device->GetDeviceRemovedReason());
        }

        void transition(ID3D12GraphicsCommandList* list, ID3D12Resource* resource,
                        D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            if (before == after) return;
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
            list->ResourceBarrier(1, &barrier);
        }

        // Shared BUFFER transport also works when row-major texture sharing is unavailable.
        // Only local textures are sampled or used as UAVs, as in the reference copy path.
        struct Channel
        {
            ComPtr<ID3D12Heap> heaps[2];
            ComPtr<ID3D12Resource> buffers[2];
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};

            void create(ID3D12Device* primary, ID3D12Device* secondary,
                        DXGI_FORMAT format, UINT width, UINT height)
            {
                const auto texture = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
                UINT64 bytes = 0;
                primary->GetCopyableFootprints(&texture, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
                const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(bytes, D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER);
                const auto allocation = primary->GetResourceAllocationInfo(0, 1, &buffer);
                if (allocation.SizeInBytes == UINT64_MAX) throw std::runtime_error("Invalid shared allocation");
                D3D12_HEAP_DESC heap = {};
                heap.SizeInBytes = allocation.SizeInBytes;
                heap.Alignment = allocation.Alignment;
                heap.Properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                heap.Flags = D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER;
                check(primary->CreateHeap(&heap, IID_PPV_ARGS(&heaps[0])));
                Handle handle;
                check(primary->CreateSharedHandle(heaps[0].Get(), nullptr, GENERIC_ALL, nullptr, &handle.value));
                check(secondary->OpenSharedHandle(handle.value, IID_PPV_ARGS(&heaps[1])));
                ID3D12Device* devices[] = {primary, secondary};
                for (UINT side = 0; side != 2; ++side)
                    check(devices[side]->CreatePlacedResource(heaps[side].Get(), 0, &buffer,
                        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&buffers[side])));
            }

            void copy(ID3D12GraphicsCommandList* list, UINT side, ID3D12Resource* texture, bool toShared)
            {
                const auto state = toShared ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COPY_SOURCE;
                transition(list, buffers[side].Get(), D3D12_RESOURCE_STATE_COMMON, state);
                CD3DX12_TEXTURE_COPY_LOCATION tex(texture, 0);
                CD3DX12_TEXTURE_COPY_LOCATION buf(buffers[side].Get(), footprint);
                if (toShared) list->CopyTextureRegion(&buf, 0, 0, 0, &tex, nullptr);
                else list->CopyTextureRegion(&tex, 0, 0, 0, &buf, nullptr);
                transition(list, buffers[side].Get(), state, D3D12_RESOURCE_STATE_COMMON);
            }
        };

        enum Image { Color, Depth, Coc, Mask, Pyramid1, Pyramid2, Filled, Wmax, Filtered, ImageCount };
        struct ImageData
        {
            ComPtr<ID3D12Resource> texture;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
            UINT width = 0, height = 0;
            D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        };
        struct Pass
        {
            const char* macro;
            std::array<int, 3> inputs;
            Image output;
        };
        constexpr Pass passes[] = {
            {"DOF_PASS_COC", {Depth, -1, -1}, Coc},
            {"DOF_PASS_HOLES", {Depth, -1, -1}, Mask},
            {"DOF_PASS_PYRAMID1", {Color, Mask, -1}, Pyramid1},
            {"DOF_PASS_PYRAMID2", {Pyramid1, -1, -1}, Pyramid2},
            {"DOF_PASS_PATCH2", {Color, Mask, -1}, Pyramid2},
            {"DOF_PASS_PATCH1", {Color, Pyramid2, Mask}, Pyramid1},
            {"DOF_PASS_PATCH0", {Color, Pyramid1, Mask}, Filled},
            {"DOF_PASS_WMAX", {Mask, Coc, -1}, Wmax},
            {"DOF_PASS_WFILTER", {Wmax, -1, -1}, Filtered}
        };
        constexpr UINT passCount = static_cast<UINT>(sizeof(passes) / sizeof(passes[0]));

        // Payload of the transfers per pixel, for the GPU profiler: color RGBA16F + depth R32F in, filled RGBA16F +
        // filtered R16F out.
        constexpr uint64_t inBytesPerPixel = 8 + 4;
        constexpr uint64_t outBytesPerPixel = 8 + 2;
    }

    struct DofBridge::Impl
    {
        ComPtr<ID3D12Device> primary;
        ComPtr<ID3D12CommandQueue> primaryQueue;
        std::shared_ptr<gfx::GDevice> secondary;
        ComPtr<ID3D12RootSignature> root;
        std::array<ComPtr<ID3D12PipelineState>, passCount> pipelines;
        ComPtr<ID3D12DescriptorHeap> descriptors;
        ComPtr<ID3D12Fence> primaryFence, secondaryFence;
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> primaryList;
        std::array<ImageData, ImageCount> images;
        std::array<Channel, 4> channels;
        UINT width = 0, height = 0, descriptorSize = 0;
        UINT64 serial = 0;
        bool failed = false;
        std::string error, adapterName;
        // GPU profiler (NAU_GPU_PROFILER=1): timestamps on the graphics queue of the second GPU and on Nau's queue (the
        // bridge's own command list there). Declared last, so they are destroyed (unregistered) before the devices.
        // They only signal their own fence after the bridge's one. Transfers "dof_in" (GPU 0 -> GPU 1) and "dof_out"
        // (GPU 1 -> GPU 0) run within one frame: matched by the frame (FrameToken), age 0.
        gpu_profiler::D3D12TimestampSource secondaryTimestamps;
        gpu_profiler::D3D12TimestampSource primaryTimestamps;

        void createPipelines()
        {
            CD3DX12_DESCRIPTOR_RANGE srv, uav;
            srv.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0);
            uav.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
            CD3DX12_ROOT_PARAMETER parameters[3];
            parameters[0].InitAsConstants(8, 0);
            parameters[1].InitAsDescriptorTable(1, &srv);
            parameters[2].InitAsDescriptorTable(1, &uav);
            CD3DX12_ROOT_SIGNATURE_DESC desc(3, parameters);
            ComPtr<ID3DBlob> blob, errors;
            check(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors));
            auto device = secondary->GetDXDevice();
            check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));

            wchar_t executable[32768];
            const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
            if (!length || length >= 32768) throw std::runtime_error("Executable path unavailable");
            std::wstring path(executable, length);
            path.resize(path.find_last_of(L"\\/") + 1);
            path += L"Shaders/DOF/dof_pipeline.hlsl";
            for (UINT i = 0; i < passCount; ++i)
            {
                const D3D_SHADER_MACRO macros[] = {{passes[i].macro, "1"}, {nullptr, nullptr}};
                const HRESULT hr = D3DCompileFromFile(path.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    "CSMain", "cs_5_1", D3DCOMPILE_ENABLE_STRICTNESS, 0, &blob, &errors);
                if (FAILED(hr))
                    throw std::runtime_error(errors ? std::string(static_cast<const char*>(errors->GetBufferPointer()),
                        errors->GetBufferSize()) : "DOF shader missing or failed to compile");
                D3D12_COMPUTE_PIPELINE_STATE_DESC pso = {};
                pso.pRootSignature = root.Get();
                pso.CS = {blob->GetBufferPointer(), blob->GetBufferSize()};
                check(device->CreateComputePipelineState(&pso, IID_PPV_ARGS(&pipelines[i])));
                blob.Reset();
                errors.Reset();
            }
        }

        void initialize(ID3D12Device* device, ID3D12CommandQueue* queue)
        {
            primary = device;
            primaryQueue = queue;
            const LUID luid = primary->GetAdapterLuid();
            // Do not initialize the module factory on a one-adapter machine: its
            // enumeration creates devices/queues before the transfer test rejects it.
            ComPtr<IDXGIFactory1> factory;
            check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
            bool hasSecondary = false;
            for (UINT i = 0; ; ++i)
            {
                ComPtr<IDXGIAdapter1> adapter;
                const HRESULT hr = factory->EnumAdapters1(i, &adapter);
                if (hr == DXGI_ERROR_NOT_FOUND) break;
                check(hr);
                DXGI_ADAPTER_DESC1 desc = {};
                check(adapter->GetDesc1(&desc));
                if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) continue;
                if (desc.AdapterLuid.HighPart == luid.HighPart && desc.AdapterLuid.LowPart == luid.LowPart) continue;
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
                {
                    hasSecondary = true;
                    break;
                }
            }
            if (!hasSecondary) throw std::runtime_error("no suitable secondary hardware GPU (preflight)");
            gfx::GDeviceFactory::SetDebugLayerEnabled(false);
            for (const auto& candidate : gfx::GDeviceFactory::GetAllDevices(false))
            {
                const LUID other = candidate->GetDXDevice()->GetAdapterLuid();
                if (other.HighPart != luid.HighPart || other.LowPart != luid.LowPart)
                {
                    secondary = candidate;
                    break;
                }
            }
            if (!secondary) throw std::runtime_error("no suitable secondary hardware GPU");
            const auto name = secondary->GetName();
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), nullptr, 0, nullptr, nullptr);
            adapterName.resize(bytes);
            WideCharToMultiByte(CP_UTF8, 0, name.data(), static_cast<int>(name.size()), adapterName.data(), bytes, nullptr, nullptr);
            check(primary->CreateFence(0, D3D12_FENCE_FLAG_SHARED | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER,
                IID_PPV_ARGS(&primaryFence)));
            Handle handle;
            check(primary->CreateSharedHandle(primaryFence.Get(), nullptr, GENERIC_ALL, nullptr, &handle.value));
            check(secondary->GetDXDevice()->OpenSharedHandle(handle.value, IID_PPV_ARGS(&secondaryFence)));
            check(primary->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
            check(primary->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                IID_PPV_ARGS(&primaryList)));
            check(primaryList->Close());
            createPipelines();
            // Does nothing unless the profiler is enabled.
            gpu_profiler::GpuProfiler::instance().setGpuName(gpu_profiler::SecondaryGpu, adapterName.c_str());
            secondaryTimestamps.init(secondary->GetDXDevice().Get(),
                secondary->GetCommandQueue(gfx::GQueueType::Graphics)->GetD3D12CommandQueue().Get(),
                gpu_profiler::SecondaryGpu, "gfx1.dof");
            primaryTimestamps.init(primary.Get(), primaryQueue.Get(), gpu_profiler::PrimaryGpu, "gfx0.dof");
        }

        void resize(UINT w, UINT h)
        {
            // execute() completes all queues before returning, so old allocations are no longer in flight.
            auto device = secondary->GetDXDevice();
            const UINT halfW = (w / 2) ? w / 2 : 1, halfH = (h / 2) ? h / 2 : 1;
            const UINT quarterW = (w / 4) ? w / 4 : 1, quarterH = (h / 4) ? h / 4 : 1;
            const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R32_FLOAT,
                DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R8_UINT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16_FLOAT, DXGI_FORMAT_R16_FLOAT};
            images = {};
            channels = {};
            for (int i = 0; i < ImageCount; ++i)
            {
                auto& image = images[i];
                image.format = formats[i];
                image.width = i == Pyramid1 ? halfW : i == Pyramid2 ? quarterW : w;
                image.height = i == Pyramid1 ? halfH : i == Pyramid2 ? quarterH : h;
                const auto desc = CD3DX12_RESOURCE_DESC::Tex2D(image.format, image.width, image.height, 1, 1,
                    1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
                const auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                    D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&image.texture)));
            }
            channels[0].create(primary.Get(), device.Get(), formats[Color], w, h);
            channels[1].create(primary.Get(), device.Get(), formats[Depth], w, h);
            channels[2].create(primary.Get(), device.Get(), formats[Filled], w, h);
            channels[3].create(primary.Get(), device.Get(), formats[Filtered], w, h);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = passCount * 4;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            descriptors.Reset();
            check(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptors)));
            descriptorSize = device->GetDescriptorHandleIncrementSize(heapDesc.Type);
            for (UINT pass = 0; pass < passCount; ++pass)
            {
                for (UINT slot = 0; slot < 3; ++slot)
                {
                    const int input = passes[pass].inputs[slot];
                    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
                    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srv.Texture2D.MipLevels = 1;
                    srv.Format = input < 0 ? DXGI_FORMAT_R32_FLOAT : images[input].format;
                    device->CreateShaderResourceView(input < 0 ? nullptr : images[input].texture.Get(), &srv,
                        CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptors->GetCPUDescriptorHandleForHeapStart(),
                            pass * 4 + slot, descriptorSize));
                }
                const auto& output = images[passes[pass].output];
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav = {};
                uav.Format = output.format;
                uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                device->CreateUnorderedAccessView(output.texture.Get(), nullptr, &uav,
                    CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptors->GetCPUDescriptorHandleForHeapStart(),
                        pass * 4 + 3, descriptorSize));
            }
            width = w;
            height = h;
        }

        void imageState(ID3D12GraphicsCommandList* list, Image index, D3D12_RESOURCE_STATES state)
        {
            auto& image = images[index];
            transition(list, image.texture.Get(), image.state, state);
            image.state = state;
        }

        void beginPrimary()
        {
            check(allocator->Reset());
            check(primaryList->Reset(allocator.Get(), nullptr));
        }

        void submitPrimary()
        {
            check(primaryList->Close());
            ID3D12CommandList* lists[] = {primaryList.Get()};
            primaryQueue->ExecuteCommandLists(1, lists);
            check(primaryQueue->Signal(primaryFence.Get(), ++serial));
            wait(primaryFence.Get(), serial, primary.Get());
        }

        DofResult execute(ID3D12Resource* color, ID3D12Resource* depth,
                          ID3D12Resource* filled, ID3D12Resource* filtered, const DofParameters& params)
        {
            bool primaryInFlight = false;
            try
            {
                check(primary->GetDeviceRemovedReason());
                check(secondary->GetDXDevice()->GetDeviceRemovedReason());
                const std::array<ID3D12Resource*, 4> resources = {color, depth, filled, filtered};
                const Image indices[] = {Color, Depth, Filled, Filtered};
                for (UINT i = 0; i < 4; ++i)
                {
                    if (!resources[i]) throw std::runtime_error("null DOF resource");
                    const auto desc = resources[i]->GetDesc();
                    const DXGI_FORMAT format = images[indices[i]].format;
                    const bool compatible = desc.Format == format ||
                        (format == DXGI_FORMAT_R16G16B16A16_FLOAT && desc.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS) ||
                        (format == DXGI_FORMAT_R32_FLOAT && desc.Format == DXGI_FORMAT_R32_TYPELESS) ||
                        (format == DXGI_FORMAT_R16_FLOAT && desc.Format == DXGI_FORMAT_R16_TYPELESS);
                    if (!compatible || desc.Width != width || desc.Height != height || desc.SampleDesc.Count != 1 ||
                        desc.MipLevels != 1 || desc.DepthOrArraySize != 1)
                        throw std::runtime_error("unsupported DOF resource format, dimensions or sample count");
                    ComPtr<ID3D12Device> owner;
                    check(resources[i]->GetDevice(IID_PPV_ARGS(&owner)));
                    if (owner.Get() != primary.Get()) throw std::runtime_error("DOF resource belongs to another device");
                }

                beginPrimary();
                primaryTimestamps.setCommandList(primaryList.Get());
                {
                    // Source half of dof_in, on Nau's queue.
                    NAU_GPU_TRANSFER_SRC("dof_in", gpu_profiler::PrimaryGpu, gpu_profiler::SecondaryGpu, gpu_profiler::FrameToken,
                                         primaryTimestamps.id(), uint64_t{width} * height * inBytesPerPixel);
                    channels[0].copy(primaryList.Get(), 0, color, true);
                    channels[1].copy(primaryList.Get(), 0, depth, true);
                }
                primaryTimestamps.resolve();
                primaryTimestamps.setCommandList(nullptr);
                primaryInFlight = true;
                submitPrimary();
                primaryInFlight = false;
                // After the bridge's signal and wait: nothing in its order changes.
                primaryTimestamps.submitted();

                auto queue = secondary->GetCommandQueue(gfx::GQueueType::Graphics);
                check(queue->GetD3D12CommandQueue()->Wait(secondaryFence.Get(), serial));
                auto command = queue->GetCommandList();
                auto list = command->GetGraphicsCommandList();
                secondaryTimestamps.setCommandList(list.Get());
                {
                    // Destination half of dof_in, on the second GPU.
                    NAU_GPU_TRANSFER_DST("dof_in", gpu_profiler::PrimaryGpu, gpu_profiler::SecondaryGpu, gpu_profiler::FrameToken,
                                         secondaryTimestamps.id(), uint64_t{width} * height * inBytesPerPixel);
                    imageState(list.Get(), Color, D3D12_RESOURCE_STATE_COPY_DEST);
                    imageState(list.Get(), Depth, D3D12_RESOURCE_STATE_COPY_DEST);
                    channels[0].copy(list.Get(), 1, images[Color].texture.Get(), false);
                    channels[1].copy(list.Get(), 1, images[Depth].texture.Get(), false);
                }
                ID3D12DescriptorHeap* heaps[] = {descriptors.Get()};
                list->SetDescriptorHeaps(1, heaps);
                list->SetComputeRootSignature(root.Get());
                {
                    // GPU time of the DOF passes on the second GPU, without the copies from and to the shared buffers.
                    NAU_GPU_TIMER_SRC(secondaryTimestamps.id(), "dof_compute");
                    for (UINT i = 0; i < passCount; ++i)
                    {
                        const auto& pass = passes[i];
                        for (int input : pass.inputs)
                            if (input >= 0) imageState(list.Get(), static_cast<Image>(input), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                        imageState(list.Get(), pass.output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                        float constants[8] = {};
                        if (i < 2)
                        {
                            constants[0] = params.focusDistance; constants[1] = params.focusRange;
                            constants[2] = params.farZ; constants[3] = 0; constants[4] = params.maxCoC;
                        }
                        else if (pass.output == Wmax)
                        {
                            constants[0] = params.maxCoC; constants[1] = params.taps;
                        }
                        else if (pass.output == Filtered) constants[0] = params.filterRadius;
                        list->SetComputeRoot32BitConstants(0, 8, constants, 0);
                        list->SetComputeRootDescriptorTable(1, CD3DX12_GPU_DESCRIPTOR_HANDLE(
                            descriptors->GetGPUDescriptorHandleForHeapStart(), i * 4, descriptorSize));
                        list->SetComputeRootDescriptorTable(2, CD3DX12_GPU_DESCRIPTOR_HANDLE(
                            descriptors->GetGPUDescriptorHandleForHeapStart(), i * 4 + 3, descriptorSize));
                        list->SetPipelineState(pipelines[i].Get());
                        const auto& output = images[pass.output];
                        list->Dispatch((output.width + 7) / 8, (output.height + 7) / 8, 1);
                        auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(output.texture.Get());
                        list->ResourceBarrier(1, &barrier);
                    }
                }
                {
                    // Source half of dof_out, on the second GPU.
                    NAU_GPU_TRANSFER_SRC("dof_out", gpu_profiler::SecondaryGpu, gpu_profiler::PrimaryGpu, gpu_profiler::FrameToken,
                                         secondaryTimestamps.id(), uint64_t{width} * height * outBytesPerPixel);
                    imageState(list.Get(), Filled, D3D12_RESOURCE_STATE_COPY_SOURCE);
                    imageState(list.Get(), Filtered, D3D12_RESOURCE_STATE_COPY_SOURCE);
                    channels[2].copy(list.Get(), 1, images[Filled].texture.Get(), true);
                    channels[3].copy(list.Get(), 1, images[Filtered].texture.Get(), true);
                }
                secondaryTimestamps.resolve();
                secondaryTimestamps.setCommandList(nullptr);
                queue->ExecuteCommandList(command);
                check(queue->GetD3D12CommandQueue()->Signal(secondaryFence.Get(), ++serial));
                // After the bridge's signal: its fence order and its waits are unchanged.
                secondaryTimestamps.submitted();
                // Correctness baseline: complete GPU1 before allowing any write to Nau outputs.
                // A failed GPU1 therefore cannot race the single-GPU fallback.
                wait(primaryFence.Get(), serial, secondary->GetDXDevice().Get());

                check(primaryQueue->Wait(primaryFence.Get(), serial));
                beginPrimary();
                primaryTimestamps.setCommandList(primaryList.Get());
                {
                    // Destination half of dof_out, on Nau's queue.
                    NAU_GPU_TRANSFER_DST("dof_out", gpu_profiler::SecondaryGpu, gpu_profiler::PrimaryGpu, gpu_profiler::FrameToken,
                                         primaryTimestamps.id(), uint64_t{width} * height * outBytesPerPixel);
                    channels[2].copy(primaryList.Get(), 0, filled, false);
                    channels[3].copy(primaryList.Get(), 0, filtered, false);
                }
                primaryTimestamps.resolve();
                primaryTimestamps.setCommandList(nullptr);
                primaryInFlight = true;
                submitPrimary();
                primaryInFlight = false;
                primaryTimestamps.submitted();

                // Both results are used by this frame.
                gpu_profiler::GpuProfiler::instance().markTransferConsumed("dof_in");
                gpu_profiler::GpuProfiler::instance().markTransferConsumed("dof_out");
                return DofResult::Completed;
            }
            catch (const std::exception& exception)
            {
                // The list is abandoned: its timestamps never arrive and the profiler drops that frame.
                secondaryTimestamps.setCommandList(nullptr);
                primaryTimestamps.setCommandList(nullptr);
                failed = true;
                error = exception.what();
                return primaryInFlight || FAILED(primary->GetDeviceRemovedReason())
                    ? DofResult::PrimaryFailure : DofResult::Fallback;
            }
            catch (...)
            {
                // The team's DxException does not derive from std::exception.
                secondaryTimestamps.setCommandList(nullptr);
                primaryTimestamps.setCommandList(nullptr);
                failed = true;
                error = "Mgpu command submission failed (see D3D12 diagnostics)";
                return primaryInFlight || FAILED(primary->GetDeviceRemovedReason())
                    ? DofResult::PrimaryFailure : DofResult::Fallback;
            }
        }
    };

    DofBridge::DofBridge() : m_impl(std::make_unique<Impl>()) {}
    DofBridge::~DofBridge() = default;

    bool DofBridge::prepare(void* device, void* queue, uint32_t width, uint32_t height)
    {
        if (m_impl->failed) return false;
        try
        {
            if (!device || !queue || !width || !height || width > 16384 || height > 16384)
                throw std::runtime_error("invalid DOF device, queue or dimensions");
            if (!m_impl->primary) m_impl->initialize(static_cast<ID3D12Device*>(device), static_cast<ID3D12CommandQueue*>(queue));
            if (m_impl->primary.Get() != device) throw std::runtime_error("primary device changed; recreate DOF bridge");
            if (width != m_impl->width || height != m_impl->height) m_impl->resize(width, height);
            return true;
        }
        catch (const std::exception& error)
        {
            m_impl->failed = true;
            m_impl->error = error.what();
            return false;
        }
        catch (...)
        {
            m_impl->failed = true;
            m_impl->error = "Mgpu device initialization failed (see D3D12 diagnostics)";
            return false;
        }
    }

    DofResult DofBridge::execute(void* color, void* depth, void* filled, void* filtered, const DofParameters& params)
    {
        if (m_impl->failed || !m_impl->width) return DofResult::Fallback;
        return m_impl->execute(static_cast<ID3D12Resource*>(color), static_cast<ID3D12Resource*>(depth),
            static_cast<ID3D12Resource*>(filled), static_cast<ID3D12Resource*>(filtered), params);
    }

    const char* DofBridge::lastError() const { return m_impl->error.c_str(); }
    const char* DofBridge::secondaryAdapterName() const { return m_impl->adapterName.c_str(); }
}
