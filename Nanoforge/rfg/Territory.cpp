#include "Territory.h"
#include "gui/GuiState.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include "util/TaskScheduler.h"
#include "Log.h"
#include "util/Profiler.h"
#include "util/RfgUtil.h"
#include "util/MeshUtil.h"
#include "gui/documents/PegHelpers.h"
#include "rfg/TextureIndex.h"
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include <synchapi.h> //For Sleep()
#include <ranges>
#include <RfgTools++/formats/packfiles/Packfile3.h>
#include "rfg/PackfileVFS.h"
#include "render/resources/Scene.h"

//Todo: Separate gui specific code into a different file or class
#include <IconsFontAwesome5_c.h>

void Territory::Init(PackfileVFS* packfileVFS, const string& territoryFilename, const string& territoryShortname, bool useHighLodTerrain)
{
    packfileVFS_ = packfileVFS;
    territoryFilename_ = territoryFilename;
    territoryShortname_ = territoryShortname;
    useHighLodTerrain_ = useHighLodTerrain;
}

Handle<Task> Territory::LoadAsync(Handle<Scene> scene, GuiState* state)
{
    loadThreadRunning_ = true;
    loadThreadShouldStop_ = false;

    //Create and queue territory load task
    Handle<Task> task = Task::Create(fmt::format("Loading {}...", territoryFilename_));
    TaskScheduler::QueueTask(task, std::bind(&Territory::LoadThread, this, task, scene, state));
    return task;
}

//Used by loading threads to stop early if loadThreadShouldStop_ == true
#define EarlyStopCheck() if(loadThreadShouldStop_) \
{ \
    state->ClearStatus(); \
    loadThreadRunning_ = false; \
    return; \
} \


