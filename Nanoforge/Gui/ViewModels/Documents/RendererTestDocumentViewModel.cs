﻿using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Nanoforge.FileSystem;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
using Serilog;
using Silk.NET.Vulkan;
using PrimitiveTopology = RFGM.Formats.Meshes.Shared.PrimitiveTopology;

namespace Nanoforge.Gui.ViewModels.Documents;

public partial class RendererTestDocumentViewModel : Document
{
    [ObservableProperty]
    private Scene _scene = new();

    private List<VertexFormat> _allStaticMeshVertexFormats = new();
    private RenderObject? _skybox = null;

    [RelayCommand]
    public async Task SceneInit()                     
    {
        Renderer? renderer = (Application.Current as App)!.Renderer;
        if (renderer == null)
            return;
        
        RenderContext context = renderer.Context;

        //TODO: Use TextureIndex + textures list in static meshes to find and load specific pegs for each mesh
        //string texturePath = "//data/items.vpp_pc/rpg_high.str2_pc/rpg_high.cpeg_pc";
        string texturePath = "//data/skybox.vpp_pc/rfg_skybox.csmesh_pc.str2_pc/rfg_skybox.cpeg_pc";
        Mesh mesh = LoadRfgStaticMeshFromPackfile(context, "//data/dlcp01_items.vpp_pc/rpg_launcher.str2_pc/rpg.csmesh_pc");
        Texture2D texture = LoadTextureFromPackfile(context, texturePath);
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(0.0f, 0.0f, 0.0f), Matrix4x4.Identity, mesh, texture);
        
