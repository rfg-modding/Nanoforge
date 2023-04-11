using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;
using Nanoforge.Render.Resources;
using RfgTools.Formats.Meshes;
using Nanoforge.Misc;
using Nanoforge.Render;

namespace Nanoforge.Rfg
{
    //Root structure of any RFG map. Each territory is split into one or more zones which contain the map objects.
	public class Territory : EditorObject
	{
        public append String PackfileName;

        [EditorProperty]
        public append List<Zone> Zones = .();

        public Result<void, StringView> Load(Renderer renderer, Scene _scene)
        {
            //Create render objects for low lod meshes
            for (Zone zone in Zones)
            {
                //Load zonewide terrain textures
                Texture2D combTexture = null;
                Texture2D ovlTexture = null;
                ZoneTerrain terrain = zone.Terrain;
                if (terrain.CombTexture != null && terrain.OvlTexture != null)
                {
                    combTexture = terrain.CombTexture.CreateRenderTexture(renderer.Device).GetValueOrDefault(null);
                    ovlTexture = terrain.OvlTexture.CreateRenderTexture(renderer.Device).GetValueOrDefault(null);
                    if (combTexture == null)
                    {
                        Logger.Error("Zone loading error. Failed to load comb texture");
                        return .Err("Failed to load comb texture");
                    }
                    if (ovlTexture == null)
                    {
                        Logger.Error("Zone loading error. Failed to load ovl texture");
                        return .Err("Failed to load ovl texture");
                    }
                }
                else
                {
                    Logger.Warning("Failed to load comb and ovl textures for {}. They're null in the ProjectDB. Any error must've occurred during zone import.", zone.Name);
                }

                Texture2D splatmap = terrain.Splatmap != null ? terrain.Splatmap.CreateRenderTexture(renderer.Device).GetValueOrDefault(null) : null;
                if (splatmap == null)
                {
                    Logger.Error("Failed to load splatmap while loading {}", zone.Name);
                    continue;
                }    

                _scene.Textures.Add(splatmap);
                _scene.Textures.Add(combTexture);
                _scene.Textures.Add(ovlTexture);

                //Load mesh for each subzone
                for (int i in 0 ... 8)
                {
                    List<u8> indexBuffer = null;
                    List<u8> vertexBuffer = null;
                    defer { DeleteIfSet!(indexBuffer); }
                    defer { DeleteIfSet!(vertexBuffer); }
                    if (terrain.LowLodTerrainIndexBuffers[i].Load() case .Ok(let val))
                    {
            			indexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load index buffer for low lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }
                    if (terrain.LowLodTerrainVertexBuffers[i].Load() case .Ok(let val))
                    {
                    	vertexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load vertex buffer for low lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }

                    Mesh mesh = new .();
                    if (mesh.Init(renderer.Device, terrain.LowLodTerrainMeshConfig[i], indexBuffer, vertexBuffer) case .Ok)
                    {
                        switch (_scene.CreateRenderObject("TerrainLowLod", mesh, terrain.Position, .Identity))
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
                    else
                    {
                        Logger.Error("Failed to create render object for low lod terrain submesh {} of {}", i, zone.Name);
                        delete mesh;
                    }
                }

                //Create render objects for high lod terrain meshes (terrain + stitch meshes + roads + rocks)
                for (int i in 0 ..< 9)
                {
                    //Load textures
                    Texture2D[8] subzoneTextures;
                    ZoneTerrain.Subzone subzone = terrain.Subzones[i];
                    for (int textureIndex in 0 ..< 8)
                    {
                        //TODO: Track which textures have been loaded across all zones and subzones. Only allow each to be loaded once per map to save vram usage + speed up loading.
                        ProjectTexture texture = subzone.SplatMaterialTextures[textureIndex];
                        subzoneTextures[textureIndex] = texture.CreateRenderTexture(renderer.Device).GetValueOrDefault(null);
                        if (subzoneTextures[textureIndex] == null)
                        {
                            //Doesn't matter if it's null since the renderer will just skip it. It will only cause visual bugs so we let loading continue.
                            Logger.Warning("Failed to load {} for {} subzone {}", texture.GetName().GetValueOrDefault(""), zone.Name, i);
                        }
                        else
                        {
                            _scene.Textures.Add(subzoneTextures[textureIndex]);
                        }
                    }

                    List<u8> indexBuffer = null;
                    List<u8> vertexBuffer = null;
                    defer { DeleteIfSet!(indexBuffer); }
                    defer { DeleteIfSet!(vertexBuffer); }
                    if (subzone.IndexBuffer.Load() case .Ok(let val))
                    {
                		indexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load index buffer for high lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }
                    if (subzone.VertexBuffer.Load() case .Ok(let val))
                    {
                    	vertexBuffer = val;
                    }
                    else
                    {
                        Logger.Error("Failed to load vertex buffer for high lod terrain submesh {} of {}.", i, zone.Name);
                        continue;
                    }

                    Mesh mesh = new .();
                    if (mesh.Init(renderer.Device, subzone.MeshConfig, indexBuffer, vertexBuffer) case .Ok)
                    {
                        switch (_scene.CreateRenderObject("Terrain", mesh, subzone.Position, .Identity))
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
                    }
                    else
                    {
                        Logger.Error("Failed to create render object for high lod terrain submesh {} of {}", i, zone.Name);
                        delete mesh;
                    }

                    //Load stitch meshes
                    if (subzone.HasStitchMeshes)
                    {
                        List<u8> stitchIndexBuffer = null;
                        List<u8> stitchVertexBuffer = null;
                        defer { DeleteIfSet!(stitchIndexBuffer); }
                        defer { DeleteIfSet!(stitchVertexBuffer); }
                        if (subzone.StitchMeshIndexBuffer.Load() case .Ok(let val))
                        {
                        	stitchIndexBuffer = val;
                        }
                        else
                        {
                            Logger.Error("Failed to load stitch mesh index buffer for terrain subzone {} of {}.", i, zone.Name);
                            continue;
                        }
                        if (subzone.StitchMeshVertexBuffer.Load() case .Ok(let val))
                        {
                        	stitchVertexBuffer = val;
                        }
                        else
                        {
                            Logger.Error("Failed to load stitch mesh vertex buffer for terrain subzone {} of {}.", i, zone.Name);
                            continue;
                        }

                        Mesh stitchMesh = new .();
                        if (stitchMesh.Init(renderer.Device, subzone.StitchMeshConfig, stitchIndexBuffer, stitchVertexBuffer) case .Ok)
                        {
                            switch (_scene.CreateRenderObject("TerrainStitch", mesh, subzone.Position, .Identity))
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
                        else
                        {
                            Logger.Error("Failed to render object for stitch mesh of terrain subzone {} of {}", i, zone.Name);
                            delete mesh;
                        }
                    }
                }
            }

            return .Ok;
        }
	}

    //A cubic section of a map. Contains map objects
    public class Zone : EditorObject
    {
        [EditorProperty]
        public append String Name;
        [EditorProperty]
        public append String District;
        [EditorProperty]
        public u32 DistrictFlags = 0;
        [EditorProperty]
        public bool ActivityLayer = false;
        [EditorProperty]
        public bool MissionLayer = false;
        [EditorProperty]
        public append List<ZoneObject> Objects = .();
        [EditorProperty]
        public ZoneTerrain Terrain;
    }

    //Terrain data for a single map zone
    public class ZoneTerrain : EditorObject
    {
        [EditorProperty]
        public Vec3 Position = .Zero;

        public ProjectBuffer[9] LowLodTerrainIndexBuffers;
        public ProjectBuffer[9] LowLodTerrainVertexBuffers;
        //TODO: Get this working with the ProjectDB. Gonna need to convert to an EditorObject or change the system to support reflection on fields not marked with [EditorProperty]
        //      Will be part of another pass on the ProjectDB + undo/redo code once the vfs and rock + building importers are done.
        public MeshDataBlock[9] LowLodTerrainMeshConfig;

        public ProjectTexture CombTexture = null;
        public ProjectTexture OvlTexture = null;
        public ProjectTexture Splatmap = null;

        public Subzone[9] Subzones;

        public ~this()
        {
            for (int i in 0 ... 8)
	            delete LowLodTerrainMeshConfig[i];
        }

        //Terrain subzone. Each zone consists of 9 subzones
        public class Subzone : EditorObject
        {
            //Splat material textures per subzone. The splatmap has 4 channels, so it supports 4 material textures.
            public ProjectTexture [8] SplatMaterialTextures;
            public ProjectBuffer IndexBuffer;
            public ProjectBuffer VertexBuffer;
            public MeshDataBlock MeshConfig;
            public Vec3 Position;

            public bool HasStitchMeshes = false;
            public ProjectBuffer StitchMeshIndexBuffer;
            public ProjectBuffer StitchMeshVertexBuffer;
            public MeshDataBlock StitchMeshConfig;

            public ~this()
            {
                delete MeshConfig;
                delete StitchMeshConfig;
            }    
        }
    }
}