void Territory::LoadThread(Handle<Task> task, Handle<Scene> scene, GuiState* state)
{
    PROFILER_FUNCTION();
    while (!packfileVFS_ || !packfileVFS_->Ready()) //Wait for packfile thread to finish.
    {
        Sleep(50);
    }

    EarlyStopCheck();
    LoadThreadTimer.Reset();
    state->SetStatus(ICON_FA_SYNC " Loading " + territoryShortname_ + "...", Working);

    //Get packfile with zone files
    Log->info("Loading territory data for {}...", territoryFilename_);
    Handle<Packfile3> packfile = packfileVFS_->GetPackfile(territoryFilename_);
    if (!packfile)
    {
        LOG_ERROR("Could not find territory file {} in data folder. Required to load the territory.", territoryFilename_);
        return;
    }

    //Get mission and activity zones (layer_pc files) if territory has any
    std::vector<FileHandle> missionLayerFiles = {};
    std::vector<FileHandle> activityLayerFiles = {};
    {
        PROFILER_SCOPED("Find mission and activity zones");
        if (String::Contains(territoryFilename_, "terr01"))
        {
            missionLayerFiles = packfileVFS_->GetFiles("missions.vpp_pc", "*.layer_pc", true, false);
            activityLayerFiles = packfileVFS_->GetFiles("activities.vpp_pc", "*.layer_pc", true, false);
        }
        else if (String::Contains(territoryFilename_, "dlc01"))
        {
            missionLayerFiles = packfileVFS_->GetFiles("dlcp01_missions.vpp_pc", "*.layer_pc", true, false);
            activityLayerFiles = packfileVFS_->GetFiles("dlcp01_activities.vpp_pc", "*.layer_pc", true, false);
        }
    }

    //Reserve enough space for all possible zones
    size_t maxZoneCount = packfile->Entries.size() + missionLayerFiles.size() + activityLayerFiles.size();
    ZoneFiles.reserve(maxZoneCount);
    TerrainInstances.reserve(maxZoneCount);

    //Spawn a thread for each zone
    std::vector<Handle<Task>> zoneLoadTasks;
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        const char* filename = packfile->EntryNames[i];
        string extension = Path::GetExtension(filename);
        if (extension != ".rfgzone_pc")
            continue;

        //Queue task to load this zone
        Handle<Task> zoneLoadTask = Task::Create(fmt::format("Loading {}...", filename));
        TaskScheduler::QueueTask(zoneLoadTask, std::bind(&Territory::LoadWorkerThread, this, zoneLoadTask, scene, state, packfile, filename));
        zoneLoadTasks.push_back(zoneLoadTask);
    }
    EarlyStopCheck();

    //Load mission zones
    EarlyStopCheck();
    {
        PROFILER_SCOPED("Load mission layers");
        for (FileHandle& layerFile : missionLayerFiles)
        {
            std::vector<u8> fileBuffer = layerFile.Get();
            BinaryReader reader(fileBuffer);

            ZoneFilesLock.lock();
            ZoneData& zoneFile = ZoneFiles.emplace_back();
            ZoneFilesLock.unlock();
            zoneFile.Name = Path::GetFileNameNoExtension(layerFile.ContainerName()) + " - " + Path::GetFileNameNoExtension(layerFile.Filename()).substr(7);
            zoneFile.Zone.SetName(zoneFile.Name);
            zoneFile.Zone.Read(reader);
            zoneFile.Zone.GenerateObjectHierarchy();
            zoneFile.MissionLayer = true;
            zoneFile.ActivityLayer = false;
            SetZoneShortName(zoneFile);
            if (String::StartsWith(zoneFile.Name, "p_"))
                zoneFile.Persistent = true;
        }
    }

    //Load activity zones
    EarlyStopCheck();
    {
        PROFILER_SCOPED("Load activity layers");
        for (FileHandle& layerFile : activityLayerFiles)
        {
            std::vector<u8> fileBuffer = layerFile.Get();
            BinaryReader reader(fileBuffer);

            ZoneFilesLock.lock();
            ZoneData& zoneFile = ZoneFiles.emplace_back();
            ZoneFilesLock.unlock();
            zoneFile.Name = Path::GetFileNameNoExtension(layerFile.ContainerName()) + " - " + Path::GetFileNameNoExtension(layerFile.Filename()).substr(7);
            zoneFile.Zone.SetName(zoneFile.Name);
            zoneFile.Zone.Read(reader);
            zoneFile.Zone.GenerateObjectHierarchy();
            zoneFile.MissionLayer = false;
            zoneFile.ActivityLayer = true;
            SetZoneShortName(zoneFile);
            if (String::StartsWith(zoneFile.Name, "p_"))
                zoneFile.Persistent = true;
        }
    }

    //Wait for all workers to finish
    {
        PROFILER_SCOPED("Wait for territory load worker threads");
        for (auto& loadTask : zoneLoadTasks)
            loadTask->Wait();
    }

    EarlyStopCheck();
    //Sort vector by object count for convenience
    std::sort(ZoneFiles.begin(), ZoneFiles.end(),
    [](const ZoneData& zoneA, const ZoneData& zoneB)
    {
        return zoneA.Zone.Header.NumObjects > zoneB.Zone.Header.NumObjects;
    });

    //Get zone name with most characters for UI purposes
    u32 longest = 0;
    for (auto& zone : ZoneFiles)
    {
        if (zone.ShortName.length() > longest)
            longest = (u32)zone.ShortName.length();
    }
    LongestZoneName = longest;

    //Draw bounding boxes for zone with the most objects
    if (ZoneFiles.size() > 0 && ZoneFiles[0].Zone.Objects.size() > 0)
    {
        ZoneFiles[0].RenderBoundingBoxes = true;
    }

    //Init object class data used for filtering and labelling
    EarlyStopCheck();
    InitObjectClassData();

    //Clear texture cache so only render objects that use them hold references to them
    textureCache_.clear();

    Log->info("Done loading {}. Took {} seconds", territoryFilename_, LoadThreadTimer.ElapsedSecondsPrecise());
    state->ClearStatus();
    loadThreadRunning_ = false;
    ready_ = true;
}

