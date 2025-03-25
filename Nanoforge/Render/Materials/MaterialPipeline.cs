using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
using Serilog;
using Silk.NET.Core.Native;
using Silk.NET.Shaderc;
using Silk.NET.Vulkan;
using VkPrimitiveTopology = Silk.NET.Vulkan.PrimitiveTopology;

namespace Nanoforge.Render.Materials;

public class MaterialPipeline
{
    private RenderContext _context;
    public readonly string Name;
    private VkPrimitiveTopology _topology;
    private RenderPass _renderPass;
    private uint _stride;
    private VertexInputAttributeDescription[] _attributes;

    public DescriptorSetLayout DescriptorSetLayout;
    public PipelineLayout Layout;
    private Pipeline _graphicsPipeline;
    
    private DescriptorSet[]? _descriptorSets;

    private Shader? _vertexShader;
    private string VertexShaderPath => $"{BuildConfig.ShadersDirectory}{Name}.vert";
    
    private Shader? _fragmentShader;
    private string FragmentShaderPath => $"{BuildConfig.ShadersDirectory}{Name}.frag";

    private bool _disableFaceCulling;
    
    private DescriptorAllocator? _descriptorAllocator;
    
    private VkBuffer[]? _uniformBuffers;
    private VkBuffer[]? _perObjectConstantBuffers;
    private VkBuffer[]? _materialInfoBuffers;
    
    public MaterialPipeline(RenderContext context, string name, RenderPass renderPass, VkPrimitiveTopology topology, uint stride, Span<VertexInputAttributeDescription> attributes, 
        VkBuffer[] uniformBuffers, VkBuffer[] perObjectConstantBuffers, VkBuffer[] materialInfoBuffers, bool disableFaceCulling = false)
    {
        _context = context;
        Name = name;
        _topology = topology;
        _renderPass = renderPass;
        _stride = stride;
        _attributes = attributes.ToArray();
        _uniformBuffers = uniformBuffers;
        _perObjectConstantBuffers = perObjectConstantBuffers;
        _materialInfoBuffers = materialInfoBuffers;
        _disableFaceCulling = disableFaceCulling;
        
        Init();
        
        //TODO: Do sanity check on stride to make sure it matches the attributes total size
        //TODO: Also check that the offsets of the attributes make sense and they don't overlap each other. That is not something that RFG meshes do afaik
        //TODO: Will need function to get size of vulkan formats in bytes. Just implement for the used formats. If format is not used just don't bother.
    }
    
    [MemberNotNull(nameof(_descriptorSets))]
    private void Init()
    {
        DescriptorAllocator.PoolSizeRatio[] poolSizeRatios =
        [
            new() { Type = DescriptorType.UniformBuffer,        Ratio = 1.0f },
            new() { Type = DescriptorType.CombinedImageSampler, Ratio = 8192.0f },
            new() { Type = DescriptorType.StorageBuffer       , Ratio = 1.0f },
        ];
        _descriptorAllocator = new DescriptorAllocator(_context, 10, poolSizeRatios);
        
        CreateShaders();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        UpdateDescriptorSets();
    }

    private void CreateShaders()
    {
        string vertexShaderPath = VertexShaderPath;
        string fragmentShaderPath = FragmentShaderPath;
        _vertexShader = new Shader(_context, vertexShaderPath, ShaderKind.VertexShader, optimize: true);
        _fragmentShader = new Shader(_context, fragmentShaderPath, ShaderKind.FragmentShader, optimize: true);
    }
    
    public VertexInputBindingDescription GetVertexBindingDescription()
    {
        VertexInputBindingDescription bindingDescription = new()
        {
            Binding = 0,
            Stride = _stride,
            InputRate = VertexInputRate.Vertex,
        };

        return bindingDescription;
    }

