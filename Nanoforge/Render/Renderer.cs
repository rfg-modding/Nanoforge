using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
using Nanoforge.Render.VertexFormats;
using Nanoforge.Render.VertexFormats.Rfg;
using Serilog;
using Silk.NET.Vulkan;
using Semaphore = Silk.NET.Vulkan.Semaphore;

//Note: Parts of this renderer were originally based on this repo and these sites:
//https://github.com/dfkeenan/SilkVulkanTutorial/
//https://github.com/JensKrumsieck/raytracing-vulkan/blob/dcba9d4c091f05585fbb99a9ce57603823405e31/LICENSE
//https://vulkan-tutorial.com/Introduction
//https://vkguide.dev/

namespace Nanoforge.Render;

public unsafe class Renderer
{
    public RenderContext Context;

    public const int MaxFramesInFlight = 2;

    public RenderPass _renderPass;
    
    private VkBuffer[]? _perFrameUniformBuffers;
    
    private Semaphore[]? _imageAvailableSemaphores;
    private Semaphore[]? _renderFinishedSemaphores;
    private Fence[]? _inFlightFences;
    private int _currentFrame = 0;
    public List<Scene> ActiveScenes = new();

    //Create a GPU buffer for storing per object constants like their model matrix. Gets uploaded once per scene instead of updating push constants 1000s of times
    //25000 is a reasonable cap with tons of headroom. mp_crescent is a large map and only has 4814. With the current size of PerObjectConstants it only takes up 1.4MB.
    //Note that this number is equivalent to the number of RenderObjectBase instances. Since some types like RenderChunk can have many separate pieces with their own constants.
    public const int MaxObjectsPerScene = 150000;
    public const int MaxMaterialInstances = 100000;
    private VkBuffer[] _perObjectConstantsGPUBuffer; 
    private VkBuffer[] _materialInfoGPUBuffer; 
    private GpuFrameDataWriter _gpuFrameDataWriter = new(MaxObjectsPerScene, MaxMaterialInstances);
    
    public static readonly Format RenderTextureImageFormat = Format.B8G8R8A8Srgb;
    public static Format DepthTextureFormat { get; private set; }

    private List<RenderCommand> _commands = new();
    
