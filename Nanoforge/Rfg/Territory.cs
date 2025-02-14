using System;
using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using RFGM.Formats.Meshes.Shared;
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
    
    public List<Rock> Rocks = new();
    
    public List<Chunk> Chunks = new();

    public bool Load(Renderer renderer, Scene scene)
    {
        try
        {
            //TODO: Come up with a smarter way to give each thread a command pool or queue not used by another thread.
            //TODO: Right now we're assuming that this function is only called by one thread at a time, and that its a separate thread from the render thread. If thats the case its fine to use the transfer command pool
            //TODO: Maybe make a wrapper class for command pools / command buffers that knows which pool and queue it belongs to so less params need to get passed around.
            //TODO: Maybe even have a separate "ThreadRenderContext" that gets passed around
            Dictionary<ProjectMesh, Mesh> meshCache = new();
            Dictionary<ProjectTexture, Texture2D> textureCache = new();
            
            //Load low lod terrain meshes
            foreach (Zone zone in Zones)
            {
                ZoneTerrain terrain = zone.Terrain ?? throw new Exception("Zone.Terrain is null");
                List<Texture2D> lowLodTextures = new List<Texture2D>();
                Texture2D? combTexture = LoadTexture(terrain.CombTexture, textureCache, renderer, scene);
                if (combTexture != null)
                {
                    lowLodTextures.Add(combTexture);
                }
                else
                {
                    combTexture = Texture2D.MissingTexture;
                    Log.Warning("Failed to load comb texture for {}. Terrain may look wrong.", Name);
                }

                Texture2D? ovlTexture = LoadTexture(terrain.OvlTexture, textureCache, renderer, scene);
                if (ovlTexture != null)
                {
                    lowLodTextures.Add(ovlTexture);
                }
                else
                {
                    ovlTexture = Texture2D.MissingTexture;
                    Log.Warning("Failed to load ovl texture for {}. Terrain may look wrong.", Name);
                }

                Texture2D? splatmap = LoadTexture(terrain.Splatmap, textureCache, renderer, scene);
                if (splatmap != null)
                {
                    lowLodTextures.Add(splatmap);
                }
                else
                {
                    splatmap = Texture2D.MissingTexture;
                    Log.Warning("Failed to load splatmap texture for {}. Terrain may look wrong.", Name);
                }

                //TODO: Always load these and add runtime toggle to switch between low/high lod terrain. Temporarily disabled in favor of high lod terrain until the toggle is added
                //One low lod mesh per subzone
                // for (int i = 0; i < 9; i++)
                // {
                //     Mesh? lowLodMesh = LoadMesh(terrain.LowLodTerrainMeshes[i], meshCache, renderer, scene);
                //     if (lowLodMesh == null)
                //     {
                //         Log.Error($"Error loading low lod terrain mesh {i} for {Name}");
                //         continue;
                //     }
                //     
                //     scene.CreateRenderObject("TerrainLowLod", terrain.Position, Matrix4x4.Identity, lowLodMesh, lowLodTextures.ToArray());
                // }
                
                //Load high lod terrain meshes & terrain stitch meshes
                for (int i = 0; i < 9; i++)
                {
                    Texture2D[] subzoneTextures = new Texture2D[10];
                    TerrainSubzone subzone = terrain.Subzones[i];
                    subzoneTextures[0] = splatmap;
                    subzoneTextures[1] = combTexture;
                    for (int textureIndex = 2; textureIndex < 10; textureIndex++)
                    {
                        ProjectTexture projectTexture = subzone.SplatMaterialTextures[textureIndex - 2];
                        Texture2D? texture = LoadTexture(projectTexture, textureCache, renderer, scene);
                        if (texture != null)
                        {
                            subzoneTextures[textureIndex] = texture;
                        }
                        else
                        {
                            if (textureIndex is 3 or 5 or 7 or 9)
                            {
                                subzoneTextures[textureIndex] =  Texture2D.FlatNormalMap;
                            }
                            else
                            {
                                subzoneTextures[textureIndex] =  Texture2D.MissingTexture;
                            }
                            Log.Error("Failed to create render object for high lod terrain submesh {} of {}", i, zone.Name);
                        }
                    }
                    
                    Mesh? highLodMesh = LoadMesh(subzone.Mesh, meshCache, renderer, scene);
                    if (highLodMesh is null)
                    {
                        Log.Error($"Failed to load high lod terrain mesh for subzone {i} of {zone.Name}");
                        continue;
                    }
                    scene.CreateRenderObject("Terrain", subzone.Position, Matrix4x4.Identity, highLodMesh, subzoneTextures);

                    //Load stitch mesh
                    if (subzone.HasStitchMeshes)
                    {
                        Mesh? stitchMesh = LoadMesh(subzone.StitchMesh, meshCache, renderer, scene);
                        if (stitchMesh is null)
                        {
                            Log.Error($"Failed to load high lod terrain stitch mesh for subzone {i} of {zone.Name}");
                            continue;
                        }
                        
                        scene.CreateRenderObject("TerrainStitch", subzone.Position, Matrix4x4.Identity, stitchMesh, subzoneTextures);
                    }
                }
                
                //TODO: Put road mesh loading logic here once road meshes are added
            }
            
            //TODO: Load rocks
            foreach (Rock rock in Rocks)
            {
                Texture2D? diffuse = LoadTexture(rock.DiffuseTexture, textureCache, renderer, scene);
                if (diffuse is null)
                {
                    diffuse = Texture2D.MissingTexture;
                    Log.Warning($"Failed to load diffuse texture for rock {rock.Name} for map {Name}");
                }

                //TODO: Add normal map support
                
                Mesh? rockMesh = LoadMesh(rock.Mesh, meshCache, renderer, scene);
                if (rockMesh is null)
                {
                    Log.Warning($"Failed to load rock mesh {rock.Name} for map {Name}");
                    continue;
                }

                scene.CreateRenderObject(rock.Mesh!.VertexFormat.ToString(), rock.Position, rock.Rotation, rockMesh, [diffuse]);
            }
            
            //TODO: Load chunks
            
            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading map from project files");
            return false;
        }
    }

    private Texture2D? LoadTexture(ProjectTexture? projectTexture, Dictionary<ProjectTexture, Texture2D> textureCache, Renderer renderer, Scene scene)
    {
        try
        {
            if (projectTexture == null)
                return null;
            if (textureCache.ContainsKey(projectTexture))
                return textureCache[projectTexture];

            Texture2D? texture = projectTexture.CreateRenderTexture(renderer, renderer.Context.TransferCommandPool, renderer.Context.TransferQueue);
            if (texture == null)
            {
                Log.Error("Failed to create render texture for project texture {}, UID: {}", projectTexture.Name, projectTexture.UID);
                return null;
            }
            
            textureCache[projectTexture] = texture;
            return texture;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error loading project texture {projectTexture?.Name ?? "NULL"}");
            return null;
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
            
            //TODO: TEMPORARY CHANGE - DO NOT COMMIT - MUST EDIT ALL CODE TO PASS ProjectMesh TO RENDERER ONCE CHUNK VIEWER IS WORKING WELL
            MeshInstanceData meshInstance = new MeshInstanceData(new MeshConfig()
            {
                NumVertices = projectMesh.NumVertices,
                NumIndices = projectMesh.NumIndices,
                IndexSize = projectMesh.IndexSize,
                Submeshes = projectMesh.Submeshes,
                RenderBlocks = projectMesh.RenderBlocks,
            }, vertices, indices);
            
            Mesh mesh = new(renderer.Context, meshInstance, renderer.Context.TransferCommandPool, renderer.Context.TransferQueue);
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