    private unsafe void CreateDescriptorSetLayout()
    {
        List<DescriptorSetLayoutBinding> bindings = new List<DescriptorSetLayoutBinding>();
        
        //Per frame uniform buffer
        bindings.Add(new()
        {
            Binding = 0,
            DescriptorCount = 1,
            DescriptorType = DescriptorType.UniformBuffer,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.VertexBit | ShaderStageFlags.FragmentBit,
        });
        
        //Binding for the per object constants buffer
        bindings.Add(new()
        {
            Binding = 1,
            DescriptorCount = 1,
            DescriptorType = DescriptorType.StorageBuffer,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.VertexBit | ShaderStageFlags.FragmentBit,
        });
        
        //Material info buffer
        bindings.Add(new()
        {
            Binding = 2,
            DescriptorCount = 1,
            DescriptorType = DescriptorType.StorageBuffer,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.VertexBit | ShaderStageFlags.FragmentBit,
        });

        //Big texture array that holds all the textures used by shaders
        bindings.Add(new()
        {
            Binding = 3,
            DescriptorCount = TextureManager.MaxTextures,
            DescriptorType = DescriptorType.CombinedImageSampler,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.FragmentBit,
        });

        DescriptorSetLayoutBinding[] bindingsArray = bindings.ToArray();

        fixed (DescriptorSetLayoutBinding* bindingsPtr = bindingsArray)
        fixed (DescriptorSetLayout* descriptorSetLayoutPtr = &DescriptorSetLayout)
        {
            DescriptorSetLayoutCreateInfo layoutInfo = new()
            {
                SType = StructureType.DescriptorSetLayoutCreateInfo,
                BindingCount = (uint)bindingsArray.Length,
                PBindings = bindingsPtr,
            };

            if (_context.Vk.CreateDescriptorSetLayout(_context.Device, in layoutInfo, null, descriptorSetLayoutPtr) != Result.Success)
            {
                throw new Exception("Failed to create descriptor set layout!");
            }
        }
    }