    public unsafe Renderer(uint viewportWidth, uint viewportHeight)
    {
        if (Unsafe.SizeOf<LineVertex>() != 20)
        {
            throw new Exception("SizeOf<LineVertex>() != 20. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<ColoredVertex>() != 16)
        {
            throw new Exception("SizeOf<ColoredVertex>() != 16. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<PerObjectConstants>() != 96)
        {
            throw new Exception("SizeOf<PerObjectConstants>() != 96. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<MaterialInstance>() != 48)
        {
            throw new Exception("SizeOf<MaterialInstance>() != 48. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<PerFrameBuffer>() != 144)
        {
            throw new Exception("SizeOf<PerFrameBuffer>() != 144. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<UnifiedVertex>() != 24)
        {
            throw new Exception("SizeOf<UnifiedVertex>() != 24. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<HighLodTerrainVertex>() != 8)
        {
            throw new Exception("SizeOf<HighLodTerrainVertex>() != 8. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<LowLodTerrainVertex>() != 8)
        {
            throw new Exception("SizeOf<LowLodTerrainVertex>() != 8. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<PixlitVertex>() != 16)
        {
            throw new Exception("SizeOf<PixlitVertex>() != 16. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<Pixlit1UvVertex>() != 20)
        {
            throw new Exception("SizeOf<Pixlit1UvVertex>() != 20. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<Pixlit1UvNmapVertex>() != 24)
        {
            throw new Exception("SizeOf<Pixlit1UvNmapVertex>() != 24. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<Pixlit2UvNmapVertex>() != 28)
        {
            throw new Exception("SizeOf<Pixlit2UvNmapVertex>() != 28. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<Pixlit3UvNmapVertex>() != 32)
        {
            throw new Exception("SizeOf<Pixlit3UvNmapVertex>() != 32. Required for shaders to work.");
        }
        if (Unsafe.SizeOf<Pixlit4UvNmapVertex>() != 36)
        {
            throw new Exception("SizeOf<Pixlit4UvNmapVertex>() != 36. Required for shaders to work.");
        }
        
        Context = new RenderContext();
        //Load global textures like 1x1 white texture & setup default texture sampler
        Texture2D.SetupGlobals(Context, Context.CommandPool, Context.GraphicsQueue);
        DepthTextureFormat = FindDepthFormat();
        CreateRenderPass();
        CreateUniformBuffers();
        CreatePerObjectConstantBuffers();
        CreateMaterialInfoBuffers();
        MaterialHelper.Init(Context, _renderPass, _perFrameUniformBuffers!, _perObjectConstantsGPUBuffer, _materialInfoGPUBuffer);
        CreateSyncObjects();
    }

    public void Shutdown()
    {
        Context.Vk.DeviceWaitIdle(Context.Device);
        Cleanup();
    }

    public void RenderFrame(Scene scene)
    {
        MaterialHelper.ReloadEditedShaders();

        Context.Vk.WaitForFences(Context.Device, 1, in _inFlightFences![_currentFrame], true, ulong.MaxValue);

        SubmitInfo submitInfo = new()
        {
            SType = StructureType.SubmitInfo,
        };

        var waitStages = stackalloc[] { PipelineStageFlags.ColorAttachmentOutputBit };

        var buffer = scene.CommandBuffers![_currentFrame];

        submitInfo = submitInfo with
        {
            WaitSemaphoreCount = 0,
            PWaitSemaphores = null,
            PWaitDstStageMask = waitStages,

            CommandBufferCount = 1,
            PCommandBuffers = &buffer
        };

        Context.Vk.ResetFences(Context.Device, 1, in _inFlightFences[_currentFrame]);

        RecordCommands(scene, (uint)_currentFrame);

        if (Context.Vk.QueueSubmit(Context.GraphicsQueue, 1, in submitInfo, _inFlightFences[_currentFrame]) != Result.Success)
        {
            throw new Exception("failed to submit draw command buffer!");
        }

        scene.LastFrame = _currentFrame;
        _currentFrame = (_currentFrame + 1) % MaxFramesInFlight;
    }

    public void Cleanup()
    {
        foreach (Scene scene in ActiveScenes)
        {
            scene.Destroy();
        }

        int numRemainingTextures = TextureManager.TextureSlots.Count(slot => slot is { NeverDestroy: false, InUse: true });
        if (numRemainingTextures > 0)
        {
            Log.Warning($"There are {numRemainingTextures} textures that haven't been destroyed.");
        }
        
        if (_perFrameUniformBuffers != null)
        {
            foreach (VkBuffer buffer in _perFrameUniformBuffers)
            {
                buffer.Destroy();
            }
        }

        foreach (VkBuffer buffer in _perObjectConstantsGPUBuffer)
        {
            buffer.Destroy();
        }

        Texture2D.CleanupGlobals(Context);
        Context.Vk.DestroyRenderPass(Context.Device, Context.PrimaryRenderPass, null);
        MaterialHelper.Destroy();

        for (int i = 0; i < MaxFramesInFlight; i++)
        {
            Context.Vk.DestroySemaphore(Context.Device, _renderFinishedSemaphores![i], null);
            Context.Vk.DestroySemaphore(Context.Device, _imageAvailableSemaphores![i], null);
            Context.Vk.DestroyFence(Context.Device, _inFlightFences![i], null);
        }

        Context.Dispose();
    }
    
    private void CreateRenderPass()
    {
        AttachmentDescription colorAttachment = new()
        {
            Format = RenderTextureImageFormat,
            Samples = SampleCountFlags.Count1Bit,
            LoadOp = AttachmentLoadOp.Clear,
            StoreOp = AttachmentStoreOp.Store,
            StencilLoadOp = AttachmentLoadOp.DontCare,
            InitialLayout = ImageLayout.Undefined,
            FinalLayout = ImageLayout.General,
        };

        AttachmentDescription depthAttachment = new()
        {
            Format = DepthTextureFormat,
            Samples = SampleCountFlags.Count1Bit,
            LoadOp = AttachmentLoadOp.Clear,
            StoreOp = AttachmentStoreOp.DontCare,
            StencilLoadOp = AttachmentLoadOp.DontCare,
            StencilStoreOp = AttachmentStoreOp.DontCare,
            InitialLayout = ImageLayout.Undefined,
            FinalLayout = ImageLayout.DepthStencilAttachmentOptimal,
        };

        AttachmentReference colorAttachmentRef = new()
        {
            Attachment = 0,
            Layout = ImageLayout.ColorAttachmentOptimal,
        };

        AttachmentReference depthAttachmentRef = new()
        {
            Attachment = 1,
            Layout = ImageLayout.DepthStencilAttachmentOptimal,
        };

        SubpassDescription subpass = new()
        {
            PipelineBindPoint = PipelineBindPoint.Graphics,
            ColorAttachmentCount = 1,
            PColorAttachments = &colorAttachmentRef,
            PDepthStencilAttachment = &depthAttachmentRef,
        };

        SubpassDependency dependency = new()
        {
            SrcSubpass = Vk.SubpassExternal,
            DstSubpass = 0,
            SrcStageMask = PipelineStageFlags.ColorAttachmentOutputBit | PipelineStageFlags.EarlyFragmentTestsBit,
            SrcAccessMask = 0,
            DstStageMask = PipelineStageFlags.ColorAttachmentOutputBit | PipelineStageFlags.EarlyFragmentTestsBit,
            DstAccessMask = AccessFlags.ColorAttachmentWriteBit | AccessFlags.DepthStencilAttachmentWriteBit
        };

        var attachments = new[] { colorAttachment, depthAttachment };

        fixed (AttachmentDescription* attachmentsPtr = attachments)
        {
            RenderPassCreateInfo renderPassInfo = new()
            {
                SType = StructureType.RenderPassCreateInfo,
                AttachmentCount = (uint)attachments.Length,
                PAttachments = attachmentsPtr,
                SubpassCount = 1,
                PSubpasses = &subpass,
                DependencyCount = 1,
                PDependencies = &dependency,
            };

            if (Context.Vk.CreateRenderPass(Context.Device, in renderPassInfo, null, out _renderPass) != Result.Success)
            {
                throw new Exception("Failed to create render pass!");
            }
        }
        
        Context.PrimaryRenderPass = _renderPass;
    }

    private void CreateUniformBuffers()
    {
        ulong uniformBufferSize = (ulong)Unsafe.SizeOf<PerFrameBuffer>();
        _perFrameUniformBuffers = new VkBuffer[MaxFramesInFlight];
        for (int i = 0; i < MaxFramesInFlight; i++)
        {
            _perFrameUniformBuffers[i] = new VkBuffer(Context, uniformBufferSize, BufferUsageFlags.UniformBufferBit,
                MemoryPropertyFlags.HostVisibleBit | MemoryPropertyFlags.HostCoherentBit);
        }
    }
    
    private void UpdatePerFrameUniformBuffer(Scene scene, uint currentImage)
    {
        PerFrameBuffer perFrame = new()
        {
            View = scene.Camera!.View,
            Projection = scene.Camera!.Projection,
            CameraPosition = new Vector4(scene.Camera!.Position.X, scene.Camera!.Position.Y, scene.Camera!.Position.Z, 1.0f),
        };

        _perFrameUniformBuffers![currentImage].SetData(ref perFrame);
    }

    [MemberNotNull(nameof(_perObjectConstantsGPUBuffer))]
    private void CreatePerObjectConstantBuffers()
    {
        int bufferSize = MaxObjectsPerScene * Unsafe.SizeOf<PerObjectConstants>();
        _perObjectConstantsGPUBuffer = new VkBuffer[MaxFramesInFlight];
        for (int i = 0; i < MaxFramesInFlight; i++)
        {
            _perObjectConstantsGPUBuffer[i] = new VkBuffer(Context, (ulong)bufferSize, BufferUsageFlags.StorageBufferBit,
                MemoryPropertyFlags.HostVisibleBit | MemoryPropertyFlags.HostCoherentBit);
        }
    }
    
    [MemberNotNull(nameof(_materialInfoGPUBuffer))]
    private void CreateMaterialInfoBuffers()
    {
        int bufferSize = MaxMaterialInstances * Unsafe.SizeOf<MaterialInstance>();
        _materialInfoGPUBuffer = new VkBuffer[MaxFramesInFlight];
        for (int i = 0; i < MaxFramesInFlight; i++)
        {
            _materialInfoGPUBuffer[i] = new VkBuffer(Context, (ulong)bufferSize, BufferUsageFlags.StorageBufferBit,
                MemoryPropertyFlags.HostVisibleBit | MemoryPropertyFlags.HostCoherentBit);
        }
    }

    private void UpdatePerObjectConstantBuffers(uint currentImage)
    {
        fixed (PerObjectConstants* bufferPtr = _gpuFrameDataWriter.Constants)
        {
            int size = MaxObjectsPerScene * Unsafe.SizeOf<PerObjectConstants>();
            Span<byte> data = new((byte*)bufferPtr, size);
            _perObjectConstantsGPUBuffer![currentImage].SetData(data);
        }
    }
    
    private void UpdateMaterialInfoBuffers(uint currentImage)
    {
        fixed (MaterialInstance* bufferPtr = _gpuFrameDataWriter.Materials)
        {
            int size = MaxMaterialInstances * Unsafe.SizeOf<MaterialInstance>();
            Span<byte> data = new((byte*)bufferPtr, size);
            _materialInfoGPUBuffer![currentImage].SetData(data);
        }
    }

    private void RecordCommands(Scene scene, uint index)
    {
        _commands.Clear();
        CommandBuffer commandBuffer = scene.CommandBuffers![index];

        CommandBufferBeginInfo beginInfo = new()
        {
            SType = StructureType.CommandBufferBeginInfo,
        };

        if (Context.Vk.ResetCommandBuffer(commandBuffer, CommandBufferResetFlags.None) != Result.Success)
        {
            throw new Exception("Failed to reset command buffer before recording render commands!");
        }

        if (Context.Vk.BeginCommandBuffer(commandBuffer, in beginInfo) != Result.Success)
        {
            throw new Exception("Failed to begin recording command buffer!");
        }

        Viewport viewport = new()
        {
            X = 0,
            Y = 0,
            Width = scene.ViewportWidth,
            Height = scene.ViewportHeight,
            MinDepth = 0,
            MaxDepth = 1,
        };
        Context.Vk.CmdSetViewport(commandBuffer, 0, 1, &viewport);

        Rect2D scissor = new()
        {
            Offset = { X = 0, Y = 0 },
            Extent = new Extent2D(scene.ViewportWidth, scene.ViewportHeight),
        };
        Context.Vk.CmdSetScissor(commandBuffer, 0, 1, &scissor);

        RenderPassBeginInfo renderPassInfo = new()
        {
            SType = StructureType.RenderPassBeginInfo,
            RenderPass = _renderPass,
            Framebuffer = scene.SwapChainFramebuffers![index],
            RenderArea =
            {
                Offset = { X = 0, Y = 0 },
                Extent = new Extent2D(scene.ViewportWidth, scene.ViewportHeight),
            }
        };

        var clearValues = new ClearValue[]
        {
            new()
            {
                Color = new() { Float32_0 = 0, Float32_1 = 0, Float32_2 = 0, Float32_3 = 1 },
            },
            new()
            {
                DepthStencil = new() { Depth = 1, Stencil = 0 }
            }
        };

        fixed (ClearValue* clearValuesPtr = clearValues)
        {
            renderPassInfo.ClearValueCount = (uint)clearValues.Length;
            renderPassInfo.PClearValues = clearValuesPtr;

            Context.Vk.CmdBeginRenderPass(commandBuffer, &renderPassInfo, SubpassContents.Inline);
        }

        UpdatePerFrameUniformBuffer(scene, index);

        if (TextureManager.DescriptorSetsNeedUpdate)
        {
            MaterialHelper.UpdateTextureArrayDescriptors();
            TextureManager.DescriptorSetsNeedUpdate = false;
        }

        _gpuFrameDataWriter.Reset();
        
        foreach (RenderObjectBase renderObject in scene.RenderObjects)
        {
            renderObject.WriteDrawCommands(_commands, scene.Camera!, _gpuFrameDataWriter);
        }
        
        UpdatePerObjectConstantBuffers(index);
        UpdateMaterialInfoBuffers(index);
        
        MaterialPipeline unifiedPipeline = MaterialHelper.GetMaterialPipeline("UnifiedMaterial") ?? throw new NullReferenceException("Failed to get unified material pipeline!");
        unifiedPipeline.Bind(commandBuffer, (int)index);
        foreach (IGrouping<Mesh, RenderCommand> meshGrouping in _commands.GroupBy(command => command.Mesh))
        {
            Mesh mesh = meshGrouping.Key;
            mesh.Bind(Context, commandBuffer);

            foreach (RenderCommand command in meshGrouping)
            {
                Context.Vk.CmdDrawIndexed(commandBuffer, command.IndexCount, 1, command.StartIndex, 0, command.ObjectIndex);
            }
        }
        
        scene.PrimitiveRenderer.RenderPrimitives(Context, commandBuffer, index);

        Context.Vk.CmdEndRenderPass(commandBuffer);

        if (Context.Vk.EndCommandBuffer(commandBuffer) != Result.Success)
        {
            throw new Exception("Failed to record command buffer!");
        }
    }

    private void CreateSyncObjects()
    {
        _imageAvailableSemaphores = new Semaphore[MaxFramesInFlight];
        _renderFinishedSemaphores = new Semaphore[MaxFramesInFlight];
        _inFlightFences = new Fence[MaxFramesInFlight];

        SemaphoreCreateInfo semaphoreInfo = new()
        {
            SType = StructureType.SemaphoreCreateInfo,
        };

        FenceCreateInfo fenceInfo = new()
        {
            SType = StructureType.FenceCreateInfo,
            Flags = FenceCreateFlags.SignaledBit,
        };

        for (var i = 0; i < MaxFramesInFlight; i++)
        {
            if (Context.Vk.CreateSemaphore(Context.Device, in semaphoreInfo, null, out _imageAvailableSemaphores[i]) != Result.Success ||
                Context.Vk.CreateSemaphore(Context.Device, in semaphoreInfo, null, out _renderFinishedSemaphores[i]) != Result.Success ||
                Context.Vk.CreateFence(Context.Device, in fenceInfo, null, out _inFlightFences[i]) != Result.Success)
            {
                throw new Exception("Failed to create synchronization objects for a frame!");
            }
        }
    }
    
    private Format FindSupportedFormat(IEnumerable<Format> candidates, ImageTiling tiling, FormatFeatureFlags features)
    {
        foreach (var format in candidates)
        {
            Context!.Vk.GetPhysicalDeviceFormatProperties(Context.PhysicalDevice, format, out var props);

            if (tiling == ImageTiling.Linear && (props.LinearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == ImageTiling.Optimal && (props.OptimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        throw new Exception("Failed to find supported format!");
    }

    private Format FindDepthFormat()
    {
        return FindSupportedFormat(new[] { Format.D32Sfloat, Format.D32SfloatS8Uint, Format.D24UnormS8Uint }, ImageTiling.Optimal, FormatFeatureFlags.DepthStencilAttachmentBit);
    }
}