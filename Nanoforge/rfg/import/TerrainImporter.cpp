#include "Importers.h"
#include "util/Profiler.h"
#include "rfg/PackfileVFS.h"
#include "rfg/TextureIndex.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "Log.h"
#include <RfgTools++/formats/meshes/TerrainLowLod.h>
#include <RfgTools++/formats/meshes/MeshHelpers.h>
#include <RfgTools++/formats/meshes/Terrain.h>
#include <BinaryTools/BinaryReader.h>
#include <array>

//Used to stop the import process early by the calling code setting stopSignal to true. Does nothing if stopSignal is nullptr
#define WorkerEarlyStopCheck() if(stopSignal && *stopSignal) \
{ \
    return; \
} \

#define WorkerEarlyStopCheckReturnNullObj() if(stopSignal && *stopSignal) \
{ \
    return NullObjectHandle; \
} \

ObjectHandle ImportLowLodTerrain(std::string_view filename, std::span<u8> cpuFileBytes, std::span<u8> gpuFileBytes, PackfileVFS* packfileVFS, TextureIndex* textureIndex);
ObjectHandle ImportHighLodTerrain(std::string_view filename, std::span<u8> cpuFileBytes, std::span<u8> gpuFileBytes, const std::vector<string>&materialNames, PackfileVFS* packfileVFS, TextureIndex* textureIndex);
std::optional<std::vector<ObjectHandle>> LoadTerrainTextures(const std::vector<string>& materialNames, const string& subzoneName, TextureIndex* textureIndex);

ObjectHandle Importers::ImportTerrain(std::string_view terrainName, const Vec3& position, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal)
{
    PROFILER_FUNCTION();
    Registry& registry = Registry::Get();
    TerrainLowLod rfgTerrain;
    ObjectHandle zoneTerrain = registry.CreateObject(terrainName, "ZoneTerrain");
    zoneTerrain.Set<Vec3>("Position", position);
    zoneTerrain.Property("HighLod").SetObjectList({});

    //Load low lod terrain
    {
        PROFILER_SCOPED("Import low lod terrain");

        //Find files
        string lowLodTerrainName = string(terrainName) + ".cterrain_pc";
        auto lowLodTerrainCpuFileHandles = packfileVFS->GetFiles(lowLodTerrainName, true, true);
        if (lowLodTerrainCpuFileHandles.size() == 0)
        {
            LOG_ERROR("Low lod terrain files not found for {}", terrainName);
            return NullObjectHandle;
        }
        if (lowLodTerrainCpuFileHandles.size() > 1)
        {
            Log->warn("Found {} low lod terrain files for '{}'. Only should be finding one.", lowLodTerrainCpuFileHandles.size(), lowLodTerrainName);
        }

        //Get cpu & gpu file
        FileHandle terrainMesh = lowLodTerrainCpuFileHandles[0];
        std::optional<FilePair> filePair = terrainMesh.GetPair();
        if (!filePair)
        {
            LOG_ERROR("Failed to extract low lod terrain file pair from {}/{}", terrainMesh.ContainerName(), terrainMesh.Filename());
            return NullObjectHandle;
        }

        //Import
        ObjectHandle terrainLowLod = ImportLowLodTerrain(terrainMesh.Filename(), filePair.value().CpuFile, filePair.value().GpuFile, packfileVFS, textureIndex);
        if (!terrainLowLod)
        {
            LOG_ERROR("Failed to imporrt low lod terrain from {}/{}", terrainMesh.ContainerName(), terrainMesh.Filename());
            return NullObjectHandle;
        }
        zoneTerrain.Property("LowLod").Set(terrainLowLod);
    }
    WorkerEarlyStopCheckReturnNullObj();

    //Load high lod terrain
    {
        PROFILER_SCOPED("Import high lod terrain");
        std::mutex highLodTerrainMutex;
        std::vector<FileHandle> highLodSearchResult = packfileVFS->GetFiles(string(terrainName) + "_*", true);
        for (FileHandle& file : highLodSearchResult)
        {
            WorkerEarlyStopCheckReturnNullObj();
            if (file.Extension() != ".ctmesh_pc")
                continue;

            //Get cpu & gpu file
            PROFILER_SCOPED("Import high lod terrain mesh");
            std::optional<FilePair> filePair = file.GetPair();
            if (!filePair)
            {
                LOG_ERROR("Failed to extract high lod terrain file pair from {}/{}", file.ContainerName(), file.Filename());
                continue;
            }

            //Valid high lod terrain file found. Import it
            ObjectHandle terrainLowLod = zoneTerrain.Get<ObjectHandle>("LowLod");
            ObjectHandle terrainHighLod = ImportHighLodTerrain(file.Filename(), filePair.value().CpuFile, filePair.value().GpuFile, terrainLowLod.GetStringList("TerrainMaterialNames"), packfileVFS, textureIndex);
            if (terrainHighLod)
            {
                highLodTerrainMutex.lock();
                zoneTerrain.Property("HighLod").GetObjectList().push_back(terrainHighLod);
                highLodTerrainMutex.unlock();
            }
        }
    }

    return zoneTerrain;
}