void Territory::LoadWorkerThread(Handle<Task> task, Handle<Scene> scene, GuiState* state, Handle<Packfile3> packfile, const char* zoneFilename)
{
    EarlyStopCheck();
    PROFILER_FUNCTION();

    //Load zone data
    ZoneFilesLock.lock();
    ZoneData& zoneFile = ZoneFiles.emplace_back();
    ZoneFilesLock.unlock();
    {
        PROFILER_SCOPED("Load zone");
        std::optional<std::vector<u8>> fileBuffer = packfile->ExtractSingleFile(zoneFilename);
        if (!fileBuffer)
        {
            LOG_ERROR("Failed to extract zone file \"{}\" from \"{}\".", Path::GetFileName(string(zoneFilename)), territoryFilename_);
            return;
        }

        BinaryReader reader(fileBuffer.value());
        zoneFile.Name = Path::GetFileName(std::filesystem::path(zoneFilename));
        zoneFile.Zone.SetName(zoneFile.Name);
        zoneFile.Zone.Read(reader);
        zoneFile.Zone.GenerateObjectHierarchy();
        SetZoneShortName(zoneFile);
        if (String::StartsWith(zoneFile.Name, "p_"))
            zoneFile.Persistent = true;
    }

    //Check if the zone has terrain by looking for an obj_zone object with terrain_file_name property
    EarlyStopCheck();
    auto* objZoneObject = zoneFile.Zone.GetSingleObject("obj_zone");
    if (!objZoneObject)
        return; //No terrain

    auto* terrainFilenameProperty = objZoneObject->GetProperty<StringProperty>("terrain_file_name");
    if (!terrainFilenameProperty)
        return; //No terrain

    //Remove extra null terminators if present
    string terrainName = terrainFilenameProperty->Data;
    if (terrainName.ends_with('\0'))
        terrainName.pop_back();

    //Create new terrain instance
    TerrainLock.lock();
    TerrainInstance& terrain = TerrainInstances.emplace_back();
    TerrainLock.unlock();
    terrain.Name = terrainName;
    terrain.Position = objZoneObject->Bmin + ((objZoneObject->Bmax - objZoneObject->Bmin) / 2.0f);

    {
        PROFILER_SCOPED("Load low lod terrain")
        std::vector<MeshInstanceData> lowLodMeshes = { };

        std::optional<Texture2D> texture0 = {};
        std::optional<Texture2D> texture1 = {};

        string lowLodTerrainName = terrainName + ".cterrain_pc";
        auto lowLodTerrainCpuFileHandles = state->PackfileVFS->GetFiles(lowLodTerrainName, true, true);
        if (lowLodTerrainCpuFileHandles.size() == 0)
        {
            Log->info("Low lod terrain files not found for {}. Halting loading for that zone.", zoneFile.Name);
            return;
        }
        FileHandle terrainMesh = lowLodTerrainCpuFileHandles[0];

        //Get packfile that holds terrain meshes
        Handle<Packfile3> container = terrainMesh.GetContainer();
        if (!container)
        {
            LOG_ERROR("Failed to extract container for a low lod terrain mesh \"{}\"", lowLodTerrainName);
            return;
        }

        //Extract mesh file
        std::optional<std::vector<u8>> cpuFileBytes = container->ExtractSingleFile(terrainMesh.Filename(), true);
        std::optional<std::vector<u8>> gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc", true);
        if (!cpuFileBytes)
        {
            LOG_ERROR("Failed to extract low lod terrain mesh cpu file \"{}\"", terrainMesh.Filename());
            return;
        }
        if (!gpuFileBytes)
        {
            LOG_ERROR("Failed to extract lod lod terrain mesh gpu file \"{}\"", Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc");
            return;
        }

        //Parse mesh
        BinaryReader cpuFile(cpuFileBytes.value());
        BinaryReader gpuFile(gpuFileBytes.value());
        terrain.DataLowLod.Read(cpuFile, terrainMesh.Filename());

        //Get vertex data. Each terrain file is made up of 9 meshes which are stitched together
        for (u32 i = 0; i < 9; i++)
        {
            EarlyStopCheck();
            std::optional<MeshInstanceData> meshData = terrain.DataLowLod.ReadMeshData(gpuFile, i);
            if (!meshData.has_value())
            {
                LOG_ERROR("Failed to read submesh {} from {}.", i, terrainMesh.Filename());
                continue;
            }

            lowLodMeshes.push_back(meshData.value());
        }

        //Load comb and ovl textures, used to color and light low lod terrain, respectively.
        EarlyStopCheck();
        texture0 = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, terrainName + "comb.tga");
        texture1 = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, terrainName + "_ovl.tga");

        //Initialize low lod terrain
        for (u32 i = 0; i < lowLodMeshes.size(); i++)
        {
            Handle<RenderObject> renderObject = scene->CreateRenderObject("TerrainLowLod", Mesh{ scene->d3d11Device_, lowLodMeshes[i] }, terrain.Position);
            TerrainLock.lock();
            terrain.LowLodMeshes.push_back(renderObject);
            TerrainLock.unlock();
            renderObject->Visible = !useHighLodTerrain_;
            renderObject->UseTextures = texture0.has_value() || texture1.has_value();
            renderObject->Textures[0] = texture0;
            renderObject->Textures[1] = texture1;
        }
    }

    EarlyStopCheck();
    if(useHighLodTerrain_)
    {
        PROFILER_SCOPED("Load high lod terrain");
        std::vector<MeshInstanceData> highLodMeshes = { };
        std::vector<std::tuple<u32, MeshInstanceData>> stitchMeshes = { }; //First part of tuple is subzone index, since not all of them have stitches
        std::vector<std::tuple<u32, u32, MeshInstanceData>> roadMeshes = { }; //(submesh index, road mesh index, mesh data)

        std::optional<Texture2D> blendTexture = {}; //Used to blend up to 4 other textures on high lod terrain
        std::optional<Texture2D> combTexture = {}; //Used to adjust terrain colors
        std::array<std::optional<Texture2D>, 8> materialTextures; //Up to 4 materials, each with one diffuse and normal map

        //Search for high lod terrain mesh files. Should be 9 of them per zone.
        u32 curSubzoneIndex = 0;
        std::vector<FileHandle> highLodSearchResult = state->PackfileVFS->GetFiles(terrainName + "_*", true);
        for (FileHandle& file : highLodSearchResult)
        {
            if (Path::GetExtension(file.Filename()) != ".ctmesh_pc")
                continue;

            //Valid high lod terrain file found. Load and parse it
            Handle<Packfile3> container = file.GetContainer();
            if (!container)
            {
                LOG_ERROR("Failed to get container pointer for a high lod terrain mesh {}\\{}", file.ContainerName(), file.Filename());
                continue;
            }

            //Extract high lod mesh
            std::optional<std::vector<u8>> cpuFileBytes = container->ExtractSingleFile(file.Filename(), true);
            std::optional<std::vector<u8>> gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(file.Filename()) + ".gtmesh_pc", true);
            if (!cpuFileBytes)
            {
                LOG_ERROR("Failed to extract high lod terrain mesh cpu file from {}/{}", file.ContainerName(), file.Filename());
                continue;
            }
            if (!gpuFileBytes)
            {
                LOG_ERROR("Failed to extract high lod terrain mesh gpu file from {}/{}", file.ContainerName(), file.Filename());
                continue;
            }

            //Parse high lod mesh
            BinaryReader cpuFile(cpuFileBytes.value());
            BinaryReader gpuFile(gpuFileBytes.value());
            Terrain& subzone = terrain.Subzones.emplace_back();
            subzone.Read(cpuFile, file.Filename());

            //Read terrain mesh data
            std::optional<MeshInstanceData> meshData = subzone.ReadTerrainMeshData(gpuFile);
            if (!meshData)
            {
                LOG_ERROR("Failed to read terrain mesh data for {}. Halting loading of subzone.", file.Filename());
                continue;
            }
            highLodMeshes.push_back(meshData.value());

            //Load stitch meshes
            std::optional<MeshInstanceData> stitchMeshData = subzone.ReadStitchMeshData(gpuFile);
            if (stitchMeshData)
                stitchMeshes.push_back({ curSubzoneIndex, stitchMeshData.value() });

            //Load road meshes
            std::optional<std::vector<MeshInstanceData>> roadMeshData = subzone.ReadRoadMeshData(gpuFile);
            if (roadMeshData)
            {
                for (u32 j = 0; j < roadMeshData.value().size(); j++)
                {
                    auto& roadMesh = roadMeshData.value()[j];
                    roadMeshes.push_back({ curSubzoneIndex, j, roadMesh });
                }
            }
            curSubzoneIndex++;
        }
        EarlyStopCheck();

        //Load blend texture
        blendTexture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, terrainName + "_alpha00.tga");
        combTexture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, terrainName + "comb.tga");

        //Locate and load high lod terrain textures
        u32 terrainTextureIndex = 0;
        u32 numTerrainTextures = 0;
        std::vector<string>& terrainTextureNames = terrain.DataLowLod.TerrainMaterialNames;
        while (terrainTextureIndex < terrainTextureNames.size() - 1 && numTerrainTextures < 4)
        {
            string current = terrainTextureNames[terrainTextureIndex];
            string next = terrainTextureNames[terrainTextureIndex + 1];

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

            //Load textures
            materialTextures[numTerrainTextures * 2] = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, current);
            materialTextures[numTerrainTextures * 2 + 1] = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, next);

            numTerrainTextures++;
            terrainTextureIndex += 2;
        }
        EarlyStopCheck();

        //Initialize high lod terrain
        for (u32 i = 0; i < 9; i++)
        {
            auto& subzone = terrain.Subzones[i];
            Handle<RenderObject> renderObject = scene->CreateRenderObject("Terrain", Mesh{scene->d3d11Device_, highLodMeshes[i]}, subzone.Subzone.Position);
            TerrainLock.lock();
            terrain.HighLodMeshes.push_back(renderObject);
            TerrainLock.unlock();

            //Set textures
            bool anyTextures = std::ranges::any_of(materialTextures, [](std::optional<Texture2D>& tex) { return tex.has_value(); });
            renderObject->UseTextures = anyTextures || blendTexture.has_value() || combTexture.has_value();
            renderObject->Textures[0] = blendTexture;
            renderObject->Textures[1] = combTexture;
            for (u32 j = 0; j < materialTextures.size(); j++)
                renderObject->Textures[j + 2] = materialTextures[j];

            scene->NeedsRedraw = true; //Redraw scene when new terrain meshes are added
        }
        EarlyStopCheck();

        //Initialize stitch meshes
        for (auto& stitch : stitchMeshes)
        {
            u32 subzoneIndex = std::get<0>(stitch);
            MeshInstanceData instanceData = std::get<1>(stitch);
            Terrain& subzone = terrain.Subzones[subzoneIndex];
            TerrainStitchInstance& roadStitch = subzone.StitchInstances.back(); //Final instance is for road stitch meshes
            Handle<RenderObject> stitchObject = scene->CreateRenderObject("TerrainStitch", Mesh{ scene->d3d11Device_, instanceData }, roadStitch.Position);

            TerrainLock.lock();
            terrain.StitchMeshes.push_back(StitchMesh{.Mesh = stitchObject, .SubzoneIndex = subzoneIndex });
            TerrainLock.unlock();

            //Set textures
            bool anyTextures = std::ranges::any_of(materialTextures, [](std::optional<Texture2D>& tex) { return tex.has_value(); });
            stitchObject->UseTextures = anyTextures || blendTexture.has_value() || combTexture.has_value();
            stitchObject->Textures[0] = blendTexture;
            stitchObject->Textures[1] = combTexture;
            for (u32 i = 0; i < materialTextures.size(); i++)
                stitchObject->Textures[i + 2] = materialTextures[i];
        }
        EarlyStopCheck();

        //Initialize road meshes
        for (auto& road : roadMeshes)
        {
            u32 subzoneIndex = std::get<0>(road);
            u32 roadIndex = std::get<1>(road);
            MeshInstanceData instanceData = std::get<2>(road);
            Terrain& subzone = terrain.Subzones[subzoneIndex];
            RoadMeshData& roadData = subzone.RoadMeshDatas[roadIndex];
            Handle<RenderObject> roadObject = scene->CreateRenderObject("TerrainRoad", Mesh{ scene->d3d11Device_, instanceData }, roadData.Position);

            TerrainLock.lock();
            terrain.RoadMeshes.push_back(RoadMesh{ .Mesh = roadObject, .SubzoneIndex = subzoneIndex });
            TerrainLock.unlock();

            //Locate and load road textures
            std::array<std::optional<Texture2D>, 8> roadTextures; //Up to 4 materials, each with one diffuse and normal map
            u32 roadTextureIndex = 0;
            u32 numRoadTextures = 0;
            std::vector<string>& roadTextureNames = subzone.RoadTextures[roadIndex];
            while (roadTextureIndex < roadTextureNames.size() - 1 && numRoadTextures < 4)
            {
                string current = roadTextureNames[roadTextureIndex];
                string next = roadTextureNames[roadTextureIndex + 1];

                //Ignored texture, skip. Likely used by game as a holdin when terrain has < 4 materials
                if (String::EqualIgnoreCase(current, "misc-white.tga"))
                {
                    roadTextureIndex += 2; //Skip 2 since it's always followed by flat-normalmap.tga, which is also ignored
                    continue;
                }
                //These are ignored since we already load them by default
                if (String::Contains(current, "alpha00.tga") || String::Contains(current, "comb.tga"))
                {
                    roadTextureIndex++;
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
                    roadTextureIndex++;
                    continue;
                }

                //Load textures
                roadTextures[numRoadTextures * 2] = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, current);
                roadTextures[numRoadTextures * 2 + 1] = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, next);

                numRoadTextures++;
                roadTextureIndex += 2;
            }

            //Set textures
            bool anyRoadTextures = std::ranges::any_of(roadTextures, [](std::optional<Texture2D>& tex) { return tex.has_value(); });
            roadObject->UseTextures = anyRoadTextures || blendTexture.has_value() || combTexture.has_value();
            roadObject->Textures[0] = blendTexture;
            roadObject->Textures[1] = combTexture;
            for (u32 i = 0; i < roadTextures.size(); i++)
                roadObject->Textures[i + 2] = roadTextures[i];
        }
    }

    EarlyStopCheck();
}

