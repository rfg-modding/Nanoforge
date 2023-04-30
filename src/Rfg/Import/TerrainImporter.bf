using Common;
using System;
using Nanoforge.App;
using Nanoforge.Misc;
using RfgTools.Formats.Textures;
using System.IO;
using Nanoforge.FileSystem;
using Nanoforge.App.Project;
using System.Collections;
using RfgTools.Formats.Meshes;
using Common.IO;
using RfgTools.Formats;
using System.Linq;

namespace Nanoforge.Rfg.Import
{
	public static class TerrainImporter
	{
        public static Result<void> LoadTerrain(StringView packfileName, Territory territory, Zone zone, DiffUtil changes, StringView name)
        {
            Logger.Info("Importing primary terrain files...");

            ZoneTerrain terrain = changes.CreateObject<ZoneTerrain>(scope $"{name}.cterrain_pc");
            
            //Determine terrain position from obj_zone center
            for (ZoneObject obj in zone.Objects)
            {
                if (obj.Classname == "obj_zone")
                {
                    terrain.Position = obj.BBox.Center();
                    break;
                }
            }

            //Get cterrain_pc and gterrain_pc files
            Result<u8[]> cpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{name}.cterrain_pc");
            if (cpuFile case .Err)
            {
				return .Err;
			}
            defer delete cpuFile.Value;

            Result<u8[]> gpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{name}.gterrain_pc");
            if (gpuFile case .Err)
            {
                return .Err;
            }
            defer delete gpuFile.Value;

            Terrain terrainFile = scope .();
            if (terrainFile.Load(cpuFile.Value, gpuFile.Value, true) case .Err(StringView err))
            {
                Logger.Error("Failed to parse cterrain_pc file. Error: {}", err);
                return .Err;
            }

            //Save low lod meshes to buffers
            for (int i in 0 ... 8)
            {
                switch (terrainFile.GetMeshData(i))
                {
                    case .Ok(MeshInstanceData meshData):
                        ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{name}_low_lod_{i}_indices");
                        ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{name}_low_lod_{i}_vertices");
                        terrain.LowLodTerrainMeshConfig[i] = meshData.Config.Clone(.. new .());
                        terrain.LowLodTerrainIndexBuffers[i] = indexBuffer;
                        terrain.LowLodTerrainVertexBuffers[i] = vertexBuffer;
                    case .Err(StringView err):
                        Logger.Error("Failed to get mesh data from gterrain_pc file. Error: {}", err);
                        return .Err;
                }
            }
            Logger.Info("Done importing primary terrain files");

            //Index textures in this maps packfile + common file
            TextureIndex.IndexVpp(packfileName).IgnoreError();
            TextureIndex.IndexVpp("mp_common.vpp_pc").IgnoreError(); //TODO: Do this based on precache file name instead of hardcoding it

            //Load zone-wide textures
            {
                Logger.Info("Importing zone-wide textures...");
                ProjectTexture combTexture = GetOrLoadTerrainTexture(changes, scope $"{name}comb.tga").GetValueOrDefault(null);
                ProjectTexture ovlTexture = GetOrLoadTerrainTexture(changes, scope $"{name}_ovl.tga").GetValueOrDefault(null);
                ProjectTexture splatmap = GetOrLoadTerrainTexture(changes, scope $"{name}_alpha00.tga").GetValueOrDefault(null);
                if (combTexture == null || ovlTexture == null || splatmap == null)
                    return .Err;

                terrain.CombTexture = combTexture;
                terrain.OvlTexture = ovlTexture;
                terrain.Splatmap = splatmap;
                Logger.Info("Done importing zone-wide textures");
            }

            //Load subzones (ctmesh_pc|gtmesh_pc files). These have the high lod terrain meshes
            Logger.Info("Importing terrain subzones...");
            for (int i in 0 ... 8)
            {
                Logger.Info("Importing subzone {}...", i);
                Logger.Info("Loading subzone files...");
                Result<u8[]> subzoneCpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{name}_{i}.ctmesh_pc");
                if (subzoneCpuFile case .Err)
                {
                    Logger.Error("Failed to extract terrain subzone {}_{}.ctmesh_pc.", name, i);
                    return .Err;
                }
                defer delete subzoneCpuFile.Value;

                Result<u8[]> subzoneGpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{name}_{i}.gtmesh_pc");
                if (subzoneGpuFile case .Err)
                {
                    Logger.Error("Failed to extract terrain subzone {}_{}.gtmesh_pc.", name, i);
                    return .Err;
                }
                defer delete subzoneGpuFile.Value;

                Logger.Info("Parsing subzone files...");
                TerrainSubzone subzoneFile = scope .();
                if (subzoneFile.Load(subzoneCpuFile.Value, subzoneGpuFile.Value, true) case .Err(StringView err))
                {
                    Logger.Error("Failed to parse {}_{}.ctmesh_pc. Error: {}", name, i, err);
                    return .Err;
                }

                ZoneTerrain.Subzone subzone = changes.CreateObject<ZoneTerrain.Subzone>(scope $"{name}_{i}.ctmesh_pc");

                //Load terrain mesh
                Logger.Info("Extracting subzone terrain mesh...");
                switch (subzoneFile.GetTerrainMeshData())
                {
                    case .Ok(MeshInstanceData meshData):
                        ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{name}_high_lod_{i}_indices");
                        ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{name}_high_lod_{i}_vertices");
                        subzone.MeshConfig = meshData.Config.Clone(.. new .());
                        subzone.IndexBuffer = indexBuffer;
                        subzone.VertexBuffer = vertexBuffer;
                        subzone.Position = subzoneFile.Data.Position;
                    case .Err(StringView err):
                        Logger.Error("Failed to get high lod terrain mesh data from {}_{}.gtmesh_pc. Error: {}", name, i, err);
                        return .Err;
                }

                //Load stitch meshes
                Logger.Info("Extracting subzone terrain stitch mesh...");
                if (subzoneFile.HasStitchMesh)
                {
                    subzone.HasStitchMeshes = true;
                    switch (subzoneFile.GetStitchMeshData())
                    {
                        case .Ok(MeshInstanceData meshData):
                            ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{name}_high_lod_{i}_stitch_indices");
                            ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{name}_high_lod_{i}_stitch_vertices");
                            subzone.StitchMeshConfig = meshData.Config.Clone(.. new .());
                            subzone.StitchMeshIndexBuffer = indexBuffer;
                            subzone.StitchMeshVertexBuffer = vertexBuffer;
                        case .Err(StringView err):
                            Logger.Error("Failed to get terrain stitch mesh data from {}_{}.gtmesh_pc. Error: {}", name, i, err);
                            return .Err;
                    }
                }

                //TODO: Load road meshes

                //Load rock meshes
                Logger.Info("Importing rock meshes...");
                for (int stitchIndex in 0 ..< subzoneFile.StitchInstances.Length)
                {
                    ref TerrainStitchInstance stitchInstance = ref subzoneFile.StitchInstances[stitchIndex];
                    StringView stitchPieceName = subzoneFile.StitchPieceNames2[stitchIndex];
                    if (stitchPieceName.Contains("_Road", true))
                        continue; //Skip road stitch instance. Road mesh data is stored in the terrain file and loaded separately

                    var rockSearch = territory.Rocks.Select((rock) => rock).Where((rock) => rock.Name == stitchPieceName);
                    if (rockSearch.Count() > 0) //Rock mesh was already loaded. Create a new rock instance with the same mesh + textures, but different location & rotation
                    {
                        Rock cachedRock = rockSearch.First();
                        Rock rock = changes.CreateObject<Rock>(stitchPieceName);
                        rock.Position = stitchInstance.Position;
                        rock.Rotation = stitchInstance.Rotation;
                        rock.IndexBuffer = cachedRock.IndexBuffer;
                        rock.VertexBuffer = cachedRock.VertexBuffer;
                        rock.DiffuseTexture = cachedRock.DiffuseTexture;
                        rock.NormalTexture = cachedRock.NormalTexture;
                        rock.MeshConfig = cachedRock.MeshConfig.Clone(.. new .()); //TODO: Shouldn't be making a copy here once issue #36 completed
                        territory.Rocks.Add(rock);
                    }
                    else //First time loading the rock. Must load the rfg files and extract the meshes + textures
                    {
                        Result<u8[]> rockCpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{stitchPieceName}.cstch_pc");
                        if (rockCpuFile case .Err)
                        {
                            Logger.Error("Failed to extract {}.cstch_pc.", stitchPieceName);
                            return .Err;
                        }
                        defer delete rockCpuFile.Value;

                        Result<u8[]> rockGpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{stitchPieceName}.gstch_pc");
                        if (rockGpuFile case .Err)
                        {
                            Logger.Error("Failed to extract {}.gstch_pc.", stitchPieceName);
                            return .Err;
                        }
                        defer delete rockGpuFile.Value;

                        //Parse rock mesh cpu file
                        RockMesh rockMesh = scope .();
                        if (rockMesh.Load(rockCpuFile.Value, rockGpuFile.Value, true) case .Err(StringView err))
                        {
                            Logger.Error("Failed to parse {}.cstch_pc. Error: {}", name, err);
                            return .Err;
                        }

                        //Extract rock mesh
                        Rock rock = changes.CreateObject<Rock>(stitchPieceName);
                        switch (rockMesh.GetMeshData())
                        {
                            case .Ok(MeshInstanceData meshData):
                                ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{stitchPieceName}_indices");
                                ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{stitchPieceName}_vertices");
                                rock.Position = stitchInstance.Position;
                                rock.Rotation = stitchInstance.Rotation;
                                rock.MeshConfig = meshData.Config.Clone(.. new .());
                                rock.IndexBuffer = indexBuffer;
                                rock.VertexBuffer = vertexBuffer;
                            case .Err(StringView err):
                                Logger.Error("Failed to extract mesh data from {}.gstch_pc. Error: {}", stitchPieceName, err);
                                return .Err;
                        }

                        //Load rock textures
                        if (rockMesh.TextureNames.Count >= 2)
                        {
                            //TODO: See if we need to take a more advanced approach here. Maybe base it off of material data in / around MeshDataBlock
                            //TODO: One odd case to look at is pkr_lm_spike5 in mp_puncture. Has 2 diffuse and 2 normal maps. Only one submesh so it isn't applying them to different ones. Maybe blends them?

                            //For now we just assume the first texture is diffuse and the second is normal. Since that seems be the general truth for rock meshes.
                            rock.DiffuseTexture = GetOrLoadTerrainTexture(changes, rockMesh.TextureNames[0]).GetValueOrDefault(null);
                            rock.NormalTexture = GetOrLoadTerrainTexture(changes, rockMesh.TextureNames[1]).GetValueOrDefault(null);
                            if (rock.DiffuseTexture == null || rock.NormalTexture == null)
                                return .Err;
                        }

                        territory.Rocks.Add(rock);
                    }
                }

                //Load subzone textures
                Logger.Info("Extracting subzone terrain textures...");                
                {
                    //First we get a list of textures names from the main terrain file (cterrain_pc)
                    int terrainTextureIndex = 0;
                    int numTerrainTextures = 0;
                    List<String> terrainMaterialNames = terrainFile.TerrainMaterialNames;
                    while (terrainTextureIndex < terrainMaterialNames.Count - 1 && numTerrainTextures < 4)
        			{
                        String current = scope .(terrainMaterialNames[terrainTextureIndex]);
                        String next = scope .(terrainMaterialNames[terrainTextureIndex + 1]);

                        //Ignored texture, skip. Likely used by game as a holdin when terrain has < 4 materials
                        if (current.Equals("misc-white.tga", .OrdinalIgnoreCase))
                        {
                            terrainTextureIndex += 2; //Skip 2 since it's always followed by flat-normalmap.tga, which is also ignored
                            continue;
                        }
                        //These are ignored since we already load them by default
                        if (current.Contains("alpha00.tga", ignoreCase: true) || current.Contains("comb.tga", ignoreCase: true))
                        {
                            terrainTextureIndex++;
                            continue;
                        }

                        //If current is a normal texture try to find the matching diffuse texture
                        if (current.Contains("_n.tga", ignoreCase: true))
                        {
                            next = current;
                            current.Replace("_n", "_d");
                        }
                        else if (!current.Contains("_d.tga", ignoreCase: true) || !next.Contains("_n.tga", ignoreCase: true))
                        {
                            //Isn't a diffuse or normal texture. Ignore
                            terrainTextureIndex++;
                            continue;
                        }

                        ProjectTexture texture0 = GetOrLoadTerrainTexture(changes, current).GetValueOrDefault(null);
                        if (texture0 == null)
                        {
                            Logger.Warning("Error loading textures for subzone {} of {}. Cancelling texture loading for this subzone.", i, zone.Name);
                            continue;
                        }
                        ProjectTexture texture1 = GetOrLoadTerrainTexture(changes, next).GetValueOrDefault(null);
                        if (texture1 == null)
                        {
                            Logger.Warning("Error loading textures for subzone {} of {}. Cancelling texture loading for this subzone.", i, zone.Name);
                            continue;
                        }

                        switch (numTerrainTextures)
                        {
                            case 0:
                                subzone.SplatMaterialTextures[0] = texture0;
                                subzone.SplatMaterialTextures[1] = texture1;
                            case 1:
                                subzone.SplatMaterialTextures[2] = texture0;
                                subzone.SplatMaterialTextures[3] = texture1;
                            case 2:
                                subzone.SplatMaterialTextures[4] = texture0;
                                subzone.SplatMaterialTextures[5] = texture1;
                            case 3:
                                subzone.SplatMaterialTextures[6] = texture0;
                                subzone.SplatMaterialTextures[7] = texture1;
                            default:
                                Logger.Error("Terrain importer somehow got > 3 textures. This shouldn't be able to happen!");
                                break;
                        }

                        numTerrainTextures++;
                        terrainTextureIndex += 2;
        			}
                }

                terrain.Subzones[i] = subzone;
                Logger.Info("Done importing subzone {}", i);
            }
            Logger.Info("Done loading terrain subzones");

            zone.Terrain = terrain;
            return .Ok;
        }