ObjectHandle ImportLowLodTerrain(std::string_view filename, std::span<u8> cpuFileBytes, std::span<u8> gpuFileBytes, PackfileVFS* packfileVFS, TextureIndex* textureIndex)
{
    PROFILER_FUNCTION();
    Registry& registry = Registry::Get();
    TerrainLowLod rfgTerrain;
    ObjectHandle lowLodTerrain = registry.CreateObject(filename, "TerrainLowLod");
    string terrainName = Path::GetFileNameNoExtension(filename);

    //Parse cpu file
    BinaryReader cpuFile(cpuFileBytes);
    BinaryReader gpuFile(gpuFileBytes);
    rfgTerrain.Read(cpuFile, filename);
    lowLodTerrain.Property("Signature").Set<u32>(rfgTerrain.Signature);
    lowLodTerrain.Property("Version").Set<u32>(rfgTerrain.Version);
    lowLodTerrain.Property("TextureNames").SetStringList(rfgTerrain.TextureNames);
    lowLodTerrain.Property("StitchPieceNames").SetStringList(rfgTerrain.StitchPieceNames);
    lowLodTerrain.Property("FmeshNames").SetStringList(rfgTerrain.FmeshNames);
    lowLodTerrain.Property("TerrainMaterialNames").SetStringList(rfgTerrain.TerrainMaterialNames);
    lowLodTerrain.Property("LayerMapMaterialNames").SetStringList(rfgTerrain.LayerMapMaterialNames);
    lowLodTerrain.Property("LayerMapMaterialNames2").SetStringList(rfgTerrain.LayerMapMaterialNames2);
    lowLodTerrain.Property("MinimapMaterialNames").SetStringList(rfgTerrain.MinimapMaterialNames);
    //Note: Some of these like undergrowth data might be better suited as buffers
    //TODO: Import ::Materials
    //TODO: Import ::UndergrowthLayers
    //TODO: Import ::UndergrowthCellData
    //TODO: Import ::SingleUndergrowthCellData
    //TODO: Import ::SingleUndergrowthData
    //TODO: Import ::SidemapMaterials
    //TODO: Import ::MinimapMaterials

    //Load meshes
    lowLodTerrain.SetObjectList("Meshes", {});
    for (size_t i = 0; i < rfgTerrain.Meshes.size(); i++)
    {
        MeshDataBlock& meshHeader = rfgTerrain.Meshes[i]; //Has info like vertex and index formats + submesh vertices
        ObjectHandle mesh = Importers::ImportMeshHeader(meshHeader);
        if (!mesh)
        {
            LOG_ERROR("Failed to read mesh header for submesh {} in {}.", i, filename);
            return NullObjectHandle;
        }

        std::optional<MeshInstanceData> meshData = rfgTerrain.ReadMeshData(gpuFile, i);
        if (!meshData)
        {
            LOG_ERROR("Failed to read vertices + indices for submesh {} in {}.", i, filename);
            return NullObjectHandle;
        }

        BufferHandle indexBuffer = registry.CreateBuffer(meshData.value().IndexBuffer, fmt::format("{}_lowlod_{}_indices", terrainName, i));
        BufferHandle vertexBuffer = registry.CreateBuffer(meshData.value().VertexBuffer, fmt::format("{}_lowlod_{}_vertices", terrainName, i));
        mesh.Property("Indices").Set(indexBuffer);
        mesh.Property("Vertices").Set(vertexBuffer);

        lowLodTerrain.GetObjectList("Meshes").push_back(mesh);
    }

    //Load textures
    {
        //TODO: Add some kind of global texture caching/tracking so repeat loads can be prevented when importing zones/lowLodTerrain on many threads
        PROFILER_SCOPED("Import low lod terrain textures");
        string combTextureName = terrainName + "comb.tga";
        string ovlTextureName = terrainName + "_ovl.tga";
        std::optional<string> combPath = textureIndex->GetTexturePegPath(combTextureName);
        std::optional<string> ovlPath = textureIndex->GetTexturePegPath(ovlTextureName);
        if (combPath)
        {
            ObjectHandle peg = Importers::ImportPegFromPath(combPath.value(), textureIndex);
            ObjectHandle texture = GetPegEntry(peg, combTextureName);
            lowLodTerrain.Property("CombTexture").Set(texture); //Sets even if null so it can be checked for NullObjectHandle on future loads
        }
        if (ovlPath)
        {
            ObjectHandle peg = Importers::ImportPegFromPath(ovlPath.value(), textureIndex);
            ObjectHandle texture = GetPegEntry(peg, ovlTextureName);
            lowLodTerrain.Property("OvlTexture").Set(texture);
        }
    }

    return lowLodTerrain;
}