        Mesh mesh2 = LoadRfgStaticMeshFromPackfile(context, "//data/items.vpp_pc/multi_object_backpack_thrust.str2_pc/thrust.csmesh_pc");
        Texture2D texture2 = LoadTextureFromPackfile(context, texturePath);
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(-2.0f, 0.0f, 0.0f), Matrix4x4.Identity, mesh2, texture2);
        
        Mesh mesh3 = LoadRfgStaticMeshFromPackfile(context, "//data/items.vpp_pc/EDF_Super_Gauss.str2_pc/super_gauss_rifle.csmesh_pc");
        Texture2D texture3 = LoadTextureFromPackfile(context, texturePath);
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(2.0f, 0.0f, -1.0f), Matrix4x4.Identity, mesh3, texture3);
        
        Mesh mesh4 = LoadRfgStaticMeshFromPackfile(context, "//data/items.vpp_pc/missilepod.str2_pc/dlc_rocket_pod.csmesh_pc");
        Texture2D texture4 = LoadTextureFromPackfile(context, texturePath);
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(-2.0f, 0.0f, 2.0f), Matrix4x4.Identity, mesh4, texture4);
        
        Mesh mesh5 = LoadRfgStaticMeshFromPackfile(context, "//data/skybox.vpp_pc/rfg_skybox.csmesh_pc.str2_pc/rfg_skybox.csmesh_pc");
        Texture2D texture5 = LoadTextureFromPackfile(context, texturePath);
        _skybox = Scene.CreateRenderObject("Pixlit1Uv", new Vector3(-2.0f, 0.0f, 2.0f), Matrix4x4.Identity, mesh5, texture5);
        _skybox.Position = Vector3.Zero;
        _skybox.Scale = new Vector3(25000.0f);
        
        Scene.Init(new Vector2(1920, 1080));
    }

    [RelayCommand]
    private void Update(SceneFrameUpdateParams updateParams)
    {
        foreach (RenderObject renderObject in Scene.RenderObjects)
        {
            if (renderObject == _skybox)
                continue;
            
            float angle = updateParams.TotalTime * 50.0f;
            renderObject.Orient = Matrix4x4.CreateRotationY(MathHelpers.ToRadians(angle));   
        }

        Scene.Update(updateParams);
    }
    
    private MeshInstanceData LoadRfgStaticMesh(string cpuFilePath)
    {
        try
        {
            using Stream? cpuFile = PackfileVFS.OpenFile(cpuFilePath);
            if (cpuFile == null)
            {
                string error = $"Failed to open cpu file for static mesh at {cpuFilePath}";
                Log.Error(error);
                throw new Exception(error);
            }
            
            string gpuFilePath = $"/{Path.GetDirectoryName(cpuFilePath)}/{Path.GetFileNameWithoutExtension(cpuFilePath)}.gsmesh_pc";
            using Stream? gpuFile = PackfileVFS.OpenFile(gpuFilePath);
            if (gpuFile == null)
            {
                string error = $"Failed to open gpu file for static mesh at {gpuFilePath}";
                Log.Error(error);
                throw new Exception(error);
            }
        
            StaticMesh staticMesh = new();
            staticMesh.ReadHeader(cpuFile);
            MeshInstanceData meshData = staticMesh.ReadData(gpuFile);
            
            if (meshData.Config.Topology != PrimitiveTopology.TriangleStrip)
            {
                //Adding this warning
                throw new Exception("Encountered static mesh with primitive topology other than triangle strip.");
            }

            return meshData;
        }
        catch (Exception e)
        {
            Console.WriteLine(e);
            throw;
        }
    }

    private Mesh LoadRfgStaticMeshFromPackfile(RenderContext context, string cpuFilePath)
    {
        MeshInstanceData meshData = LoadRfgStaticMesh(cpuFilePath);
        Mesh mesh = new Mesh(context, meshData.Vertices, meshData.Indices, meshData.Config.NumVertices, meshData.Config.NumIndices, (uint)meshData.Config.IndexSize);
        return mesh;
    }

    private Texture2D LoadTextureFromPackfile(RenderContext context, string cpuFilePath, int logicalTextureIndex = 0)
    {
        string filename = Path.GetFileName(cpuFilePath);
        
        string cpuFileExtension = Path.GetExtension(cpuFilePath);
        string gpuFileExtension = cpuFileExtension switch
        {
            ".cpeg_pc" => ".gpeg_pc",
            ".cvbm_pc" => ".gvbm_pc",
            _ => throw new Exception($"Unsupported peg file extension: {cpuFileExtension}")
        };
        
        using Stream? cpuFile = PackfileVFS.OpenFile(cpuFilePath);
        if (cpuFile == null)
        {
            string error = $"Failed to open cpu file for texture at {cpuFilePath}";
            Log.Error(error);
            throw new Exception(error);
        }
            
        string gpuFilePath = $"/{Path.GetDirectoryName(cpuFilePath)}/{Path.GetFileNameWithoutExtension(cpuFilePath)}{gpuFileExtension}";
        using Stream? gpuFile = PackfileVFS.OpenFile(gpuFilePath);
        if (gpuFile == null)
        {
            string error = $"Failed to open gpu file for texture at {gpuFilePath}";
            Log.Error(error);
            throw new Exception(error);
        }
        
        PegReader reader = new();
        LogicalTextureArchive result = reader.Read(cpuFile, gpuFile, filename, CancellationToken.None);
        LogicalTexture logicalTexture = result.LogicalTextures[logicalTextureIndex];
        
        using var memoryStream = new MemoryStream();
        logicalTexture.Data.CopyTo(memoryStream);
        byte[] pixels = memoryStream.ToArray();

        Format vulkanPixelFormat = logicalTexture.Format switch
        {
            RfgCpeg.Entry.BitmapFormat.PcDxt1 => Format.BC1RgbUnormBlock,
            RfgCpeg.Entry.BitmapFormat.PcDxt3 => Format.BC2UnormBlock,
            RfgCpeg.Entry.BitmapFormat.PcDxt5 => Format.BC3UnormBlock,
            _ => throw new Exception($"Unsupported pixel format: {logicalTexture.Format}")
        };
    
        //TODO: Need to support SRGB
        //if (logicalTexture.Flags.HasFlag(TextureFlags.Srgb))
        
        Texture2D texture = new Texture2D(context, (uint)logicalTexture.Size.Width, (uint)logicalTexture.Size.Height, (uint)logicalTexture.MipLevels, vulkanPixelFormat, 
            ImageTiling.Optimal, ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit, MemoryPropertyFlags.DeviceLocalBit,
            ImageAspectFlags.ColorBit);
        texture.SetPixels(pixels, generateMipMaps: false);
        texture.CreateTextureSampler();
        texture.CreateImageView();

        return texture;
    }
}
