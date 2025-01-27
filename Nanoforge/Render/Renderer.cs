using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Materials;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
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
    
    public static readonly Format RenderTextureImageFormat = Format.B8G8R8A8Srgb;
    public static Format DepthTextureFormat { get; private set; }

    public Renderer(uint viewportWidth, uint viewportHeight)
    {
        Context = new RenderContext();
        DepthTextureFormat = FindDepthFormat();
        CreateRenderPass();
        CreateUniformBuffers();
        MaterialHelper.Init(Context, _renderPass, _perFrameUniformBuffers!);
        CreateSyncObjects();
        
        //Load 1x1 white texture to use when the maximum number of textures aren't provided to a RenderObject
        Texture2D.LoadDefaultTexture(Context, Context.CommandPool, Context.GraphicsQueue);
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

        if (_perFrameUniformBuffers != null)
        {
            foreach (VkBuffer buffer in _perFrameUniformBuffers)
            {
                buffer.Destroy();
            }
        }
        
        Texture2D.DefaultTexture.Destroy();

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
            Format = FindDepthFormat(),
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
        };

        _perFrameUniformBuffers![currentImage].SetData(ref perFrame);
    }

    private void RecordCommands(Scene scene, uint index)
    {
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

        //Sort render objects by material so each material pipeline is only bound once per frame at most
        Dictionary<string, List<RenderObject>> renderObjectsByMaterial = new();
        foreach (RenderObject renderObject in scene.RenderObjects)
        {
            if (!renderObjectsByMaterial.ContainsKey(renderObject.Material.Name))
            {
                renderObjectsByMaterial.Add(renderObject.Material.Name, new List<RenderObject>());
            }

            renderObjectsByMaterial[renderObject.Material.Name].Add(renderObject);
        }

        UpdatePerFrameUniformBuffer(scene, index);
        foreach (List<RenderObject> materialGroup in renderObjectsByMaterial.Values)
        {
            if (materialGroup.Count == 0)
                continue;

            MaterialPipeline pipeline = materialGroup.First().Material.Pipeline;
            pipeline.Bind(commandBuffer);

            foreach (RenderObject renderObject in materialGroup)
            {
                Matrix4x4 translation = Matrix4x4.CreateTranslation(renderObject.Position);
                Matrix4x4 rotation = renderObject.Orient;
                Matrix4x4 scale = Matrix4x4.CreateScale(renderObject.Scale);
                Matrix4x4 model = rotation * translation * scale;
                PerObjectPushConstants pushConstants = new()
                {
                    Model = model
                };
                Context.Vk.CmdPushConstants(commandBuffer, pipeline.Layout, ShaderStageFlags.VertexBit, 0, (uint)Unsafe.SizeOf<PerObjectPushConstants>(), &model);

                renderObject.Draw(Context, commandBuffer, index);
            }
        }

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