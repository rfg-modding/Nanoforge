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
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include <synchapi.h> //For Sleep()
#include <ranges>

//Todo: Separate gui specific code into a different file or class
#include <IconsFontAwesome5_c.h>

void Territory::Init(PackfileVFS* packfileVFS, const string& territoryFilename, const string& territoryShortname)
{
    packfileVFS_ = packfileVFS;
    territoryFilename_ = territoryFilename;
    territoryShortname_ = territoryShortname;
}

Handle<Task> Territory::LoadAsync(GuiState* state)
{
    loadThreadRunning_ = true;
    loadThreadShouldStop_ = false;

    //Create and queue territory load task
    Handle<Task> task = Task::Create(fmt::format("Loading {}...", territoryFilename_));
    TaskScheduler::QueueTask(task, std::bind(&Territory::LoadThread, this, task, state));
    return task;
}

void Territory::ClearLoadThreadData()
{
    Log->info("Temporary data cleared for {} load threads.", territoryShortname_);
    for (auto& instance : TerrainInstances)
    {
        //Free vertex and index buffers
        for (auto& mesh : instance.LowLodMeshes)
        {
            if (mesh.IndexBuffer.data())
                delete[] mesh.IndexBuffer.data();
            if (mesh.VertexBuffer.data())
                delete[] mesh.VertexBuffer.data();
        }
        instance.LowLodMeshes.clear();

        for (auto& subzone : instance.Subzones)
        {
            if (subzone.InstanceData.IndexBuffer.data())
                delete[] subzone.InstanceData.IndexBuffer.data();
            if (subzone.InstanceData.VertexBuffer.data())
                delete[] subzone.InstanceData.VertexBuffer.data();
        }

        //Clear blend texture data
        if (instance.HasBlendTexture)
        {
            //Cleanup peg texture data
            instance.BlendTextureBytes = std::span<u8>((u8*)nullptr, 0);
            instance.BlendPeg.Cleanup(); //Clears data the span pointed to
            instance.Texture1Bytes = std::span<u8>((u8*)nullptr, 0);
            instance.Texture1.Cleanup(); //Clears data the span pointed to
        }
    }
}

//Used by loading threads to stop early if loadThreadShouldStop_ == true
#define EarlyStopCheck() if(loadThreadShouldStop_) \
{ \
    state->ClearStatus(); \
    loadThreadRunning_ = false; \
    return; \
} \


