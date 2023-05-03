using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;
using Nanoforge.Render.Resources;
using RfgTools.Formats.Meshes;
using Nanoforge.Misc;
using Nanoforge.Render;
using System.Linq;
using Bon;
using RfgTools.Formats;

namespace Nanoforge.Rfg
{
    //Root structure of any RFG map. Each territory is split into one or more zones which contain the map objects.
    [ReflectAll]
	public class Territory : EditorObject
	{
        public String PackfileName = new .() ~delete _;

        [EditorProperty]
        public List<Zone> Zones = new .() ~delete _;

        [BonKeepUnlessSet]
        public List<Rock> Rocks = new .() ~delete _;

        [BonKeepUnlessSet]
        public List<Chunk> Chunks = new .() ~delete _;

        public Result<void, StringView> Load(Renderer renderer, Scene scene)
        {
            //Track textures that've been read from disk during loading so they're only ever loaded once. Lets objects easily share the same texture in gpu memory
            Dictionary<ProjectTexture, Texture2D> textureCache = scope .();
            Dictionary<ProjectMesh, Mesh> meshCache = scope .();

            //Create render objects for low lod meshes
            for (Zone zone in Zones)
            {
                //Load zonewide terrain textures
                ZoneTerrain terrain = zone.Terrain;
                Texture2D combTexture = LoadTexture(terrain.CombTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                if (combTexture == null)
                {
                    Logger.Error("Zone loading error. Failed to load comb texture");
                    return .Err("Failed to load comb texture");
                }

                Texture2D ovlTexture = LoadTexture(terrain.OvlTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                if (ovlTexture == null)
                {
                    Logger.Error("Zone loading error. Failed to load ovl texture");
                    return .Err("Failed to load ovl texture");
                }

                Texture2D splatmap = LoadTexture(terrain.Splatmap, textureCache, renderer, scene).GetValueOrDefault(null);
                if (splatmap == null)
                {
                    Logger.Error("Failed to load splatmap while loading {}", zone.Name);
                    continue;
                }    

                //Load mesh for each subzone
                for (int i in 0 ... 8)
                {
                    Mesh lowLodMesh = LoadMesh(terrain.LowLodTerrainMeshes[i], meshCache, renderer, scene).GetValueOrDefault(null);
                    if (lowLodMesh == null)
                        continue;

                    switch (scene.CreateRenderObject("TerrainLowLod", lowLodMesh, terrain.Position, .Identity))
                    {
                        case .Ok(RenderObject obj):
                            obj.Visible = false; //Hide low lod terrain by default
                            obj.Textures[0] = combTexture;
                            obj.Textures[1] = ovlTexture;
                            obj.UseTextures = true;
                        case .Err:
                            Logger.Error("Failed to create render object for low lod terrain submesh {} of {}", i, zone.Name);
                            continue;
                    }
                }

                //Create render objects for high lod terrain meshes (terrain + stitch meshes + roads + rocks)
                for (int i in 0 ..< 9)
                {
                    //Load textures
                    Texture2D[8] subzoneTextures = .();
                    ZoneTerrain.Subzone subzone = terrain.Subzones[i];
                    for (int textureIndex in 0 ..< 8)
                    {
                        ProjectTexture projectTexture = subzone.SplatMaterialTextures[textureIndex];
                        subzoneTextures[textureIndex] = LoadTexture(projectTexture, textureCache, renderer, scene).GetValueOrDefault();
                        if (subzoneTextures[textureIndex] == null)
                        {
                            //Doesn't matter if it's null since the renderer will just skip it. It will only cause visual bugs so we let loading continue.
                            Logger.Warning("Failed to load {} for {} subzone {}", projectTexture.Name, zone.Name, i);
                        }
                    }

                    Mesh highLodMesh = LoadMesh(subzone.Mesh, meshCache, renderer, scene).GetValueOrDefault(null);
                    if (highLodMesh == null)
                        continue;

                    switch (scene.CreateRenderObject("Terrain", highLodMesh, subzone.Position, .Identity))
                    {
                        case .Ok(RenderObject obj):
                            obj.Visible = true; //Show high lod terrain by default
                            obj.Textures[0] = splatmap;
                            obj.Textures[1] = combTexture;
                            for (int textureIndex in 0 ..< 8)
                            {
                                obj.Textures[2 + textureIndex] = subzoneTextures[textureIndex];
                            }
                            obj.UseTextures = true;
                        case .Err:
                            Logger.Error("Failed to create render object for high lod terrain submesh {} of {}", i, zone.Name);
                            continue;
                    }

                    //Load stitch meshes
                    if (subzone.HasStitchMeshes)
                    {
                        Mesh stitchMesh = LoadMesh(subzone.StitchMesh, meshCache, renderer, scene).GetValueOrDefault(null);
                        if (stitchMesh == null)
                            continue;

                        switch (scene.CreateRenderObject("TerrainStitch", stitchMesh, subzone.Position, .Identity))
                        {
                            case .Ok(RenderObject obj):
                                obj.Visible = true;
                                obj.Textures[0] = splatmap;
                                obj.Textures[1] = combTexture;
                                for (int textureIndex in 0 ..< 8)
                                {
                                    obj.Textures[2 + textureIndex] = subzoneTextures[textureIndex];
                                }
                                obj.UseTextures = true;
                            case .Err:
                                Logger.Error("Failed to create render object for stitch mesh of terrain subzone {} of {}", i, zone.Name);
                                continue;
                        }
                    }
                }

                //Load chunks
                for (ZoneObject obj in zone.Objects)
                {
                    if (!obj.GetType().HasBaseType(typeof(ObjectMover)))
                        continue;

                    ObjectMover mover = obj as ObjectMover;
                    Chunk chunk = mover.ChunkData;
                    if (chunk == null)
                    {
                        continue;
                    }

                    //Load textures
                    Texture2D diffuse = LoadTexture(chunk.DiffuseTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                    if (diffuse == null)
                    {
                        Logger.Error("Failed to load diffuse texture for {}", chunk.Name);
                    }

                    Texture2D specular = LoadTexture(chunk.SpecularTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                    if (specular == null)
                    {
                        Logger.Error("Failed to load specular map for {}", chunk.Name);
                    }

                    Texture2D normal = LoadTexture(chunk.NormalTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                    if (normal == null)
                    {
                        Logger.Error("Failed to load normal map for {}", chunk.Name);
                    }    

                    //Get variant used by this object
                    u32 variantUID = mover.ChunkUID;
                    var variantSearch = chunk.Variants.Select((variant) => variant).Where((variant) => variant.VariantUID == variantUID);
                    if (variantSearch.Count() == 0)
                    {
                        continue;
                    }
                    ChunkVariant variant = variantSearch.First();

                    //Create render object
                    Mesh chunkMesh = LoadMesh(chunk.Mesh, meshCache, renderer, scene).GetValueOrDefault(null);
                    if (chunkMesh == null)
                        continue;

                    switch (scene.CreateRenderChunk(chunk.Mesh.VertexFormat.ToString(.. scope .()), chunkMesh, mover.Position, mover.Orient, variant))
                    {
                        case .Ok(RenderChunk obj):
                            obj.Visible = true;
                            obj.Textures[0] = diffuse;
                            obj.Textures[1] = specular;
                            obj.Textures[2] = normal;
                            obj.UseTextures = true;
                        case .Err:
                            delete chunkMesh;
                            Logger.Error("Failed to create render chunk for object {}", mover.Name);
                            continue;
                    }
                }
            }

            //Create meshes for rocks
            for (Rock rock in Rocks)
            {
                //Load textures
                Texture2D diffuse = LoadTexture(rock.DiffuseTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                if (diffuse == null)
                {
                    Logger.Error("Failed to load diffuse texture {} for {}", rock.DiffuseTexture.Name, rock.Name);
                    continue;
                }

                Texture2D normal = LoadTexture(rock.NormalTexture, textureCache, renderer, scene).GetValueOrDefault(null);
                if (normal == null)
                {
                    Logger.Error("Failed to load normal texture {} for {}", rock.NormalTexture.Name, rock.Name);
                    continue;
                }

                Mesh rockMesh = LoadMesh(rock.Mesh, meshCache, renderer, scene).GetValueOrDefault(null);
                if (rockMesh == null)
                    continue;

                switch (scene.CreateRenderObject("Rock", rockMesh, rock.Position, rock.Rotation))
                {
                    case .Ok(RenderObject obj):
                        obj.Visible = true;
                        obj.Textures[0] = diffuse;
                        obj.Textures[2] = normal;
                        obj.UseTextures = true;
                    case .Err:
                        delete rockMesh;
                        Logger.Error("Failed to create render object for rock mesh {}", rock.Name);
                        continue;
                }
            }

            return .Ok;
        }

        private Result<Texture2D> LoadTexture(ProjectTexture projectTexture, Dictionary<ProjectTexture, Texture2D> textureCache, Renderer renderer, Scene scene)
        {
            if (projectTexture == null)
                return .Err;

            if (textureCache.ContainsKey(projectTexture))
            {
                return textureCache[projectTexture];
            }
            else
            {
                Texture2D texture = projectTexture != null ? projectTexture.CreateRenderTexture(renderer.Device).GetValueOrDefault(null) : null;
                if (texture == null)
                {
                    return .Err;
                }
                else
                {
                    textureCache[projectTexture] = texture;
                    scene.Textures.Add(texture);
                    return texture;
                }
            }
        }

        private Result<Mesh> LoadMesh(ProjectMesh projectMesh, Dictionary<ProjectMesh, Mesh> meshCache, Renderer renderer, Scene scene)
        {
            if (projectMesh == null)
                return .Err;

            if (meshCache.ContainsKey(projectMesh))
            {
                return meshCache[projectMesh];
            }
            else
            {
                Mesh mesh = new .();
                if (mesh.Init(renderer.Device, projectMesh) case .Ok)
                {
                    meshCache[projectMesh] = mesh;
                    scene.Meshes.Add(mesh);
                    return mesh;
                }
                else
                {
                    Logger.Error("Failed to create renderer mesh for {}", projectMesh.Name);
                    delete mesh;
                    return .Err;
                }
            }
        }
	}

    //A cubic section of a map. Contains map objects
    [ReflectAll]
    public class Zone : EditorObject
    {
        [EditorProperty]
        public String District = new .() ~delete _;
        [EditorProperty]
        public u32 DistrictFlags = 0;
        [EditorProperty]
        public bool ActivityLayer = false;
        [EditorProperty]
        public bool MissionLayer = false;
        [EditorProperty]
        public List<ZoneObject> Objects = new .() ~delete _;
        [EditorProperty]
        public ZoneTerrain Terrain;
    }

    //Terrain data for a single map zone
    [ReflectAll]
    public class ZoneTerrain : EditorObject
    {
        [EditorProperty]
        public Vec3 Position = .Zero;

        public ProjectMesh[9] LowLodTerrainMeshes;
        public ProjectTexture CombTexture = null;
        public ProjectTexture OvlTexture = null;
        public ProjectTexture Splatmap = null;

        public Subzone[9] Subzones;

        //Terrain subzone. Each zone consists of 9 subzones
        [ReflectAll]
        public class Subzone : EditorObject
        {
            public ProjectMesh Mesh;
            //Splat material textures per subzone. The splatmap has 4 channels, so it supports 4 material textures.
            public ProjectTexture[8] SplatMaterialTextures;
            public Vec3 Position;

            public bool HasStitchMeshes = false;
            public ProjectMesh StitchMesh;
        }
    }

    [ReflectAll]
    public class ProjectMesh : EditorObject
    {
        public u32 NumVertices;
        public u8 VertexStride;
        public VertexFormat VertexFormat;
        public u32 NumIndices;
        public u8 IndexSize;
        public PrimitiveType PrimitiveType;
        public List<SubmeshData> Submeshes = new .() ~delete _;
        public List<RenderBlock> RenderBlocks = new .() ~delete _;

        public ProjectBuffer IndexBuffer;
        public ProjectBuffer VertexBuffer;

        public void InitFromRfgMeshConfig(MeshDataBlock config)
        {
            NumVertices = config.Header.NumVertices;
            VertexStride = config.Header.VertexStride0;
            VertexFormat = config.Header.VertexFormat;
            NumIndices = config.Header.NumIndices;
            IndexSize = config.Header.IndexSize;
            PrimitiveType = config.Header.PrimitiveType;
            config.Submeshes.CopyTo(Submeshes);
            config.RenderBlocks.CopyTo(RenderBlocks);
        }
    }

    [ReflectAll]
    public class Rock : EditorObject
    {
        public Vec3 Position = .Zero;
        public Mat3 Rotation = .Identity;
        public ProjectMesh Mesh;
        public ProjectTexture DiffuseTexture;
        public ProjectTexture NormalTexture;
    }

    //Data for a destructible object (mesh, textures, metadata). There's one instance of these per chunk file. Copies of a building RfgMover/ZoneObject would reference this
    [ReflectAll]
    public class Chunk : EditorObject
    {
        public ProjectMesh Mesh;
        public ProjectTexture DiffuseTexture;
        public ProjectTexture SpecularTexture;
        public ProjectTexture NormalTexture;
        public List<ChunkVariant> Variants = new .() ~delete _;
    }

    //Chunk files can hold multiple variants made by combining their subpieces in different ways. Referred to as "destroyables" in RfgTools.
    [ReflectAll]
    public class ChunkVariant : EditorObject
    {
        //TODO: Consider storing these in buffers if they bloat the projectDB files too much. They're only needed when creating the renderer object at the moment
        public u32 VariantUID; //Used by mover zone objects to identify which variant they're using.
        public List<PieceData> PieceData = new .() ~delete _;
        public List<PieceInstance> Pieces = new .() ~delete _;

        //Note: Pulls data from RfgTools Subpiece and Subpiece data. Only has the data NF needs at the moment
        [ReflectAll]
        public struct PieceData
        {
            public u16 RenderSubmesh; //TODO: Is this a render block index? UPDATE NAME BEFORE COMMIT!
        }

        //Pulls data from RfgTools Dlod
        [ReflectAll]
        public struct PieceInstance
        {
            public Vec3 Position;
            public Mat3 Orient;
            public u16 FirstSubmesh; //TODO: Update name. Might be first render block
            public u8 NumSubmeshes; //TODO: Same^^^^
        }
    }
}