    private unsafe void CreateGraphicsPipeline()
    {
        PipelineShaderStageCreateInfo vertShaderStageInfo = _vertexShader!.GetPipelineStageInfo();
        PipelineShaderStageCreateInfo fragShaderStageInfo = _fragmentShader!.GetPipelineStageInfo();
        PipelineShaderStageCreateInfo* shaderStages = stackalloc[]
        {
            vertShaderStageInfo,
            fragShaderStageInfo
        };

        var bindingDescription = GetVertexBindingDescription();
        var attributeDescriptions = _attributes;

        fixed (VertexInputAttributeDescription* attributeDescriptionsPtr = attributeDescriptions)
        fixed (DescriptorSetLayout* descriptorSetLayoutPtr = &DescriptorSetLayout)
        {
            PipelineVertexInputStateCreateInfo vertexInputInfo = new()
            {
                SType = StructureType.PipelineVertexInputStateCreateInfo,
                VertexBindingDescriptionCount = 1,
                VertexAttributeDescriptionCount = (uint)attributeDescriptions.Length,
                PVertexBindingDescriptions = &bindingDescription,
                PVertexAttributeDescriptions = attributeDescriptionsPtr,
            };

            PipelineInputAssemblyStateCreateInfo inputAssembly = new()
            {
                SType = StructureType.PipelineInputAssemblyStateCreateInfo,
                Topology = _topology,
                PrimitiveRestartEnable = false,
            };

            //Viewport and scissor get set each frame via dynamic state. Much more convenient to code around since you don't need to recreate your pipelines if the swapchain resizes
            DynamicState* dynamicStates = stackalloc[]
            {
                DynamicState.Viewport,
                DynamicState.Scissor,
            };
            PipelineDynamicStateCreateInfo dynamicStateCreateInfo = new()
            {
                SType = StructureType.PipelineDynamicStateCreateInfo,
                DynamicStateCount = 2,
                PDynamicStates = dynamicStates
            };

            PipelineViewportStateCreateInfo viewportState = new()
            {
                SType = StructureType.PipelineViewportStateCreateInfo,
                ViewportCount = 1,
                PViewports = null,
                ScissorCount = 1,
                PScissors = null,
            };

            PipelineRasterizationStateCreateInfo rasterizer = new()
            {
                SType = StructureType.PipelineRasterizationStateCreateInfo,
                DepthClampEnable = false,
                RasterizerDiscardEnable = false,
                PolygonMode = PolygonMode.Fill,
                LineWidth = 1,
                CullMode = _disableFaceCulling ? CullModeFlags.None : CullModeFlags.BackBit,
                FrontFace = FrontFace.CounterClockwise,
                DepthBiasEnable = false,
            };

            PipelineMultisampleStateCreateInfo multisampling = new()
            {
                SType = StructureType.PipelineMultisampleStateCreateInfo,
                SampleShadingEnable = false,
                RasterizationSamples = SampleCountFlags.Count1Bit,
            };

            PipelineDepthStencilStateCreateInfo depthStencil = new()
            {
                SType = StructureType.PipelineDepthStencilStateCreateInfo,
                DepthTestEnable = true,
                DepthWriteEnable = true,
                DepthCompareOp = CompareOp.Less,
                DepthBoundsTestEnable = false,
                StencilTestEnable = false,
            };

            PipelineColorBlendAttachmentState colorBlendAttachment = new()
            {
                ColorWriteMask = ColorComponentFlags.RBit | ColorComponentFlags.GBit | ColorComponentFlags.BBit | ColorComponentFlags.ABit,
                BlendEnable = false,
            };

            PipelineColorBlendStateCreateInfo colorBlending = new()
            {
                SType = StructureType.PipelineColorBlendStateCreateInfo,
                LogicOpEnable = false,
                LogicOp = LogicOp.Copy,
                AttachmentCount = 1,
                PAttachments = &colorBlendAttachment,
            };
            colorBlending.BlendConstants[0] = 0;
            colorBlending.BlendConstants[1] = 0;
            colorBlending.BlendConstants[2] = 0;
            colorBlending.BlendConstants[3] = 0;
            
            PipelineLayoutCreateInfo pipelineLayoutInfo = new()
            {
                SType = StructureType.PipelineLayoutCreateInfo,
                SetLayoutCount = 1,
                PSetLayouts = descriptorSetLayoutPtr
            };

            if (_context.Vk.CreatePipelineLayout(_context.Device, in pipelineLayoutInfo, null, out Layout) != Result.Success)
            {
                throw new Exception("Failed to create pipeline layout!");
            }

            GraphicsPipelineCreateInfo pipelineInfo = new()
            {
                SType = StructureType.GraphicsPipelineCreateInfo,
                StageCount = 2,
                PStages = shaderStages,
                PVertexInputState = &vertexInputInfo,
                PInputAssemblyState = &inputAssembly,
                PViewportState = &viewportState,
                PRasterizationState = &rasterizer,
                PMultisampleState = &multisampling,
                PDepthStencilState = &depthStencil,
                PColorBlendState = &colorBlending,
                Layout = Layout,
                RenderPass = _renderPass,
                Subpass = 0,
                BasePipelineHandle = default,
                PDynamicState = &dynamicStateCreateInfo
            };

            if (_context.Vk.CreateGraphicsPipelines(_context.Device, default, 1, in pipelineInfo, null, out _graphicsPipeline) != Result.Success)
            {
                throw new Exception("Failed to create graphics pipeline!");
            }
        }

        SilkMarshal.Free((nint)vertShaderStageInfo.PName);
        SilkMarshal.Free((nint)fragShaderStageInfo.PName);
    }
    