void Territory::LoadThread(Handle<Task> task, GuiState* state)
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
    Packfile3* packfile = packfileVFS_->GetPackfile(territoryFilename_);
    if (!packfile)
        THROW_EXCEPTION("Could not find territory file {} in data folder. Required for the program to function.", territoryFilename_);

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

    /*Todo: If lock contention becomes a performance issue construct the ZoneData instances locally and add them to ZoneFiles after loading is finished.
            Note: You'll need to fix copying behavior for ZoneData first, likely by implementing copy and move constructors. Currently they get corrupted on copy.*/
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
        TaskScheduler::QueueTask(zoneLoadTask, std::bind(&Territory::LoadWorkerThread, this, zoneLoadTask, state, packfile, filename));
        zoneLoadTasks.push_back(zoneLoadTask);
    }
    EarlyStopCheck();

    //Load mission zones
    EarlyStopCheck();
    {
        PROFILER_SCOPED("Load mission layers");
        for (auto& layerFile : missionLayerFiles)
        {
            auto fileBuffer = layerFile.Get();
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

            delete[] fileBuffer.data();
        }
    }

    //Load activity zones
    EarlyStopCheck();
    {
        PROFILER_SCOPED("Load activity layers");
        for (auto& layerFile : activityLayerFiles)
        {
            auto fileBuffer = layerFile.Get();
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

            delete[] fileBuffer.data();
        }
    }

    //Wait for all workers to finish
    {
        PROFILER_SCOPED("Wait for territory load worker threads");
        for (auto& task : zoneLoadTasks)
            task->Wait();
    }

    EarlyStopCheck();
    //Sort vector by object count for convenience
    std::sort(ZoneFiles.begin(), ZoneFiles.end(),
    [](const ZoneData& a, const ZoneData& b)
    {
        return a.Zone.Header.NumObjects > b.Zone.Header.NumObjects;
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

    Log->info("Done loading {}. Took {} seconds", territoryFilename_, LoadThreadTimer.ElapsedSecondsPrecise());
    state->ClearStatus();
    loadThreadRunning_ = false;
    ready_ = true;
}

void Territory::LoadWorkerThread(Handle<Task> task, GuiState* state, Packfile3* packfile, const char* zoneFilename)
{
    EarlyStopCheck();
    PROFILER_FUNCTION();

    //Load zone data
    ZoneFilesLock.lock();
    ZoneData& zoneFile = ZoneFiles.emplace_back();
    ZoneFilesLock.unlock();
    {
        PROFILER_SCOPED("Load zone");
        auto fileBuffer = packfile->ExtractSingleFile(zoneFilename);
        if (!fileBuffer)
            THROW_EXCEPTION("Failed to extract zone file \"{}\" from \"{}\".", Path::GetFileName(string(zoneFilename)), territoryFilename_);

        BinaryReader reader(fileBuffer.value());
        zoneFile.Name = Path::GetFileName(std::filesystem::path(zoneFilename));
        zoneFile.Zone.SetName(zoneFile.Name);
        zoneFile.Zone.Read(reader);
        zoneFile.Zone.GenerateObjectHierarchy();
        SetZoneShortName(zoneFile);
        if (String::StartsWith(zoneFile.Name, "p_"))
            zoneFile.Persistent = true;

        delete[] fileBuffer.value().data();
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

    //{
    //    PROFILER_SCOPED("Load low lod terrain")
    //    string lowLodTerrainName = terrainName + ".cterrain_pc";
    //    auto lowLodTerrainCpuFileHandles = state->PackfileVFS->GetFiles(lowLodTerrainName, true, true);
    //    if (lowLodTerrainCpuFileHandles.size() == 0)
    //    {
    //        Log->info("Low lod terrain files not found for {}. Halting loading for that zone.", zoneFile.Name);
    //        return;
    //    }

    //    FileHandle terrainMesh = lowLodTerrainCpuFileHandles[0];
    //    Vec3 position = objZoneObject->Bmin + ((objZoneObject->Bmax - objZoneObject->Bmin) / 2.0f);

    //    //Get packfile that holds terrain meshes
    //    auto* container = terrainMesh.GetContainer();
    //    if (!container)
    //        THROW_EXCEPTION("Failed to get container pointer for a terrain mesh.");

    //    //Get mesh file byte arrays
    //    auto cpuFileBytes = container->ExtractSingleFile(terrainMesh.Filename(), true);
    //    auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc", true);

    //    //Ensure the mesh files were extracted
    //    if (!cpuFileBytes)
    //        THROW_EXCEPTION("Failed to extract terrain mesh cpu file.");
    //    if (!gpuFileBytes)
    //        THROW_EXCEPTION("Failed to extract terrain mesh gpu file.");

    //    BinaryReader cpuFile(cpuFileBytes.value());
    //    BinaryReader gpuFile(gpuFileBytes.value());

    //    terrain.Position = position;
    //    terrain.DataLowLod.Read(cpuFile, terrainMesh.Filename());

    //    //Get vertex data. Each terrain file is made up of 9 meshes which are stitched together
    //    for (u32 i = 0; i < 9; i++)
    //    {
    //        EarlyStopCheck();
    //        std::optional<MeshInstanceData> meshData = terrain.DataLowLod.ReadMeshData(gpuFile, i);
    //        if (!meshData.has_value())
    //            THROW_EXCEPTION("Failed to read submesh {} from {}.", i, terrainMesh.Filename());

    //        terrain.LowLodMeshes.push_back(meshData.value());
    //    }

    //    //Clear resources
    //    delete container;
    //    delete[] cpuFileBytes.value().data();
    //    delete[] gpuFileBytes.value().data();
    //    EarlyStopCheck();

    //    //Get comb texture. Used to color low lod terrain
    //    string blendTextureName = Path::GetFileNameNoExtension(terrainMesh.Filename()) + "comb.cvbm_pc";
    //    terrain.HasBlendTexture = FindTexture(state->PackfileVFS, blendTextureName, terrain.BlendPeg, terrain.BlendTextureBytes, terrain.BlendTextureWidth, terrain.BlendTextureHeight);

    //    //Get ovl texture. Used to light low lod terrain
    //    string texture1Name = Path::GetFileNameNoExtension(terrainMesh.Filename()) + "_ovl.cvbm_pc";
    //    terrain.HasTexture1 = FindTexture(state->PackfileVFS, texture1Name, terrain.Texture1, terrain.Texture1Bytes, terrain.Texture1Width, terrain.Texture1Height);
    //}

    EarlyStopCheck();
    {
        PROFILER_SCOPED("Load high lod terrain");
        //Search for high lod terrain mesh files. Should be 9 of them per zone.
        std::vector<FileHandle> highLodSearchResult = state->PackfileVFS->GetFiles(terrainName + "_*", true);
        for (auto& file : highLodSearchResult)
        {
            if (Path::GetExtension(file.Filename()) != ".ctmesh_pc")
                continue;

            //Valid high lod terrain file found. Load and parse it
            auto* container = file.GetContainer();
            if (!container)
                THROW_EXCEPTION("Failed to get container pointer for a high lod terrain mesh.");

            //Get mesh file data
            auto cpuFileBytes = container->ExtractSingleFile(file.Filename(), true);
            auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(file.Filename()) + ".gtmesh_pc", true);

            //Ensure the mesh files were extracted
            if (!cpuFileBytes)
                THROW_EXCEPTION("Failed to extract high lod terrain mesh cpu file.");
            if (!gpuFileBytes)
                THROW_EXCEPTION("Failed to extract high lod terrain mesh gpu file.");

            BinaryReader cpuFile(cpuFileBytes.value());
            BinaryReader gpuFile(gpuFileBytes.value());

            //Parse high lod terrain mesh
            TerrainSubzone& subzone = terrain.Subzones.emplace_back();
            subzone.Data.Read(cpuFile, file.Filename());

            //Read terrain mesh data
            std::optional<MeshInstanceData> meshData = subzone.Data.ReadTerrainMeshData(gpuFile);
            if (!meshData)
            {
                Log->error("Failed to read mesh data for {}. Halting loading of subzone.", file.Filename());
                delete container;
                delete[] cpuFileBytes.value().data();
                delete[] gpuFileBytes.value().data();
                continue;
            }
            subzone.InstanceData = meshData.value();

            //Todo: Load road and stitch meshes

            delete container;
            delete[] cpuFileBytes.value().data();
            delete[] gpuFileBytes.value().data();

            //Get comb texture. Used to color low lod terrain
            string blendTextureName = terrainName + "comb.cvbm_pc";
            terrain.HasBlendTexture = FindTexture(state->PackfileVFS, blendTextureName, terrain.BlendPeg, terrain.BlendTextureBytes, terrain.BlendTextureWidth, terrain.BlendTextureHeight);

            //Get ovl texture. Used to light low lod terrain
            string texture1Name = terrainName + "_ovl.cvbm_pc";
            terrain.HasTexture1 = FindTexture(state->PackfileVFS, texture1Name, terrain.Texture1, terrain.Texture1Bytes, terrain.Texture1Width, terrain.Texture1Height);
        }
    }

    //Done loading, store terrain instance and mark for renderer.
    terrain.NeedsRenderInit = true;
    terrain.Success = true;

    EarlyStopCheck();
}

bool Territory::FindTexture(PackfileVFS* vfs, const string& textureName, PegFile10& peg, std::span<u8>& textureBytes, u32& textureWidth, u32& textureHeight)
{
    PROFILER_FUNCTION();

    //Search for texture
    std::vector<FileHandle> search = vfs->GetFiles(textureName, true, true);
    if (search.size() == 0)
    {
        Log->warn("Couldn't find {} for {}.", textureName, territoryFilename_);
        return false;
    }

    FileHandle& handle = search[0];
    auto* container = handle.GetContainer();
    if (!container)
        THROW_EXCEPTION("Failed to get container pointer for a terrain mesh.");

    //Get mesh file byte arrays
    auto cpuFileBytes = container->ExtractSingleFile(textureName, true);
    auto gpuFileBytes = container->ExtractSingleFile(RfgUtil::CpuFilenameToGpuFilename(textureName), true);

    //Ensure the texture files were extracted
    if (!cpuFileBytes)
        THROW_EXCEPTION("Failed to extract texture cpu file {}", textureName);
    if (!gpuFileBytes)
        THROW_EXCEPTION("Failed to extract texture gpu file {}", textureName);

    BinaryReader cpuFile(cpuFileBytes.value());
    BinaryReader gpuFile(gpuFileBytes.value());

    peg.Read(cpuFile);
    peg.ReadTextureData(gpuFile, peg.Entries[0]);
    auto maybeTexturePixelData = peg.GetTextureData(0);
    if (maybeTexturePixelData)
    {
        textureBytes = maybeTexturePixelData.value();
        textureWidth = peg.Entries[0].Width;
        textureHeight = peg.Entries[0].Height;
    }
    else
    {
        Log->warn("Failed to extract pixel data for terrain blend texture {}", textureName);
    }

    delete container;
    return true;
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
    [](const ZoneObjectClass& a, const ZoneObjectClass& b)
    {
        return a.NumInstances > b.NumInstances;
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