        //Get an RFG texture. If the tga hasn't been imported already it'll attempt to locate the peg containing the texture and import it.
        //All other textures in the peg will also be imported
        public static Result<ProjectTexture> GetOrLoadTerrainTexture(DiffUtil changes, StringView tgaName)
        {
            //Check if the tga was already loaded
            ImportedTextures importedTextures = ProjectDB.FindOrCreate<ImportedTextures>("Global::ImportedTextures");
            if (importedTextures.GetTexture(tgaName) case .Ok(ProjectTexture texture))
        		return texture;

            //Locate the peg containing this tga
            String pegCpuFilePath = scope .();
            if (TextureIndex.GetTexturePegPath(tgaName, pegCpuFilePath) case .Err)
            {
                Logger.Error("Failed to import rfg texture {} during map import. Make sure the map vpp and precache vpp were indexed for textures", tgaName);
                return .Err;
            }

            //Get gpu file name
            String pegGpuFilePath = scope .()..Append(pegCpuFilePath);
            if (pegGpuFilePath.EndsWith(".cpeg_pc"))
                pegGpuFilePath.Replace(".cpeg_pc", ".gpeg_pc");
            if (pegGpuFilePath.EndsWith(".cvbm_pc"))
                pegGpuFilePath.Replace(".cvbm_pc", ".gvbm_pc");

            //Extract cpu file & gpu file
            u8[] cpuFileBytes = null; defer { DeleteIfSet!(cpuFileBytes); }
            u8[] gpuFileBytes = null; defer { DeleteIfSet!(gpuFileBytes); }
            switch (PackfileVFS.ReadAllBytes(pegCpuFilePath))
            {
                case .Ok(u8[] bytes):
                    cpuFileBytes = bytes;
                case .Err:
                    Logger.Error("Failed to extract cpu file for rfg texture {} during map import.", tgaName);
                    return .Err;
            }
            switch (PackfileVFS.ReadAllBytes(pegGpuFilePath))
            {
                case .Ok(u8[] bytes):
                    gpuFileBytes = bytes;
                case .Err:
                    Logger.Error("Failed to extract gpu file for rfg texture {} during map import.", tgaName);
                    return .Err;
            }

            //Parse peg
            PegV10 peg = scope .();
            if (peg.Load(cpuFileBytes, gpuFileBytes, true) case .Err(StringView err))
            {
                Logger.Error("Failed to parse peg file for rfg texture {} during map import. Error: {}", tgaName, err);
                return .Err;
            }

            //Import all the subtextures. Uses up more drive space but typically we're gonna need them all if we need one, and it saves us from extracting the peg from a container multiple times (slow)
            //Could add a setting to toggle this if it ends up bloating projects signficantly
            ProjectTexture result = null;
            String pegName = Path.GetFileName(pegCpuFilePath, .. scope .());
            for (int i in 0 ..< peg.Entries.Length)
            {
                ref PegV10.Entry entry = ref peg.Entries[i];
                char8* entryName = peg.GetEntryName(i).GetValueOrDefault("");
                if (entryName == null)
                    entryName = "";

                //Get entry pixel data
                Span<u8> entryPixels = null;
                switch (peg.GetEntryPixels(i))
                {
                    case .Ok(Span<u8> pixels):
                        entryPixels = pixels;
                    case .Err(StringView err):
                        Logger.Error("Failed to extract pixels for {} in {} during texture import. Error: {}", entryName, pegName, err);
                        return .Err;
                }

                ProjectTexture texture = changes.CreateObject<ProjectTexture>(.(entryName));
                texture.Name.Set(.(entryName)); //Temporary workaround to ProjectDB.Find() not working on uncommitted objects
                if (ProjectDB.CreateBuffer(entryPixels, .(entryName)) case .Ok(ProjectBuffer pixelBuffer)) //Save pixel data to project buffer for quick and easy access later
                {
        			texture.Data = pixelBuffer;
                    texture.Format = ProjectTexture.PegFormatToDxgiFormat(entry.Format, (u16)entry.Flags);
                    texture.Height = entry.Height;
                    texture.Width = entry.Width;
                    texture.NumMipLevels = entry.MipLevels;
        		}
                else
                {
                    Logger.Error("Failed to create projectDB pixel buffer for {}/{} during texture import.", pegName, entryName);
                    return .Err;
                }

                if (StringView.Equals(.(entryName), tgaName, true))
                {
                    result = texture;
                }

                //Track imported textures so we don't import multiple times
                importedTextures.AddTexture(texture);
            }

            //Did we successfully import the tga?
            if (result != null)
            {
                return .Ok(result);
            }
            else
            {
                Logger.Error("Failed to find {} in {}. The TextureIndex is likely be out of sync with your data folder.", tgaName, pegName);
                return .Err;
            }
        }
	}
}