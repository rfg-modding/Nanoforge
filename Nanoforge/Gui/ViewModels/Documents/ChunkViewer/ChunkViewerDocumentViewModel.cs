using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Threading;
using Avalonia;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Nanoforge.FileSystem;
using Nanoforge.Gui.ViewModels.Dialogs;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using Nanoforge.Rfg;
using RFGM.Formats.Materials;
using RFGM.Formats.Meshes;
using RFGM.Formats.Meshes.Chunks;
using RFGM.Formats.Meshes.Shared;
using RFGM.Formats.Peg;
using RFGM.Formats.Peg.Models;
using Serilog;
using Silk.NET.Vulkan;
using PrimitiveTopology = RFGM.Formats.Meshes.Shared.PrimitiveTopology;

namespace Nanoforge.Gui.ViewModels.Documents.ChunkViewer;

public enum TextureType
{
    Diffuse,
    Specular,
    Normal,
    Missing,
    Decal,
    Unknown,
}

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

    //TODO: Turn this into a CVar and give the user a way to edit it and trigger a mesh reload
    public readonly bool UseDebugTextures = false;
    
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
        bool preloadedParentDirectory = false;
        string parentPreloadPath = string.Empty;
        try
        {
            Renderer? renderer = (Application.Current as App)!.Renderer;
            if (renderer == null)
                return;

            status.Setup(4, "Waiting for data folder to be mounted...");
            status.Title = $"Loading {Path.GetFileName(ChunkCpuFilePath)}";

            while (!PackfileVFS.Ready)
            {
                Thread.Sleep(200);
            }

            status.NextStep("Loading chunk files...");
            RenderContext context = renderer.Context;

            EntryBase? chunkFileEntrySearch = PackfileVFS.GetEntry(ChunkCpuFilePath);
            if (chunkFileEntrySearch is not FileEntry chunkFileEntry)
            {
                Log.Error($"Chunk viewer failed to find chunk cpu file at {ChunkCpuFilePath}");
                status.SetStatus("Failed to open file. Check the logs.");
                status.CanClose = true;
                return;
            }

            if (chunkFileEntry.Parent is not DirectoryEntry parentEntry)
            {
                Log.Error($"Cpu file at {ChunkCpuFilePath} is not in a packfile. Can not continue.");
                status.SetStatus("File structure problem. Check the logs.");
                status.CanClose = true;
                return;
            }

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
            Texture2D defaultNormalMap = Texture2D.FromFile(context, context.TransferCommandPool, context.TransferQueue, $"{BuildConfig.AssetsDirectory}textures/FlatNormalMap.png")!;
            //Used as the default specular map since its grey. Not 100% shiny, not completely matte.
            Texture2D defaultSpecularMap = Texture2D.FromFile(context, context.TransferCommandPool, context.TransferQueue, $"{BuildConfig.AssetsDirectory}textures/Missing.png")!;
            
            List<List<Texture2D>> materialTextures = new();
            if (UseDebugTextures)
            {
                status.NextStep(); //There isn't a preload/indexing step when debug textures are used
                status.NextStep("Loading textures...");
                for (var materialIndex = 0; materialIndex < ChunkFile.Materials.Count; materialIndex++)
                {
                    if (UseDebugTextures)
                    {
                        string texturePath = $"{BuildConfig.AssetsDirectory}textures/{materialIndex}b.png";
                        Texture2D? texture = Texture2D.FromFile(context, context.TransferCommandPool, context.TransferQueue, texturePath);
                        if (texture == null)
                        {
                            Log.Error($"Chunk viewer failed to load test texture at {texturePath}");
                            status.SetStatus("Failed to load textures. Check the logs.");
                            status.CanClose = true;
                            return;
                        }

                        materialTextures.Add([texture]);
                    }
                }
            }
            else
            {
                status.NextStep("Indexing textures...");
                //If the parent directory is a str2 we preload it into memory for a major speed boost
                if (parentEntry.Name.EndsWith(".str2_pc", StringComparison.OrdinalIgnoreCase))
                {
                    Debug.Assert(parentEntry.Parent != null);
                    parentPreloadPath = $"//data/{parentEntry.Parent!.Name}/{parentEntry.Name}/";
                    Log.Information($"Preloading {parentPreloadPath} before loading chunk textures...");
                    PackfileVFS.PreloadDirectory(parentPreloadPath);
                    preloadedParentDirectory = true;
                }

                //First we need to index relevant vpps/str2s for textures
                //Index common/preload vpps
                TextureIndex.IndexVpp("misc.vpp_pc");
                TextureIndex.IndexVpp("mp_common.vpp_pc");
                TextureIndex.IndexVpp("dlc01_precache.vpp_pc");
                TextureIndex.IndexVpp("terr01_precache.vpp_pc");
                TextureIndex.IndexDirectory(parentEntry);

                status.NextStep("Loading textures...");
                foreach (var material in ChunkFile.Materials)
                {
                    List<Texture2D> textures = new();
                    
                    //Find diffuse map
                    string? diffuseTextureName = material.Textures.Select(texture => texture.Name).FirstOrDefault(textureName => ClassifyTexture(textureName!) == TextureType.Diffuse, null);
                    if (diffuseTextureName == null)
                    {
                        textures.Add(Texture2D.DefaultTexture);
                    }
                    else
                    {
                        Texture2D? diffuseTexture = TryLoadTextureFromTgaName(diffuseTextureName, chunkFileEntry, context, context.TransferCommandPool, context.TransferQueue);
                        if (diffuseTexture == null)
                        {
                            textures.Add(Texture2D.DefaultTexture);
                        }
                        else
                        {
                            textures.Add(diffuseTexture);
                        }
                    }
                    
                    //Find normal map
                    string? normalTextureName = material.Textures.Select(texture => texture.Name).FirstOrDefault(textureName => ClassifyTexture(textureName!) == TextureType.Normal, null);
                    if (normalTextureName == null)
                    {
                        textures.Add(defaultNormalMap);
                    }
                    else
                    {
                        Texture2D? normalTexture = TryLoadTextureFromTgaName(normalTextureName, chunkFileEntry, context, context.TransferCommandPool, context.TransferQueue);
                        if (normalTexture == null)
                        {
                            textures.Add(defaultNormalMap);
                        }
                        else
                        {
                            textures.Add(normalTexture);
                        }
                    }
                        
                    //Find specular map
                    string? specularTextureName = material.Textures.Select(texture => texture.Name).FirstOrDefault(textureName => ClassifyTexture(textureName!) == TextureType.Specular, null);
                    if (specularTextureName == null)
                    {
                        textures.Add(defaultSpecularMap);
                    }
                    else
                    {
                        Texture2D? specularTexture = TryLoadTextureFromTgaName(specularTextureName, chunkFileEntry, context, context.TransferCommandPool, context.TransferQueue);
                        if (specularTexture == null)
                        {
                            textures.Add(defaultSpecularMap);
                        }
                        else
                        {
                            textures.Add(specularTexture);
                        }
                    }
                    
                    materialTextures.Add(textures);
                }
            }
            
            //Chunks can have multiple materials. Each material can have multiple textures
            List<Texture2D[]> texturesByMaterial = new();
            foreach (List<Texture2D> textures in materialTextures)
            {
                Debug.Assert(textures.Count <= 10);
                texturesByMaterial.Add(textures.ToArray()); //Currently we only use the diffuse textures for chunks
            }
            
            Mesh mesh = new Mesh(context, RawMeshData, context.TransferCommandPool, context.TransferQueue);
            float destroyableSpacing = 20.0f;

            if (ChunkFile.Destroyables.Count > 0)
            {
                int destroyableIndex = 0;
                foreach (Destroyable destroyable in ChunkFile.Destroyables)
                {
                    //TODO: Change to make RenderChunk for every destroyable
                    RenderChunk chunk = CreateRenderChunkFromDestroyable(context, mesh, destroyable, texturesByMaterial);
                    chunk.Position.X = destroyableIndex * destroyableSpacing;
                    RenderChunks.Add(chunk);
                    destroyableIndex++;
                }
            }
            else
            {
                //TODO: Add warning on scene or in inspector indicating that it has no destroyables and thus nothing will be drawn
                //TODO: Maybe reuse that to show when exception happens during loading process. Currently it still says "waiting for scene to load..."
                Log.Warning("Opened chunk '{ChunkCpuFileName}' in viewer. It has no destroyables. Will be rendered as a simple mesh", ChunkCpuFilePath);

                Scene.CreateRenderObject(ChunkFile.Config.VertexFormat.ToString(), Vector3.Zero, Matrix4x4.Identity, mesh, texturesByMaterial[0]);
            }
            
            Scene.Init(renderer.Context);
            //TODO: Make version that attempts to auto position cam based on size of chunk (if it has the necessary data)
            Scene.Camera!.TargetPosition = new Vector3(-29.884623f, 36.636642f, -35.14956f);
            renderer.ActiveScenes.Add(Scene);
            SceneInitialized = true;

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
        finally
        {
            if (preloadedParentDirectory)
            {
                PackfileVFS.UnloadDirectory(parentPreloadPath);
            }
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
        updateParams.CameraControlsEnabled = this.Focused;
        Scene.Update(updateParams);
    }

    private RenderChunk CreateRenderChunkFromDestroyable(RenderContext context, Mesh mesh, Destroyable destroyable, List<Texture2D[]> texturesByMaterial)
    {
        RenderChunk chunk = Scene.CreateRenderChunk(mesh.Config.VertexFormat.ToString(), Vector3.Zero, Matrix4x4.Identity, mesh, texturesByMaterial, destroyable);
        return chunk;
    }

    private static Texture2D? TryLoadTextureFromTgaName(string tgaName, FileEntry chunkFileEntry, RenderContext context, CommandPool pool, Queue queue)
    {
        try
        {
            if (chunkFileEntry.Parent is not DirectoryEntry parent)
            {
                Log.Error($"Passed chunk file entry with no parent directory into TryLoadTextureFromTgaName(). This should never happen. Chunks should always be in a vpp_pc or str2_pc.");
                return null;
            }

            //Get file paths
            string pegCpuFilePath = TextureIndex.GetTexturePegPath(tgaName) ?? throw new Exception($"Chunk viewer failed to find peg path for texture {tgaName} in {chunkFileEntry.Name}");
            string pegGpuFilePath = new string(pegCpuFilePath);
            if (pegGpuFilePath.EndsWith(".cpeg_pc"))
                pegGpuFilePath = pegGpuFilePath.Replace(".cpeg_pc", ".gpeg_pc");
            if (pegGpuFilePath.EndsWith(".cvbm_pc"))
                pegGpuFilePath = pegGpuFilePath.Replace(".cvbm_pc", ".gvbm_pc");

            //Extract cpu file & gpu file
            using Stream cpuFile = PackfileVFS.OpenFile(pegCpuFilePath) ?? throw new Exception($"Importer failed to open peg cpu file at {pegCpuFilePath}");
            using Stream gpuFile = PackfileVFS.OpenFile(pegGpuFilePath) ?? throw new Exception($"Importer failed to open peg gpu file at {pegGpuFilePath}");
            
            PegReader reader = new();
            LogicalTextureArchive peg = reader.Read(cpuFile, gpuFile, tgaName, CancellationToken.None);
            foreach (LogicalTexture logicalTexture in peg.LogicalTextures)
            {
                if (!string.Equals(logicalTexture.Name, tgaName, StringComparison.OrdinalIgnoreCase))
                    continue;
                
                using var memoryStream = new MemoryStream();
                logicalTexture.Data.CopyTo(memoryStream);
                byte[] pixels = memoryStream.ToArray();

                Format pixelFormat = ProjectTexture.PegFormatToVulkanFormat(logicalTexture.Format, logicalTexture.Flags);

                uint numMipLevels = (uint)logicalTexture.MipLevels;
                //uint numMipLevels = 1;//(uint)logicalTexture.MipLevels;
                Texture2D texture = new(context, (uint)logicalTexture.Size.Width, (uint)logicalTexture.Size.Height, numMipLevels, pixelFormat, ImageTiling.Optimal,
                    ImageUsageFlags.TransferSrcBit | ImageUsageFlags.TransferDstBit | ImageUsageFlags.SampledBit,
                    MemoryPropertyFlags.DeviceLocalBit,
                    ImageAspectFlags.ColorBit);
                texture.SetPixels(pixels, pool, queue);
                texture.CreateTextureSampler();
                texture.CreateImageView();

                return texture;
            }
            
            return null;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading chunk texture.");
            return null;
        }
    }
    
    private static TextureType ClassifyTexture(string textureName)
    {
        //There's a few that don't follow the naming scheme
        List<string> diffuseTextureNameExceptions = 
        [
            "edf_mc_compmetal_01.tga", "edf_mc_compmetal_01.tga", "LargeSign_Mason_Wanted.tga", "locker_poster_WA.tga", "WA_Animated_Eos_Bits.tga", "LargeSign_Comp_FG.tga",
            "LargeSign_Mason.tga", "ore_mineral_decal.tga"
        ];
        
        if (textureName.EndsWith("_d.tga", StringComparison.OrdinalIgnoreCase) || diffuseTextureNameExceptions.Any(name => string.Equals(name, textureName, StringComparison.OrdinalIgnoreCase)))
        {
            return TextureType.Diffuse;
        }

        if (textureName.EndsWith("_s.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Specular;
        }

        if (textureName.EndsWith("_n.tga", StringComparison.OrdinalIgnoreCase) || string.Equals(textureName, "flat-normalmap.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Normal;
        }

        if (string.Equals(textureName, "missing.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Missing;
        }

        if (textureName.StartsWith("dcl_", StringComparison.OrdinalIgnoreCase) || textureName.Contains("_decal_", StringComparison.OrdinalIgnoreCase) || textureName.EndsWith("_x.tga", StringComparison.OrdinalIgnoreCase))
        {
            return TextureType.Decal;
        }

        return TextureType.Unknown;
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
}