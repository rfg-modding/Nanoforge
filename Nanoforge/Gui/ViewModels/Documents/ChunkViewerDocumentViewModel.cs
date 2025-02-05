using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Threading;
using Avalonia;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Nanoforge.FileSystem;
using Nanoforge.Gui.ViewModels.Dialogs;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Chunks;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
using Serilog;
using Silk.NET.Vulkan;
using PrimitiveTopology = RFGM.Formats.Meshes.Shared.PrimitiveTopology;

namespace Nanoforge.Gui.ViewModels.Documents;

public partial class ChunkViewerDocumentViewModel : NanoforgeDocument
{
    [ObservableProperty]
    private string _chunkCpuFilePath;
    
    [ObservableProperty]
    private Scene _scene = new();

    [ObservableProperty]
    private bool _sceneInitialized = false;

    [ObservableProperty]
    private ChunkFile? _chunkFile;

    [ObservableProperty]
    private MeshInstanceData? _rawMeshData;

    [ObservableProperty]
    private ObservableCollection<RenderChunk> _renderChunks = new();

    public ChunkViewerDocumentViewModel()
    {
        if (!Design.IsDesignMode)
            throw new Exception("The default ChunkViewerDocumentViewModel can only be used in Design Mode.");

        ChunkCpuFilePath = "";
    }

    public ChunkViewerDocumentViewModel(string chunkCpuFilePath)
    {
        ChunkCpuFilePath = chunkCpuFilePath;
        TaskDialog dialog = new TaskDialog();
        dialog.ShowDialog(MainWindow.Instance);
        ThreadPool.QueueUserWorkItem(_ => SceneInit(dialog.ViewModel!));
    }

    public void SceneInit(TaskDialogViewModel status)
    {
        try
        {
            Renderer? renderer = (Application.Current as App)!.Renderer;
            if (renderer == null)
                return;

            status.Setup(2, "Waiting for data folder to be mounted...");
            status.Title = $"Loading {Path.GetFileName(ChunkCpuFilePath)}";

            while (!PackfileVFS.Ready)
            {
                Thread.Sleep(200);
            }

            status.NextStep("Loading chunk files...");
            RenderContext context = renderer.Context;

            using Stream? cpuFile = PackfileVFS.OpenFile(ChunkCpuFilePath);
            if (cpuFile == null)
            {
                Log.Error($"Chunk viewer failed to open chunk cpu file stream at {ChunkCpuFilePath}");
                status.SetStatus("Failed to open file. Check the logs.");
                status.CanClose = true;
                return;
            }

            string chunkGpuFilePath = GpuFilePathFromCpuFilePath(ChunkCpuFilePath, ".gchk_pc");
            using Stream? gpuFile = PackfileVFS.OpenFile(chunkGpuFilePath);
            if (gpuFile == null)
            {
                Log.Error($"Chunk viewer failed to open chunk gpu file stream at {chunkGpuFilePath}");
                status.SetStatus("Failed to open file. Check the logs.");
                status.CanClose = true;
                return;
            }

            ChunkFile = new ChunkFile(Path.GetFileName(ChunkCpuFilePath));
            ChunkFile.ReadHeader(cpuFile);
            RawMeshData = ChunkFile.ReadData(gpuFile);
            InspectorTarget = this;
            OutlinerTarget = this;
            
            Log.Information($"Opening '{ChunkCpuFilePath}' in chunk viewer. Vertex format: {ChunkFile.Config.VertexFormat}, Index size: {ChunkFile.Config.IndexSize}, Num destroyables: {ChunkFile.Destroyables.Count}");
            
            //TODO: Figure out if vulkan thread is broken again or what. Had threading validation layer warnings popup again after opening 0101office_corp_large_a then backpackrack, both in 1.vpp_pc/ns_base.str2_pc
            if (ChunkFile.Destroyables.Count > 0)
            {
                Mesh mesh = new Mesh(context, RawMeshData, context.TransferCommandPool, context.TransferQueue);
                float destroyableSpacing = 20.0f;
                
                int destroyableIndex = 0;
                foreach (Destroyable destroyable in ChunkFile.Destroyables)
                {
                    //TODO: Change to make RenderChunk for every destroyable
                    RenderChunk chunk = CreateRenderChunkFromDestroyable(context, mesh, destroyable);
                    chunk.Position.X = destroyableIndex * destroyableSpacing;
                    RenderChunks.Add(chunk);   
                    destroyableIndex++;
                }
            }
            else
            {
                //TODO: Add warning on scene or in inspector indicating that it has no destroyables and thus nothing will be drawn
                //TODO: Maybe reuse that to show when exception happens during loading process. Currently it still says "waiting for scene to load..."
                Log.Information("Opened chunk '{ChunkCpuFileName}' in viewer. It has no destroyables.", ChunkCpuFilePath);
            }
            
            //TODO: Make UI for seeing info about the chunk file
            //TODO:     Ideally use the inspector. Will need to write code to change the inspector based on the currently selected document + add interface for documents to define their Inspector view + VM
            //TODO: Make UI for switching the current destroyable to be displayed and/or display all of them
            
            //TODO: Load the textures and apply them
            //TODO:     Try to use new knowledge of mesh materials to properly apply the textures to each subpiece
            
            //TODO: Update RenderChunk and RenderObject to only use ProjectMesh instead of MeshInstanceData
            //TODO: Update this to use ProjectMesh instead of MeshInstanceData
            
            //TODO: Remove extra funcs from this class that aren't used
            //TODO: Make this class + the Mesh class use ProjectMesh as input instead of MeshInstanceData
            //TODO:     To make it so this document doesn't need a project open could make a class that inherits ProjectBuffer but stores the buffers in memory
            //TODO:     Or just restrict opening files in the file explorer until a project is opened
            
            //TODO: Try to add a custom title bar for NF. If its as easy as it is in WPF then it's worthwhile. Better make sure it works on both Linux and Windows though.
            
            Scene.Init(renderer.Context);
            //TODO: For debugging purposes
            //TODO: Make version that attempts to auto position cam based on size of chunk (if it has the necessary data)
            Scene.Camera!.TargetPosition = new Vector3(-29.884623f, 36.636642f, -35.14956f);
            renderer.ActiveScenes.Add(Scene);
            SceneInitialized = true;
            
            //TODO: Make sure all resources get destroyed when document is closed

            status.NextStep("Done!");
            status.CanClose = true;
            status.CloseDialog();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading chunk file in chunk viewer");
            status.SetStatus("Error loading chunk files. Check the logs.");
            status.CanClose = true;
        }
    }