std::optional<Texture2D> Territory::LoadTexture(ComPtr<ID3D11Device> d3d11Device, TextureIndex* textureSearchIndex, const string& textureName)
{
    PROFILER_FUNCTION();

    //Check if the texture was already loaded
    auto cacheSearch = textureCache_.find(String::ToLower(textureName));
    if (cacheSearch != textureCache_.end())
        return cacheSearch->second;

    //Todo: Pass optional container* to func
        //Todo: Check if container contains target texture
        //Todo: Can loop through peg/vbm files in container and use TextureIndex to check if they contain the target texture
        //Todo: If true, load peg and create DX11 texture for it.
            //Todo: Check perf difference. Look for other areas to improve perf in tracy
            //Todo: Consider making function to generate Texture2D from Peg and other ptrs. Use here and in TextureIndex

    //Load texture
    std::optional<Texture2D> texture = textureSearchIndex->GetRenderTexture(textureName, d3d11Device);
    if (texture.has_value()) //Cache texture if successful
        textureCache_[String::ToLower(textureName)] = texture.value();

    return texture;
}

bool Territory::ShouldShowObjectClass(u32 classnameHash)
{
    for (const auto& objectClass : ZoneObjectClasses)
    {
        if (objectClass.Hash == classnameHash)
            return objectClass.Show;
    }
    return true;
}

