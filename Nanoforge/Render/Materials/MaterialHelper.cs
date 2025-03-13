using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
using Serilog;
using Silk.NET.Vulkan;
using VkPrimitiveTopology = Silk.NET.Vulkan.PrimitiveTopology;

namespace Nanoforge.Render.Materials;

public static class MaterialHelper
{
    private static RenderContext? _context;
    private static bool _initialized = false;

    private static Dictionary<string, MaterialPipeline> _materials = new();

    //Note: At the moment there's just one render pass used by the renderer. The MaterialPipelines need it, so it gets passed here.
    private static RenderPass _renderPass;
    private static VkBuffer[]? _uniformBuffers;
    private static VkBuffer[]? _perObjectConstantBuffers;
    private static DescriptorAllocator? _descriptorAllocator;

    public static void Init(RenderContext context, RenderPass renderPass, VkBuffer[] uniformBuffers, VkBuffer[] perObjectConstantBuffers)
    {
        try
        {
            if (_initialized)
            {
                throw new InvalidOperationException("Attempted to initializer MaterialHelper more than once. Not allowed.");
            }

            _context = context;
            _renderPass = renderPass;
            _uniformBuffers = uniformBuffers;
            _perObjectConstantBuffers = perObjectConstantBuffers;

            DescriptorAllocator.PoolSizeRatio[] poolSizeRatios =
            [
                new() { Type = DescriptorType.UniformBuffer,        Ratio = 1.0f },
                new() { Type = DescriptorType.CombinedImageSampler, Ratio = 10.0f },
                new() { Type = DescriptorType.StorageBuffer       , Ratio = 1.0f },
            ];
            _descriptorAllocator = new DescriptorAllocator(_context, 10, poolSizeRatios);

            //TODO: Port the rest of the vertex formats and shaders from the Beeflang version of NF and DX11
            CreateMaterial("Terrain", VkPrimitiveTopology.TriangleStrip, stride: 8,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R16G16Sint,      Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 4 },  //Normal
                ]
            );
            CreateMaterial("TerrainStitch", VkPrimitiveTopology.TriangleStrip, stride: 16,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat,  Offset = 0  },  //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,    Offset = 12 },  //Normal
                ]
            );
            CreateMaterial("TerrainLowLod", VkPrimitiveTopology.TriangleStrip, stride: 8,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R16G16B16A16Sint, Offset = 0  }, //Position
                ]
            );
            CreateMaterial("Pixlit1Uv", VkPrimitiveTopology.TriangleStrip, stride: 20,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R16G16Sint,      Offset = 16 }  //TexCoord0
                ]
            );
            CreateMaterial("Pixlit1UvNmap", VkPrimitiveTopology.TriangleStrip, stride: 24,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0 }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm, Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R8G8B8A8Unorm, Offset = 16 }, //Tangent
                    new VertexInputAttributeDescription { Binding = 0, Location = 3, Format = Format.R16G16Sint, Offset = 20 } //TexCoord0
                ]
            );
            CreateMaterial("Pixlit2UvNmap", VkPrimitiveTopology.TriangleStrip, stride: 28,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R8G8B8A8Unorm,   Offset = 16 }, //Tangent
                    new VertexInputAttributeDescription { Binding = 0, Location = 3, Format = Format.R16G16Sint,      Offset = 20 }, //TexCoord0
                    new VertexInputAttributeDescription { Binding = 0, Location = 4, Format = Format.R16G16Sint,      Offset = 24 }  //TexCoord1
                ]
            );
            CreateMaterial("Pixlit3UvNmap", VkPrimitiveTopology.TriangleStrip, stride: 32,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R8G8B8A8Unorm,   Offset = 16 }, //Tangent
                    new VertexInputAttributeDescription { Binding = 0, Location = 3, Format = Format.R16G16Sint,      Offset = 20 }, //TexCoord0
                    new VertexInputAttributeDescription { Binding = 0, Location = 4, Format = Format.R16G16Sint,      Offset = 24 }, //TexCoord1
                    new VertexInputAttributeDescription { Binding = 0, Location = 5, Format = Format.R16G16Sint,      Offset = 28 }  //TexCoord2
                ]
            );
            CreateMaterial("Pixlit4UvNmap", VkPrimitiveTopology.TriangleStrip, stride: 36,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R8G8B8A8Unorm,   Offset = 16 }, //Tangent
                    new VertexInputAttributeDescription { Binding = 0, Location = 3, Format = Format.R16G16Sint,      Offset = 20 }, //TexCoord0
                    new VertexInputAttributeDescription { Binding = 0, Location = 4, Format = Format.R16G16Sint,      Offset = 24 }, //TexCoord1
                    new VertexInputAttributeDescription { Binding = 0, Location = 5, Format = Format.R16G16Sint,      Offset = 28 }, //TexCoord2
                    new VertexInputAttributeDescription { Binding = 0, Location = 6, Format = Format.R16G16Sint,      Offset = 32 }  //TexCoord3
                ]
            );
            CreateMaterial("Linelist", VkPrimitiveTopology.LineList, stride: 20,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32A32Sfloat, Offset = 0  }, //Position and size
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,      Offset = 16 }, //Color
                ]
            );
            //TODO: See if there's a way to draw these without disabling face culling. Was having problems with some faces being hidden at some camera angles regardless of front face being CW or CCW
            CreateMaterial("SolidTriList", VkPrimitiveTopology.TriangleList, stride: 16,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0  }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm,   Offset = 12 }, //Color
                ],
            disableFaceCulling: true);

            _initialized = true;
        }
        catch (Exception ex)
        {
            Log.Error($"MaterialHelper.Init() failed: `{ex.Message}`");
            throw;
        }
    }

    private static void CreateMaterial(string name, VkPrimitiveTopology topology, uint stride, Span<VertexInputAttributeDescription> attributes, bool disableFaceCulling = false)
    {
        if (_materials.ContainsKey(name))
            throw new Exception($"Material with name '{name}' already exists!");

        MaterialPipeline materialPipeline = new(_context!, name, _renderPass, topology, stride, attributes, disableFaceCulling);
        _materials.Add(name, materialPipeline);
    }

    public static MaterialPipeline? GetMaterialPipeline(string name)
    {
        foreach (var kv in _materials)
        {
            if (kv.Key.Equals(name, StringComparison.InvariantCultureIgnoreCase))
            {
                return kv.Value;
            }
        }

        return null;
    }

    public static MaterialInstance CreateMaterialInstance(string name, Texture2D[]? textures = null)
    {
        MaterialPipeline? pipeline = GetMaterialPipeline(name);
        if (pipeline == null)
        {
            throw new Exception($"Material pipeline with name '{name}' does not exist!");
        }

        if (textures == null)
        {
            textures = new Texture2D[10];
            Array.Fill(textures, Texture2D.DefaultTexture);
        }

        DescriptorSet[] descriptorSets = CreateDescriptorSets(textures, pipeline);
        MaterialInstance material = new(pipeline, descriptorSets);
        return material;
    }

    private static unsafe DescriptorSet[] CreateDescriptorSets(Texture2D[] textures, MaterialPipeline material)
    {
        var layouts = new DescriptorSetLayout[Renderer.MaxFramesInFlight];
        Array.Fill(layouts, material.DescriptorSetLayout);

        DescriptorSet[] descriptorSets = new DescriptorSet[Renderer.MaxFramesInFlight];
        for (int i = 0; i < descriptorSets.Length; i++)
        {
            descriptorSets[i] = _descriptorAllocator!.Allocate(layouts[i]);
        }

        for (int i = 0; i < Renderer.MaxFramesInFlight; i++)
        {
            DescriptorBufferInfo bufferInfo = new()
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

            DescriptorImageInfo[] imageInfos = new DescriptorImageInfo[textures.Length];
            for (var j = 0; j < textures.Length; j++)
            {
                var texture = textures[j];
                imageInfos[j] = new DescriptorImageInfo
                {
                    ImageLayout = ImageLayout.ShaderReadOnlyOptimal,
                    ImageView = texture.ImageViewHandle,
                    Sampler = texture.SamplerHandle
                };
            }

            fixed (DescriptorImageInfo* imageInfoPtr = imageInfos)
            {
                List<WriteDescriptorSet> descriptorWrites = new();

                //Per frame uniform buffer
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = descriptorSets[i],
                    DstBinding = 0,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.UniformBuffer,
                    DescriptorCount = 1,
                    PBufferInfo = &bufferInfo,
                });

                uint firstSamplerBinding = 1;
                for (uint j = 0; j < textures.Length; j++)
                {
                    descriptorWrites.Add(new()
                    {
                        SType = StructureType.WriteDescriptorSet,
                        DstSet = descriptorSets[i],
                        DstBinding = firstSamplerBinding + j,
                        DstArrayElement = 0,
                        DescriptorType = DescriptorType.CombinedImageSampler,
                        DescriptorCount = 1,
                        PImageInfo = imageInfoPtr + j,
                    });
                }
                
                //Per object constants buffer
                uint lastSamplerBinding = firstSamplerBinding + (uint)textures.Length - 1;
                descriptorWrites.Add(new()
                {
                    SType = StructureType.WriteDescriptorSet,
                    DstSet = descriptorSets[i],
                    DstBinding = lastSamplerBinding + 1,
                    DstArrayElement = 0,
                    DescriptorType = DescriptorType.StorageBuffer,
                    DescriptorCount = 1,
                    PBufferInfo = &perObjectConstantsBuffer,
                });

                var descriptorWritesArray = descriptorWrites.ToArray();
                fixed (WriteDescriptorSet* descriptorWritesPtr = descriptorWritesArray)
                {
                    _context!.Vk.UpdateDescriptorSets(_context.Device, (uint)descriptorWritesArray.Length, descriptorWritesPtr, 0, null);
                }
            }
        }

        return descriptorSets;
    }

    public static void ReloadEditedShaders()
    {
        foreach (MaterialPipeline material in _materials.Values)
        {
            material.ReloadEditedShaders();
        }
    }

    public static void Destroy()
    {
        _descriptorAllocator!.Destroy();
        foreach (MaterialPipeline material in _materials.Values)
        {
            material.Destroy();
        }

        _materials.Clear();
    }
}