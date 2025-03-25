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
    private static VkBuffer[]? _materialInfoBuffers;

    public static void Init(RenderContext context, RenderPass renderPass, VkBuffer[] uniformBuffers, VkBuffer[] perObjectConstantBuffers, VkBuffer[] materialInfoBuffers)
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
            _materialInfoBuffers = materialInfoBuffers;
            
            CreateMaterial("UnifiedMaterial", VkPrimitiveTopology.TriangleStrip, stride: 24,
                attributes:
                [
                    new VertexInputAttributeDescription { Binding = 0, Location = 0, Format = Format.R32G32B32Sfloat, Offset = 0 }, //Position
                    new VertexInputAttributeDescription { Binding = 0, Location = 1, Format = Format.R8G8B8A8Unorm, Offset = 12 }, //Normal
                    new VertexInputAttributeDescription { Binding = 0, Location = 2, Format = Format.R8G8B8A8Unorm, Offset = 16 }, //Tangent
                    new VertexInputAttributeDescription { Binding = 0, Location = 3, Format = Format.R16G16Sint, Offset = 20 } //TexCoord0
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
        if (_uniformBuffers is null || _perObjectConstantBuffers is null || _materialInfoBuffers is null)
            throw new Exception("Required GPU buffers not passed to MaterialHelper!");

        MaterialPipeline materialPipeline = new(_context!, name, _renderPass, topology, stride, attributes, _uniformBuffers, _perObjectConstantBuffers, _materialInfoBuffers, disableFaceCulling);
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

    public static void ReloadEditedShaders()
    {
        foreach (MaterialPipeline material in _materials.Values)
        {
            material.ReloadEditedShaders();
        }
    }

    public static void Destroy()
    {
        foreach (MaterialPipeline material in _materials.Values)
        {
            material.Destroy();
        }

        _materials.Clear();
    }

    public static void UpdateTextureArrayDescriptors()
    {
        foreach (MaterialPipeline pipeline in _materials.Values)
        {
            pipeline.UpdateDescriptorSets();
            //pipeline.UpdateTextureArrayDescriptors();
        }
    }
}