bool Territory::ObjectClassRegistered(u32 classnameHash, u32& outIndex)
{
    outIndex = InvalidZoneIndex;
    u32 i = 0;
    for (const auto& objectClass : ZoneObjectClasses)
    {
        if (objectClass.Hash == classnameHash)
        {
            outIndex = i;
            return true;
        }
        i++;
    }
    return false;
}

void Territory::UpdateObjectClassInstanceCounts()
{
    //Zero instance counts for each object class
    for (auto& objectClass : ZoneObjectClasses)
        objectClass.NumInstances = 0;

    //Update instance count for each object class in visible zones
    for (auto& zoneFile : ZoneFiles)
    {
        if (!zoneFile.RenderBoundingBoxes)
            continue;

        auto& zone = zoneFile.Zone;
        for (auto& object : zone.Objects)
        {
            for (auto& objectClass : ZoneObjectClasses)
            {
                if (objectClass.Hash == object.ClassnameHash)
                {
                    objectClass.NumInstances++;
                    break;
                }
            }
        }
    }

    //Sort vector by instance count for convenience
    std::sort(ZoneObjectClasses.begin(), ZoneObjectClasses.end(),
    [](const ZoneObjectClass& classA, const ZoneObjectClass& classB)
    {
        return classA.NumInstances > classB.NumInstances;
    });
}