    public override bool OnClose()
    {
        Renderer renderer = App.Current.Renderer!;
        Scene.Destroy();
        renderer.ActiveScenes.Remove(Scene);
        return base.OnClose();
    }

    //TODO: Once helper functions for drawing lines/boxes/etc are added update this to optionally draw other interesting data from the chunk files like other bounding boxes
    [RelayCommand]
    private void Update(SceneFrameUpdateParams updateParams)
    {
        Scene.Update(updateParams);
    }

    private RenderChunk CreateRenderChunkFromDestroyable(RenderContext context, Mesh mesh, Destroyable destroyable)
    {
        Texture2D[] textures = new Texture2D[10];
        for (int i = 0; i < 10; i++)
        {
            textures[i] = Texture2D.DefaultTexture;
        }
        RenderChunk chunk = Scene.CreateRenderChunk(mesh.Config.VertexFormat.ToString(), Vector3.Zero, Matrix4x4.Identity, mesh, textures, destroyable);
        return chunk;
    }
    
    private string GpuFilePathFromCpuFilePath(string cpuFilePath, string gpuFileExtension)
    {
        int lastSlashIndex = cpuFilePath.LastIndexOf('/');
        string directory = cpuFilePath.Substring(0, lastSlashIndex);
        string fileName = cpuFilePath.Substring(lastSlashIndex + 1);
        string fileNameWithoutExtension = Path.GetFileNameWithoutExtension(fileName);
        string gpuFilePath = $"{directory}/{fileNameWithoutExtension}{gpuFileExtension}";
        return gpuFilePath;
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

            string gpuFilePath = GpuFilePathFromCpuFilePath(cpuFilePath, ".gsmesh_pc");
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
        Mesh mesh = new Mesh(context, meshData, context.TransferCommandPool, context.TransferQueue);
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

        string gpuFilePath = GpuFilePathFromCpuFilePath(cpuFilePath, gpuFileExtension);
        using Stream? gpuFile = PackfileVFS.OpenFile(gpuFilePath);
        if (gpuFile == null)
        {
            string error = $"Failed to open gpu file for texture at {gpuFilePath}";
            Log.Error(error);
            throw new Exception(error);
        }

        PegReader reader = new();
        LogicalTextureArchive result = reader.Read(cpuFile, gpuFile, filename, CancellationToken.None);
        List<LogicalTexture> textures = result.LogicalTextures.ToList();
        LogicalTexture logicalTexture = textures[logicalTextureIndex];

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

        Texture2D texture = new Texture2D(context, (uint)logicalTexture.Size.Width, (uint)logicalTexture.Size.Height, (uint)logicalTexture.MipLevels,
            vulkanPixelFormat,
            ImageTiling.Optimal, ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit,
            MemoryPropertyFlags.DeviceLocalBit,
            ImageAspectFlags.ColorBit);
        texture.SetPixels(pixels, context.TransferCommandPool, context.TransferQueue, generateMipMaps: false);
        texture.CreateTextureSampler();
        texture.CreateImageView();

        return texture;
    }
}