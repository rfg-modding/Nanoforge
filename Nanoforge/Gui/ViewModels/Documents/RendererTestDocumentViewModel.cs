using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using Avalonia;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Nanoforge.Gui.Views.Controls;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Shared;
using Silk.NET.Vulkan;
using PrimitiveTopology = RFGM.Formats.Meshes.Shared.PrimitiveTopology;

namespace Nanoforge.Gui.ViewModels.Documents;

public partial class RendererTestDocumentViewModel : Document
{
    [ObservableProperty]
    private Scene _scene = new();

    private List<VertexFormat> _allStaticMeshVertexFormats = new();
    private RenderObject? _skybox = null;

    public RendererTestDocumentViewModel()
    {
        SceneInit();
    }
    
    public void SceneInit()
    {
        //TODO: Replace these hardcoded paths with packfile paths and load from the game files using PackfileVFS once its ported.
        const string texturePath = "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/viking_room.png";
        //const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/autoblaster/autoblaster.csmesh_pc";
        //const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/missilepod/dlc_rocket_pod.csmesh_pc";
        //const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/multi_object_backpack_thrust/thrust.csmesh_pc";
        //const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/EDF_Super_Gauss/super_gauss_rifle.csmesh_pc";
        //const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/dlcp01_items/Unpack/toolbox/amb_toolbox.csmesh_pc";
        const string meshPath = "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/dlcp01_items/Unpack/rpg_launcher/rpg.csmesh_pc";

        Renderer renderer = (Application.Current as App)!.Renderer;
        RenderContext context = renderer.Context;
        
        Mesh mesh = LoadRfgStaticMeshFromFile(context, meshPath);
        Texture2D texture = LoadTextureFromFile(context, texturePath);
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(0.0f, 0.0f, 0.0f), Matrix4x4.Identity, mesh, texture);
        