void Territory::InitObjectClassData()
{
    ZoneObjectClasses =
    {
        {"rfg_mover",                      2898847573, 0, Vec3{ 0.923f, 0.648f, 0.0f }, true ,   false, ICON_FA_HOME " "},
        {"cover_node",                     3322951465, 0, Vec3{ 1.0f, 0.0f, 0.0f },     false,   false, ICON_FA_SHIELD_ALT " "},
        {"navpoint",                       4055578105, 0, Vec3{ 1.0f, 0.968f, 0.0f },   false,   false, ICON_FA_LOCATION_ARROW " "},
        {"general_mover",                  1435016567, 0, Vec3{ 0.738f, 0.0f, 0.0f },   true ,   false, ICON_FA_CUBES  " "},
        {"player_start",                   1794022917, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_STREET_VIEW " "},
        {"multi_object_marker",            1332551546, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_MAP_MARKER " "},
        {"weapon",                         2760055731, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_CROSSHAIRS " "},
        {"object_action_node",             2017715543, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_RUNNING " "},
        {"object_squad_spawn_node",        311451949,  0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_USERS " "},
        {"object_npc_spawn_node",          2305434277, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_USER " "},
        {"object_guard_node",              968050919,  0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_SHIELD_ALT " "},
        {"object_path_road",               3007680500, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_ROAD " "},
        {"shape_cutter",                   753322256,  0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_CUT " "},
        {"item",                           27482413,   0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TOOLS " "},
        {"object_vehicle_spawn_node",      3057427650, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_CAR_SIDE " "},
        {"ladder",                         1620465961, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_LEVEL_UP_ALT " "},
        {"constraint",                     1798059225, 0, Vec3{ 0.958f, 0.0f, 1.0f },   true ,   false, ICON_FA_LOCK " "},
        {"object_effect",                  2663183315, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_FIRE " "},
        //Todo: Want a better icon for this one
        {"trigger_region",                 2367895008, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BORDER_STYLE " "},
        {"object_bftp_node",               3005715123, 0, Vec3{ 1.0f, 1.0f, 1.0f },     false,   false, ICON_FA_BOMB " "},
        {"object_bounding_box",            2575178582, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BORDER_NONE " "},
        {"object_turret_spawn_node",       96035668,   0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_CROSSHAIRS " "},
        //Todo: Want a better icon for this one
        {"obj_zone",                       3740226015, 0, Vec3{ 0.935f, 0.0f, 1.0f },     true ,   false, ICON_FA_SEARCH_LOCATION " "},
        {"object_patrol",                  3656745166, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BINOCULARS " "},
        {"object_dummy",                   2671133140, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_MEH_BLANK " "},
        {"object_raid_node",               3006762854, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_CAR_CRASH " "},
        {"object_delivery_node",           1315235117, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_SHIPPING_FAST " "},
        {"marauder_ambush_region",         1783727054, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_USER_NINJA " "},
        {"unknown",                        0, 0,          Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_QUESTION_CIRCLE " "},
        {"object_activity_spawn",          2219327965, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_SCROLL " "},
        {"object_mission_start_node",      1536827764, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_MAP_MARKED " "},
        {"object_demolitions_master_node", 3497250449, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_BOMB " "},
        {"object_restricted_area",         3157693713, 0, Vec3{ 1.0f, 0.0f, 0.0f },     true ,   true,  ICON_FA_USER_SLASH " "},
        {"effect_streaming_node",          1742767984, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   true,  ICON_FA_SPINNER " "},
        {"object_house_arrest_node",       227226529,  0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_USER_LOCK " "},
        {"object_area_defense_node",       2107155776, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_USER_SHIELD " "},
        {"object_safehouse",               3291687510, 0, Vec3{ 0.0f, 0.905f, 1.0f },     true ,   false, ICON_FA_FIST_RAISED " "},
        {"object_convoy_end_point",        1466427822, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TRUCK_MOVING " "},
        {"object_courier_end_point",       3654824104, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_FLAG_CHECKERED " "},
        {"object_riding_shotgun_node",     1227520137, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_TRUCK_MONSTER " "},
        {"object_upgrade_node",            2502352132, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   false, ICON_FA_ARROW_UP " "},
        {"object_ambient_behavior_region", 2407660945, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TREE " "},
        {"object_roadblock_node",          2100364527, 0, Vec3{ 0.25f, 0.177f, 1.0f },  false,   true,  ICON_FA_HAND_PAPER " "},
        {"object_spawn_region",            1854373986, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   true,  ICON_FA_USER_PLUS " "},
        {"obj_light",                      2915886275, 0, Vec3{ 1.0f, 1.0f, 1.0f },     true ,   true,  ICON_FA_LIGHTBULB " "}
    };

    for (auto& zone : ZoneFiles)
    {
        for (auto& object : zone.Zone.Objects)
        {
            u32 outIndex = InvalidZoneIndex;
            if (!ObjectClassRegistered(object.ClassnameHash, outIndex))
            {
                ZoneObjectClasses.push_back({ object.Classname, object.ClassnameHash, 0, Vec3{ 1.0f, 1.0f, 1.0f }, true });
                Log->warn("Found unknown object class with hash {} and name \"{}\"", object.ClassnameHash, object.Classname);
            }
        }
    }
}