ObjectHandle ImportHighLodTerrain(std::string_view filename, std::span<u8> cpuFileBytes, std::span<u8> gpuFileBytes, const std::vector<string>& materialNames, PackfileVFS* packfileVFS, TextureIndex* textureIndex)
{
    PROFILER_FUNCTION();
    Registry& registry = Registry::Get();
    Terrain rfgTerrain;
    ObjectHandle highLodTerrain = registry.CreateObject(filename, "TerrainHighLod");
    string subzoneName = Path::GetFileNameNoExtension(filename);

    //Parse cpu file
    BinaryReader cpuFile(cpuFileBytes);
    BinaryReader gpuFile(gpuFileBytes);
    rfgTerrain.Read(cpuFile, filename);
    highLodTerrain.Set<u32>("Signature", rfgTerrain.Signature);
    highLodTerrain.Set<u32>("Version", rfgTerrain.Version);
    highLodTerrain.Set<u32>("Index", rfgTerrain.Index); //TODO: Is this RFG runtime only?
    highLodTerrain.Set<u32>("SubzoneIndex", rfgTerrain.Subzone.SubzoneIndex);
    highLodTerrain.Set<u32>("HeaderVersion", rfgTerrain.Subzone.HeaderVersion);
    highLodTerrain.Set<Vec3>("Position", rfgTerrain.Subzone.Position);
    highLodTerrain.Set<Vec3>("AabbMin", rfgTerrain.Subzone.RenderableData.AabbMin);
    highLodTerrain.Set<Vec3>("AabbMax", rfgTerrain.Subzone.RenderableData.AabbMax);
    highLodTerrain.Set<Vec3>("BspherePosition", rfgTerrain.Subzone.RenderableData.BspherePosition);
    highLodTerrain.Set<f32>("BsphereRadius", rfgTerrain.Subzone.RenderableData.BsphereRadius);
    highLodTerrain.SetStringList("StitchPieceNames", rfgTerrain.StitchPieceNames);
    highLodTerrain.SetStringList("StitchPieceNames2", rfgTerrain.StitchPieceNames2);

    //Read patches
    for (TerrainPatch& patch : rfgTerrain.Patches)
    {
        ObjectHandle patchObj = registry.CreateObject("", "TerrainPatch");
        patchObj.Set<u32>("InstanceOffset", patch.InstanceOffset); //TODO: Determine if this can be discarded. May be runtime only
        patchObj.Set<Vec3>("Position", patch.Position);
        patchObj.Set<Mat3>("Rotation", patch.Rotation);
        patchObj.Set<u32>("SubmeshIndex", patch.SubmeshIndex);
        patchObj.Set<Vec3>("LocalAabbMin", patch.LocalAabbMin);
        patchObj.Set<Vec3>("LocalAabbMax", patch.LocalAabbMax);
        patchObj.Set<Vec3>("LocalBspherePosition", patch.LocalBspherePosition);
        patchObj.Set<f32>("LocalBsphereRadius", patch.LocalBsphereRadius);

        highLodTerrain.GetObjectList("Patches").push_back(patchObj);
    }

    //Read stitch instance data
    for (TerrainStitchInstance& stitchInstance : rfgTerrain.StitchInstances)
    {
        ObjectHandle stitch = registry.CreateObject("", "StitchInstance");
        stitch.Set<Vec3>("Position", stitchInstance.Position); //Note: Other values on this struct ignored for being runtime only
        stitch.Set<Mat3>("Rotation", stitchInstance.Rotation);
        stitch.Set<u32>("HavokHandle", stitchInstance.HavokHandle);

        highLodTerrain.GetObjectList("StitchInstances").push_back(stitch);
    }

    //Load terrain mesh
    {
        PROFILER_SCOPED("Import terrain meshes");
        ObjectHandle mesh = Importers::ImportMeshHeader(rfgTerrain.TerrainMesh);
        if (!mesh)
        {
            LOG_ERROR("Failed to read terrain mesh header in {}.", filename);
            return NullObjectHandle;
        }

        std::optional<MeshInstanceData> meshData = rfgTerrain.ReadTerrainMeshData(gpuFile);
        if (!meshData)
        {
            LOG_ERROR("Failed to read terrain mesh vertices + indices {}.", filename);
            return NullObjectHandle;
        }

        BufferHandle indexBuffer = registry.CreateBuffer(meshData.value().IndexBuffer, fmt::format("{}_indices", subzoneName));
        BufferHandle vertexBuffer = registry.CreateBuffer(meshData.value().VertexBuffer, fmt::format("{}_vertices", subzoneName));
        mesh.Property("Indices").Set(indexBuffer);
        mesh.Property("Vertices").Set(vertexBuffer);

        highLodTerrain.Set<ObjectHandle>("Mesh", mesh);
    }

    //Load stitch meshes
    if (rfgTerrain.HasStitchMesh)
    {
        PROFILER_SCOPED("Import stitch meshes");
        ObjectHandle mesh = Importers::ImportMeshHeader(rfgTerrain.StitchMesh);
        if (!mesh)
        {
            LOG_ERROR("Failed to read terrain stitch mesh header in {}.", filename);
            return NullObjectHandle;
        }

        std::optional<MeshInstanceData> meshData = rfgTerrain.ReadStitchMeshData(gpuFile);
        if (!meshData)
        {
            LOG_ERROR("Failed to read terrain stitch mesh vertices + indices {}.", filename);
            return NullObjectHandle;
        }

        BufferHandle indexBuffer = registry.CreateBuffer(meshData.value().IndexBuffer, fmt::format("{}_stitch_indices", subzoneName));
        BufferHandle vertexBuffer = registry.CreateBuffer(meshData.value().VertexBuffer, fmt::format("{}_stitch_vertices", subzoneName));
        mesh.Property("Indices").Set(indexBuffer);
        mesh.Property("Vertices").Set(vertexBuffer);

        highLodTerrain.Set<ObjectHandle>("StitchMesh", mesh);
    }

    //Load road meshes
    highLodTerrain.SetObjectList("RoadMeshes", {});
    if (rfgTerrain.RoadMeshes.size() > 0 && rfgTerrain.RoadMeshes.size() == rfgTerrain.RoadMeshDatas.size() &&
        rfgTerrain.RoadMeshes.size() == rfgTerrain.RoadMaterials.size() && rfgTerrain.RoadMeshes.size() == rfgTerrain.RoadTextures.size())
    {
        PROFILER_SCOPED("Import road meshes");
        //Read vertices + indices
        std::optional<std::vector<MeshInstanceData>> meshData = rfgTerrain.ReadRoadMeshData(gpuFile);
        if (!meshData.has_value())
        {
            LOG_ERROR("Failed to read road mesh vertices + indices {}.", filename);
            return NullObjectHandle;
        }

        for (size_t i = 0; i < rfgTerrain.RoadMeshes.size(); i++)
        {
            PROFILER_SCOPED(fmt::format("Import road mesh {}", i).c_str())
            MeshDataBlock& roadMeshHeader = rfgTerrain.RoadMeshes[i];
            RoadMeshData& roadMeshData = rfgTerrain.RoadMeshDatas[i];
            RfgMaterial& roadMaterial = rfgTerrain.RoadMaterials[i]; //TODO: Import this
            std::vector<string>& roadTextures = rfgTerrain.RoadTextures[i];
            MeshInstanceData& meshInstance = meshData.value()[i];

            ObjectHandle roadMesh = Importers::ImportMeshHeader(roadMeshHeader);
            if (!roadMesh)
            {
                LOG_ERROR("Failed to read road mesh header {} in {}.", i, filename);
                continue; //Still attempt to load remaining meshes
            }
            roadMesh.Set<Vec3>("Position", roadMeshData.Position); //Note: Other values in this struct ignored due to being runtime only
            roadMesh.SetStringList("MaterialTextureNames", roadTextures);

            //Save vertices + indices
            BufferHandle indexBuffer = registry.CreateBuffer(meshInstance.IndexBuffer, fmt::format("{}_road_{}_indices", subzoneName, i));
            BufferHandle vertexBuffer = registry.CreateBuffer(meshInstance.VertexBuffer, fmt::format("{}_road_{}_vertices", subzoneName, i));
            roadMesh.Property("Indices").Set(indexBuffer);
            roadMesh.Property("Vertices").Set(vertexBuffer);

            highLodTerrain.GetObjectList("RoadMeshes").push_back(roadMesh);
        }
    }
    else if (rfgTerrain.RoadMeshes.size() > 0)
    {
        LOG_ERROR("Road mesh vector size mismatch for {}. Skipping road meshes. Sizes: {}, {}, {}, {}", filename, rfgTerrain.RoadMeshDatas.size(), rfgTerrain.RoadMeshes.size(), rfgTerrain.RoadMaterials.size(), rfgTerrain.RoadTextures.size());
        highLodTerrain.SetObjectList("RoadMeshes", {});
    }

    //Load terrain textures
    {
        PROFILER_SCOPED("Load terrain textures");
        std::optional<std::vector<ObjectHandle>> textures = LoadTerrainTextures(materialNames, subzoneName, textureIndex);
        if (!textures)
        {
            LOG_ERROR("Failed to load terrain textures for {}", subzoneName);
            return NullObjectHandle;
        }
        for (size_t i = 0; i < 10; i++)
        {
            string objName = fmt::format("Texture{}", i);
            highLodTerrain.Set<ObjectHandle>(objName, textures.value()[i]);
        }
    }

    //Load road textures
    for (ObjectHandle roadMesh : highLodTerrain.GetObjectList("RoadMeshes"))
    {
        PROFILER_SCOPED("Load road textures");

        std::optional<std::vector<ObjectHandle>> textures = LoadTerrainTextures(roadMesh.GetStringList("MaterialTextureNames"), subzoneName, textureIndex);
        if (!textures)
        {
            LOG_ERROR("Failed to load road textures for {}", subzoneName);
            return NullObjectHandle;
        }
        for (size_t i = 0; i < 10; i++)
        {
            string objName = fmt::format("Texture{}", i);
            roadMesh.Set<ObjectHandle>(objName, textures.value()[i]);
        }
    }

    return highLodTerrain;
}

