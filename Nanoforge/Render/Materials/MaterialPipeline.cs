using System;
using System.IO;
using System.Runtime.CompilerServices;
using System.Threading;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
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

    private Shader? _vertexShader;
    private string VertexShaderPath => $"{BuildConfig.ShadersDirectory}{Name}.vert";
    private DateTime _vertexShaderLastWriteTime;
    
    private Shader? _fragmentShader;
    private string FragmentShaderPath => $"{BuildConfig.ShadersDirectory}{Name}.frag";
    private DateTime _fragmentShaderLastWriteTime;
    
    public MaterialPipeline(RenderContext context, string name, RenderPass renderPass, VkPrimitiveTopology topology, uint stride, Span<VertexInputAttributeDescription> attributes)
    {
        _context = context;
        Name = name;
        _topology = topology;
        _renderPass = renderPass;
        _stride = stride;
        _attributes = attributes.ToArray();
        _vertexShaderLastWriteTime = File.GetLastWriteTime(VertexShaderPath);
        _fragmentShaderLastWriteTime = File.GetLastWriteTime(FragmentShaderPath);
        
        Init();
        
        //TODO: Do sanity check on stride to make sure it matches the attributes total size
        //TODO: Also check that the offsets of the attributes make sense and they don't overlap each other. That is not something that RFG meshes do afaik
        //TODO: Will need function to get size of vulkan formats in bytes. Just implement for the used formats. If format is not used just don't bother.
    }

    private void Init()
    {
        CreateShaders();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
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
        DescriptorSetLayoutBinding uboLayoutBinding = new()
        {
            Binding = 0,
            DescriptorCount = 1,
            DescriptorType = DescriptorType.UniformBuffer,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.VertexBit,
        };

        DescriptorSetLayoutBinding samplerLayoutBinding = new()
        {
            Binding = 1,
            DescriptorCount = 1,
            DescriptorType = DescriptorType.CombinedImageSampler,
            PImmutableSamplers = null,
            StageFlags = ShaderStageFlags.FragmentBit,
        };

        DescriptorSetLayoutBinding[] bindings = [uboLayoutBinding, samplerLayoutBinding];

        fixed (DescriptorSetLayoutBinding* bindingsPtr = bindings)
        fixed (DescriptorSetLayout* descriptorSetLayoutPtr = &DescriptorSetLayout)
        {
            DescriptorSetLayoutCreateInfo layoutInfo = new()
            {
                SType = StructureType.DescriptorSetLayoutCreateInfo,
                BindingCount = (uint)bindings.Length,
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
                Topology = VkPrimitiveTopology.TriangleStrip,
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
                CullMode = CullModeFlags.BackBit,
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

            PushConstantRange pushConstantRange = new()
            {
                StageFlags = ShaderStageFlags.VertexBit,
                Offset = 0,
                Size = (uint)Unsafe.SizeOf<PerObjectPushConstants>()
            };
            
            PipelineLayoutCreateInfo pipelineLayoutInfo = new()
            {
                SType = StructureType.PipelineLayoutCreateInfo,
                PPushConstantRanges = &pushConstantRange,
                PushConstantRangeCount = 1,
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

    public void ReloadEditedShaders()
    {
        DateTime vertexShaderWriteTime = File.GetLastWriteTime(VertexShaderPath);
        DateTime fragmentShaderWriteTime = File.GetLastWriteTime(FragmentShaderPath);
        if (vertexShaderWriteTime != _vertexShaderLastWriteTime || fragmentShaderWriteTime != _fragmentShaderLastWriteTime)
        {
            //TODO: Add NF logging
            Console.WriteLine($"Reloading shaders for {Name}");
            Thread.Sleep(250); //Wait a moment to make sure the shader isn't being saved by another process while we're loading it. Stupid fix, but it works.
            _context.Vk.QueueWaitIdle(_context.GraphicsQueue); //Wait for graphics queue so we know the pipeline isn't in use when we destroy it
            Destroy();
            Init();
            
            _vertexShaderLastWriteTime = vertexShaderWriteTime;
            _fragmentShaderLastWriteTime = fragmentShaderWriteTime;
        }
    }
    
    public void Bind(CommandBuffer commandBuffer)
    {
        _context.Vk.CmdBindPipeline(commandBuffer, PipelineBindPoint.Graphics, _graphicsPipeline);
    }

    public unsafe void Destroy()
    {
        _context.Vk.DestroyPipeline(_context.Device, _graphicsPipeline, null);
        _context.Vk.DestroyPipelineLayout(_context.Device, Layout, null);
        
        _context.Vk.DestroyDescriptorSetLayout(_context.Device, DescriptorSetLayout, null);
        _vertexShader!.Destroy();
        _fragmentShader!.Destroy();
    }
}