ZoneObjectClass& Territory::GetObjectClass(u32 classnameHash)
{
    for (auto& objectClass : ZoneObjectClasses)
    {
        if (objectClass.Hash == classnameHash)
            return objectClass;
    }

    //Todo: Handle case of invalid hash. Returning std::optional would work
    THROW_EXCEPTION("Failed to find object class with classname hash {}", classnameHash);
}

void Territory::SetZoneShortName(ZoneData& zone)
{
    const string& fullName = zone.Name;
    zone.ShortName = fullName; //Set shortname to fullname by default in case of failure
    const string expectedPostfix0 = ".rfgzone_pc";
    const string expectedPostfix1 = ".layer_pc";
    const string expectedPrefix = zone.Persistent ? "p_" + territoryShortname_ + "_" : territoryShortname_ + "_";

    //Try to find location of rfgzone_pc or layer_pc extension. If neither is found return
    size_t postfixIndex0 = fullName.find(expectedPostfix0, 0);
    size_t postfixIndex1 = fullName.find(expectedPostfix1, 0);
    size_t finalPostfixIndex = string::npos;
    if (postfixIndex0 != string::npos)
        finalPostfixIndex = postfixIndex0;
    else if (postfixIndex1 != string::npos)
        finalPostfixIndex = postfixIndex1;
    else
        return;

    //Make sure the name starts with the expected prefix
    if (!String::StartsWith(fullName, expectedPrefix))
        return;

    //For some files there is nothing between the prefix and postfix. We preserve the prefix in this case so something remains
    if (finalPostfixIndex == expectedPrefix.length())
    {
        zone.ShortName = fullName.substr(0, finalPostfixIndex); //Remove the postfix, keep the rest
    }
    else //Otherwise we strip the prefix and postfix and use the remaining string as our shortname
    {
        zone.ShortName = fullName.substr(expectedPrefix.length(), finalPostfixIndex - expectedPrefix.length()); //Keep string between prefix and postfix
    }

    //Add p_ prefix back onto persistent files
    if (zone.Persistent)
        zone.ShortName = "p_" + zone.ShortName;
}