        Mesh mesh2 = LoadRfgStaticMeshFromFile(context, "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/multi_object_backpack_thrust/thrust.csmesh_pc");
        Texture2D texture2 = LoadTextureFromFile(context, "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/bright_future.png");
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(-2.0f, 0.0f, 0.0f), Matrix4x4.Identity, mesh2, texture2);
        
        Mesh mesh3 = LoadRfgStaticMeshFromFile(context, "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/EDF_Super_Gauss/super_gauss_rifle.csmesh_pc");
        Texture2D texture3 = LoadTextureFromFile(context, "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/bright_future.png");
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(2.0f, 0.0f, -1.0f), Matrix4x4.Identity, mesh3, texture3);
        
        Mesh mesh4 = LoadRfgStaticMeshFromFile(context, "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/items/Unpack/missilepod/dlc_rocket_pod.csmesh_pc");
        Texture2D texture4 = LoadTextureFromFile(context, "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/bright_future.png");
        Scene.CreateRenderObject("Pixlit1UvNmap", new Vector3(-2.0f, 0.0f, 2.0f), Matrix4x4.Identity, mesh4, texture4);
        
        Mesh mesh5 = LoadRfgStaticMeshFromFile(context, "/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/skybox/Unpack/rfg_skybox.csmesh_pc/rfg_skybox.csmesh_pc");
        Texture2D texture5 = LoadTextureFromFile(context, "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/bright_future.png");
        _skybox = Scene.CreateRenderObject("Pixlit1Uv", new Vector3(-2.0f, 0.0f, 2.0f), Matrix4x4.Identity, mesh5, texture5);
        _skybox.Position = Vector3.Zero;
        _skybox.Scale = new Vector3(25000.0f);
        
        /*
        List<string> meshPaths = new();
        FindFilesRecursive("/mnt/StorageC/_RFGR_Unpack/GOG_Terraform/Unpack/", ".csmesh_pc", meshPaths);

        List<string> bigassMeshPaths = meshPaths.Select(path => path).Where(path => path.Contains("tharsis") || path.Contains("sky")).ToList();

        // foreach (string path in bigassMeshPaths)
        // {
        //     meshPaths.Remove(path);
        // }
        
        int numMeshes = meshPaths.Count;
        int gridWidth = (int)Math.Sqrt(numMeshes) + 1;

        int i = 0;
        foreach (string meshPath in meshPaths)
        {
            MeshInstanceData meshData = LoadRfgStaticMesh(meshPath);

            Mesh mesh = new Mesh(context, meshData.Vertices, meshData.Indices, meshData.Config.NumVertices, meshData.Config.NumIndices, (uint)meshData.Config.IndexSize);
            Texture2D texture4 = LoadTextureFromFile(context, "/home/moneyl/projects/CSVulkanRenderer/CSVulkanRenderer/assets/bright_future.png");

            const float meshSeparation = 2.5f;
            int x = i % gridWidth;
            int y = i / gridWidth;

            float bigMeshSeparation = 40.0f;
            int bigMeshIndex = bigassMeshPaths.IndexOf(meshPath);
            bool isBigMesh = bigassMeshPaths.Contains(meshPath);

            Vector3 position = isBigMesh switch
            {
                true => new Vector3((gridWidth + 10) * meshSeparation, 0.0f, -50.0f +  bigMeshIndex * bigMeshSeparation),
                _ => new Vector3(x * meshSeparation, 0.0f, y * meshSeparation)
            };

            //Scene.CreateRenderObject(meshData.Config.VertexFormat.ToString(), new Vector3(-2.0f, 0.0f, 2.0f), Matrix4x4.Identity, mesh, texture4);
            Scene.CreateRenderObject(meshData.Config.VertexFormat.ToString(), position, Matrix4x4.Identity, mesh, texture4);

            i++;
        }*/
        
        Scene.Init(new Vector2(1920, 1080));
    }

    [RelayCommand]
    private void Update(FrameUpdateParams updateParams)
    {
        foreach (RenderObject renderObject in Scene.RenderObjects)
        {
            if (renderObject == _skybox)
                continue;
            
            float angle = updateParams.TotalTime * 50.0f;
            renderObject.Orient = Matrix4x4.CreateRotationY(MathHelpers.DegreesToRadians(angle));   
        }
    }
    
    private MeshInstanceData LoadRfgStaticMesh(string cpuFilePath)
    {
        try
        {
            string gpuFilePath = $"{Path.GetDirectoryName(cpuFilePath)}/{Path.GetFileNameWithoutExtension(cpuFilePath)}.gsmesh_pc";
            FileStream cpuFile = new(cpuFilePath, FileMode.Open);
            FileStream gpuFile = new(gpuFilePath, FileMode.Open);
        
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

    private Mesh LoadRfgStaticMeshFromFile(RenderContext context, string cpuFilePath)
    {
        MeshInstanceData meshData = LoadRfgStaticMesh(cpuFilePath);
        Mesh mesh = new Mesh(context, meshData.Vertices, meshData.Indices, meshData.Config.NumVertices, meshData.Config.NumIndices, (uint)meshData.Config.IndexSize);
        return mesh;
    }
    
    private Texture2D LoadTextureFromFile(RenderContext context, string path)
    {
        using var img = SixLabors.ImageSharp.Image.Load<SixLabors.ImageSharp.PixelFormats.Rgba32>(path);
        ulong imageSize = (ulong)(img.Width * img.Height * img.PixelType.BitsPerPixel / 8);
        uint mipLevels = (uint)(Math.Floor(Math.Log2(Math.Max(img.Width, img.Height))) + 1);

        byte[] pixelData = new byte[imageSize];
        img.CopyPixelDataTo(pixelData);

        Texture2D texture = new Texture2D(context, (uint)img.Width, (uint)img.Height, mipLevels, Format.R8G8B8A8Srgb, ImageTiling.Optimal,
                                     ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit, MemoryPropertyFlags.DeviceLocalBit,
                                          ImageAspectFlags.ColorBit);
        texture.TransitionLayoutImmediate(ImageLayout.TransferDstOptimal);
        texture.SetPixels(pixelData, true);
        texture.CreateTextureSampler();
        texture.CreateImageView();

        return texture;
    }
    
    private void FindFilesRecursive(string rootDirectory, string fileExtension, List<string> filePaths)
    {
        foreach (var file in Directory.EnumerateFiles(rootDirectory, $"*{fileExtension}"))
        {
            filePaths.Add(file);
        }
        foreach (var directory in Directory.EnumerateDirectories(rootDirectory))
        {
            FindFilesRecursive($"{directory}/", fileExtension, filePaths);            
        }
    }
}
