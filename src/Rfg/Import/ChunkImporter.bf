using Common;
using Common.IO;
using System;
using Nanoforge.App.Project;
using System.Linq;
using Nanoforge.FileSystem;
using RfgTools.Formats.Meshes;
using Nanoforge.Misc;
using Nanoforge.App;
using RfgTools.Formats.Textures;
using System.IO;

namespace Nanoforge.Rfg.Import
{
	public static class ChunkImporter
	{
        public static Result<Chunk> LoadChunk(StringView packfileName, Territory territory, Zone zone, DiffUtil changes, StringView chunkName)
        {
            //Get files
            Result<u8[]> cpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{chunkName}.cchk_pc");
            if (cpuFile case .Err)
            {
            	return .Err;
            }
            defer delete cpuFile.Value;

            Result<u8[]> gpuFile = PackfileVFS.ReadAllBytes(scope $"//data/{packfileName}/ns_base.str2_pc/{chunkName}.gchk_pc");
            if (gpuFile case .Err)
            {
                return .Err;
            }
            defer delete gpuFile.Value;

            //Parse
            ChunkMesh chunkFile = scope .();
            if (chunkFile.Load(cpuFile.Value, gpuFile.Value, true) case .Err(StringView err))
            {
                Logger.Error("Failed to load rfg chunk file '{}'. Error: {}", chunkName, err);
                return .Err;
            }

            //Import metadata from the chunk files
            Chunk chunk = changes.CreateObject<Chunk>(chunkName);
            for (Destroyable destroyable in chunkFile.Destroyables)
            {
                var variant = changes.CreateObject<ChunkVariant>(destroyable.Name);
                for (var subpieceData in ref destroyable.SubpieceData)
                {
                    ChunkVariant.PieceData data = .();
                    data.RenderSubmesh = subpieceData.RenderSubpiece;
					variant.PieceData.Add(data);
				}
                for (var dlod in ref destroyable.Dlods)
                {
                    ChunkVariant.PieceInstance instance = .();
                    instance.Position = dlod.Pos;
                    instance.Orient = dlod.Orient;
                    instance.FirstSubmesh = dlod.FirstPiece;
                    instance.NumSubmeshes = dlod.MaxPieces;
                    variant.Pieces.Add(instance);
                }
                chunk.Variants.Add(variant);
            }

            //Load mesh buffers
            switch (chunkFile.GetMeshData())
            {
                case .Ok(MeshInstanceData meshData):
                    ProjectBuffer indexBuffer = ProjectDB.CreateBuffer(meshData.IndexBuffer, scope $"{chunkName}_indices");
                    ProjectBuffer vertexBuffer = ProjectDB.CreateBuffer(meshData.VertexBuffer, scope $"{chunkName}_vertices");
                    ProjectMesh mesh = changes.CreateObject<ProjectMesh>(scope $"{chunkName}.cchk_pc");
                    mesh.InitFromRfgMeshConfig(meshData.Config);
                    mesh.IndexBuffer = indexBuffer;
                    mesh.VertexBuffer = vertexBuffer;
                    chunk.Mesh = mesh;
                case .Err(StringView err):
                    Logger.Error("Failed to get mesh data from gterrain_pc file. Error: {}", err);
                    return .Err;
            }

            //TODO: This isn't sufficient. Need to load all the materials/textures specified in the chunk file and apply them to each submesh as necessary
            //Load textures
            var diffuseSearch = chunkFile.Textures.Select((tgaName) => tgaName).Where((tgaName) => tgaName.Contains("_d", true));
            var specularSearch = chunkFile.Textures.Select((tgaName) => tgaName).Where((tgaName) => tgaName.Contains("_s", true));
            var normalSearch = chunkFile.Textures.Select((tgaName) => tgaName).Where((tgaName) => tgaName.Contains("_n", true));
            if (diffuseSearch.Count() > 0)
                chunk.DiffuseTexture = GetOrLoadChunkTexture(changes, diffuseSearch.First()).GetValueOrDefault(null);
            if (specularSearch.Count() > 0)
	            chunk.SpecularTexture = GetOrLoadChunkTexture(changes, specularSearch.First()).GetValueOrDefault(null);
            if (normalSearch.Count() > 0)
	            chunk.NormalTexture = GetOrLoadChunkTexture(changes, normalSearch.First()).GetValueOrDefault(null);

            return chunk;
        }

        //TODO: Stick this a importer helper class/file & remove GetOrLoadTerrainTexture from TerrainImporter.bf. They're identical
        //Get an RFG texture. If the tga hasn't been imported already it'll attempt to locate the peg containing the texture and import it.
        //All other textures in the peg will also be imported
        public static Result<ProjectTexture> GetOrLoadChunkTexture(DiffUtil changes, StringView tgaName)
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