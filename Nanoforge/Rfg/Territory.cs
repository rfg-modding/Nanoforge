using System;
using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using Serilog;

namespace Nanoforge.Rfg;

public class Territory : EditorObject
{
    public Vector3 Position = Vector3.Zero;
    public string PackfileName = string.Empty;
    public bool Compressed = false;
    public bool Condensed = false;
    public bool Compacted => Compressed && Condensed;

    public List<Zone> Zones = new();

    public bool Load(Renderer renderer, Scene scene)
    {
        try
        {
            //TODO: Come up with a smarter way to give each thread a command pool or queue not used by another thread.
            //TODO: Right now we're assuming that this function is only called by one thread at a time, and that its a separate thread from the render thread. If thats the case its fine to use the transfer command pool
            //TODO: Maybe make a wrapper class for command pools / command buffers that knows which pool and queue it belongs to so less params need to get passed around.
            //TODO: Maybe even have a separate "ThreadRenderContext" that gets passed around
            Dictionary<ProjectMesh, Mesh> meshCache = new();
            
            //Load low lod terrain meshes
            foreach (Zone zone in Zones)
            {
                ZoneTerrain terrain = zone.Terrain ?? throw new Exception("Zone.Terrain is null");
                //TODO: Port code to load low lod terrain textures
                
                //One low lod mesh per subzone
                for (int i = 0; i < 9; i++)
                {
                    Mesh? lowLodMesh = LoadMesh(terrain.LowLodTerrainMeshes[i], meshCache, renderer, scene);
                    if (lowLodMesh == null)
                    {
                        Log.Error($"Error loading low lod terrain mesh {i} for {Name}");
                        continue;
                    }
                    
                    scene.CreateRenderObject("TerrainLowLod", terrain.Position, Matrix4x4.Identity, lowLodMesh, []);
                }
            }
            
            //TODO: Port code to load the rest of the meshes (high lod terrain, rocks, road, chunks)
            
            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading map from project files");
            return false;
        }
    }

    private Mesh? LoadMesh(ProjectMesh? projectMesh, Dictionary<ProjectMesh, Mesh> meshCache, Renderer renderer, Scene scene)
    {
        try
        {
            if (projectMesh == null)
                return null;
            if (meshCache.ContainsKey(projectMesh))
                return meshCache[projectMesh];

            byte[] vertices = projectMesh.VertexBuffer?.Load() ?? throw new Exception("Failed to load vertex buffer");
            byte[] indices = projectMesh.IndexBuffer?.Load() ?? throw new Exception("Failed to load index buffer");
            Mesh mesh = new(renderer.Context, vertices, indices, projectMesh.NumVertices, projectMesh.NumIndices, projectMesh.IndexSize, renderer.Context.TransferCommandPool, renderer.Context.TransferQueue);
            meshCache[projectMesh] = mesh;
            return mesh;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error loading project mesh {projectMesh?.Name ?? "NULL"}");
            return null;
        }
    }
    
    //TODO: Port the rest of this classes functions and fields
}

public class Chunk : EditorObject
{
    //TODO: Port
}