std::optional<std::vector<ObjectHandle>> LoadTerrainTextures(const std::vector<string>& materialNames, const string& subzoneName, TextureIndex* textureIndex)
{
    std::vector<ObjectHandle> result = {};
    std::array<std::optional<string>, 8> materialPegPaths; //Up to 4 materials, each with one diffuse and normal map
    std::array<string, 8> materialTextureNames;
    string terrainName = subzoneName;
    terrainName.pop_back();
    terrainName.pop_back();

    //Load splatmap and comb texture
    string splatmapTextureName = terrainName + "_alpha00.tga";
    string combTextureName = terrainName + "comb.tga";
    std::optional<string> splatmapPath = textureIndex->GetTexturePegPath(splatmapTextureName);
    std::optional<string> combPath = textureIndex->GetTexturePegPath(combTextureName);
    ObjectHandle splatmapPeg = NullObjectHandle;
    ObjectHandle combPeg = NullObjectHandle;
    if (splatmapPath)
        splatmapPeg = Importers::ImportPegFromPath(splatmapPath.value(), textureIndex);
    if (combPath)
        combPeg = Importers::ImportPegFromPath(combPath.value(), textureIndex);

    result.push_back(GetPegEntry(splatmapPeg, splatmapTextureName));
    result.push_back(GetPegEntry(combPeg, combTextureName));

    //First get the texture names from the material list in the low lod terrain file
    //This is a stopgap measure until the nanoforge properly supports the RFG material system. Probably could be done easier using meshes built in material data.
    u32 terrainTextureIndex = 0;
    u32 numTerrainTextures = 0;
    while (terrainTextureIndex < materialNames.size() - 1 && numTerrainTextures < 4)
    {
        PROFILER_SCOPED("Find high lod texture");
        string current = materialNames[terrainTextureIndex];
        string next = materialNames[terrainTextureIndex + 1];

        //Ignored texture, skip. Likely used by game as a holdin when terrain has < 4 materials
        if (String::EqualIgnoreCase(current, "misc-white.tga"))
        {
            terrainTextureIndex += 2; //Skip 2 since it's always followed by flat-normalmap.tga, which is also ignored
            continue;
        }
        //These are ignored since we already load them by default
        if (String::Contains(current, "alpha00.tga") || String::Contains(current, "comb.tga"))
        {
            terrainTextureIndex++;
            continue;
        }

        //If current is a normal texture try to find the matching diffuse texture
        if (String::Contains(String::ToLower(current), "_n.tga"))
        {
            next = current;
            current = String::Replace(current, "_n", "_d");
        }
        else if (!String::Contains(String::ToLower(current), "_d.tga") || !String::Contains(String::ToLower(next), "_n.tga"))
        {
            //Isn't a diffuse or normal texture. Ignore
            terrainTextureIndex++;
            continue;
        }

        //Locate textures
        materialPegPaths[numTerrainTextures * 2] = textureIndex->GetTexturePegPath(current);
        materialTextureNames[numTerrainTextures * 2] = current;

        materialPegPaths[numTerrainTextures * 2 + 1] = textureIndex->GetTexturePegPath(next);
        materialTextureNames[numTerrainTextures * 2 + 1] = next;

        numTerrainTextures++;
        terrainTextureIndex += 2;
    }

    //Load found textures into buffers
    for (size_t i = 0; i < materialPegPaths.size(); i++)
    {
        PROFILER_SCOPED(fmt::format("Load texture {}", i).c_str());
        std::optional<string> texturePath = materialPegPaths[i];
        if (!texturePath)
        {
            result.push_back(NullObjectHandle);
            continue;
        }

        ObjectHandle peg = Importers::ImportPegFromPath(texturePath.value(), textureIndex);
        ObjectHandle texture = GetPegEntry(peg, materialTextureNames[i]);
#if DEBUG_BUILD
        if (!texture)
            Log->warn("Failed to get peg entry {} from {}", materialTextureNames[i], texturePath.value());
#endif
        result.push_back(texture);
    }

    return result;
}