    [MemberNotNull(nameof(_descriptorSets))]
    public unsafe void UpdateDescriptorSets()
    {
        if (_descriptorSets is null)
        {
            var layouts = new DescriptorSetLayout[Renderer.MaxFramesInFlight];
            Array.Fill(layouts, DescriptorSetLayout);
            
            DescriptorSet[] descriptorSets = new DescriptorSet[Renderer.MaxFramesInFlight];
            for (int i = 0; i < descriptorSets.Length; i++)
            {
                descriptorSets[i] = _descriptorAllocator!.Allocate(layouts[i]);
            }
            _descriptorSets = descriptorSets;
        }

        for (int i = 0; i < Renderer.MaxFramesInFlight; i++)
        {
            //Per frame uniform buffer
            DescriptorBufferInfo perFrameConstantsBuffer = new()
            {
                Buffer = _uniformBuffers![i].VkHandle,
                Offset = 0,
                Range = (ulong)Unsafe.SizeOf<PerFrameBuffer>(),
            };
            
            //Per object constant storage buffer
            DescriptorBufferInfo perObjectConstantsBuffer = new()
            {
                Buffer = _perObjectConstantBuffers![i].VkHandle,
                Offset = 0,
                Range = _perObjectConstantBuffers![i].Size
            };
            
            //Material info buffer
            DescriptorBufferInfo materialInfoBuffer = new()
            {
                Buffer = _materialInfoBuffers![i].VkHandle,
                Offset = 0,
                Range = _materialInfoBuffers![i].Size
            };

            DescriptorImageInfo[] imageInfos = new DescriptorImageInfo[TextureManager.MaxTextures];
            for (var j = 0; j < TextureManager.TextureSlots.Length; j++)
            {
                Texture2D texture;
                TextureManager.TextureSlot slot = TextureManager.TextureSlots[j];
                if (slot.InUse)
                    texture = slot.Texture!;
                else
                    texture = Texture2D.DefaultTexture;

                imageInfos[j] = new DescriptorImageInfo
                {
                    ImageLayout = ImageLayout.ShaderReadOnlyOptimal,
                    ImageView = texture.ImageViewHandle,
                    Sampler = Texture2D.DefaultSampler,
                };
            }

            fixed (DescriptorImageInfo* imageInfosPtr = imageInfos)
            {
                List<WriteDescriptorSet> descriptorWrites = new();

                //Per frame uniform buffer
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = _descriptorSets[i],
                    DstBinding = 0,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.UniformBuffer,
                    DescriptorCount = 1,
                    PBufferInfo = &perFrameConstantsBuffer,
                });
                
                //Per object constants buffer
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = _descriptorSets[i],
                    DstBinding = 1,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.StorageBuffer,
                    DescriptorCount = 1,
                    PBufferInfo = &perObjectConstantsBuffer,
                });
                
                //Material info buffer
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = _descriptorSets[i],
                    DstBinding = 2,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.StorageBuffer,
                    DescriptorCount = 1,
                    PBufferInfo = &materialInfoBuffer,
                });
                
                //Big texture array                
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = _descriptorSets[i],
                    DstBinding = 3,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.CombinedImageSampler,
                    DescriptorCount = TextureManager.MaxTextures,
                    PImageInfo = imageInfosPtr,
                });

                var descriptorWritesArray = descriptorWrites.ToArray();
                fixed (WriteDescriptorSet* descriptorWritesPtr = descriptorWritesArray)
                {
                    _context!.Vk.UpdateDescriptorSets(_context.Device, (uint)descriptorWritesArray.Length, descriptorWritesPtr, 0, null);
                }
            }
        }
    }

    public void ReloadEditedShaders()
    {
        if (_vertexShader is { SourceFilesEdited: true } || _fragmentShader is { SourceFilesEdited: true })    
        {
            Log.Information($"Reloading shaders for {Name}");
            Thread.Sleep(250); //Wait a moment to make sure the shader isn't being saved by another process while we're loading it. Stupid fix, but it works.
            _context.Vk.QueueWaitIdle(_context.GraphicsQueue); //Wait for graphics queue so we know the pipeline isn't in use when we destroy it
            Destroy();
            Init();
        }
    }
    
    public unsafe void Bind(CommandBuffer commandBuffer, int frameIndex)
    {
        _context.Vk.CmdBindPipeline(commandBuffer, PipelineBindPoint.Graphics, _graphicsPipeline);
        _context.Vk.CmdBindDescriptorSets(commandBuffer, PipelineBindPoint.Graphics, Layout, 0, 1, in _descriptorSets![frameIndex], 0, null);
    }

    public unsafe void Destroy()
    {
        if (_descriptorAllocator != null)
        {
            _descriptorAllocator.Destroy();
            _descriptorAllocator = null;
        }
        _descriptorSets = null;
        
        _context.Vk.DestroyPipeline(_context.Device, _graphicsPipeline, null);
        _context.Vk.DestroyPipelineLayout(_context.Device, Layout, null);
        
        _context.Vk.DestroyDescriptorSetLayout(_context.Device, DescriptorSetLayout, null);
        _vertexShader!.Destroy();
        _fragmentShader!.Destroy();
    }
}