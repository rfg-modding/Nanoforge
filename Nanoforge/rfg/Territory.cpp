#include "Territory.h"
#include "common/Defer.h"
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
#include <synchapi.h> //For Sleep()
#include <ranges>
#include <RfgTools++/formats/packfiles/Packfile3.h>
#include "rfg/PackfileVFS.h"
#include "application/project/Project.h"
#include "render/resources/Scene.h"
#include "application/Registry.h"
#include "rfg/import/Importers.h"
#include "gui/util/WinUtil.h"
#include <RfgTools++\formats\zones\ZoneFile.h>

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

//Used by the primary loading thread to stop early if loadThreadShouldStop_ == true
#define EarlyStopCheck() if(loadThreadShouldStop_) \
{ \
    state->ClearStatus(); \
    loadThreadRunning_ = false; \
    return; \
} \

//Same as above but for subthreads. Different macro is used since only the primary thread should change loadThreadRunning_
#define SubThreadEarlyStopCheck() if(loadThreadShouldStop_) \
{ \
    return; \
} \

ObjectHandle FindTerritory(std::string_view territoryFilename)
{
    return Registry::Get().FindObject(territoryFilename, "Territory");
}

void Territory::LoadThread(Handle<Task> task, Handle<Scene> scene, GuiState* state)
{
    PROFILER_FUNCTION();
    while (!packfileVFS_ || !packfileVFS_->Ready()) //Wait for packfile thread to finish.
    {
        Sleep(50);
    }
    EarlyStopCheck();
    LoadThreadTimer.Reset();

    //Ensure territory is imported
    Object = FindTerritory(territoryFilename_);
    if (!Object)
    {
        //Not found. Import territory
        Timer timer(true);
        state->SetStatus(ICON_FA_SYNC " Importing " + territoryShortname_ + ". This might take a while. Subsequent loads will be much quicker...", Working);
        Object = Importers::ImportTerritory(territoryFilename_, state->PackfileVFS, state->TextureSearchIndex, &loadThreadShouldStop_);
        if (Object)
        {
            if (state->CurrentProject)
                state->CurrentProject->Save();
        }
        else
        {
            if (!state->MainWindowFocused)
                ToastNotification(fmt::format("Failed to import {}. Check logs.", territoryShortname_), "Map import error");

            LOG_ERROR("Failed to load territory. Check logs.");
            return;
        }
        if (!state->MainWindowFocused)
            ToastNotification(fmt::format("Finished importing {}", territoryShortname_), "Map import complete");

        for (ObjectHandle zone : Object.GetObjectList("Zones"))
            SetZoneShortName(zone); //Set shorthand names for each zone (used in UI to save space). E.g. terr01_04_07.rfgzone_pc -> 04_07 (xz coords)

        Log->info("Imported {} in {}s", territoryShortname_, timer.ElapsedSecondsPrecise());
    }
    EarlyStopCheck();

    state->SetStatus(ICON_FA_SYNC " Loading " + territoryShortname_ + "...", Working);

    //Create terrain instance for each zone
    for (ObjectHandle zone : Object.GetObjectList("Zones"))
        TerrainInstances.push_back({});

    //Already imported. Queue tasks to load each zone
    std::vector<Handle<Task>> zoneLoadTasks;
    size_t i = 0;
    for (ObjectHandle zone : Object.GetObjectList("Zones"))
    {
        Handle<Task> zoneLoadTask = Task::Create(fmt::format("Loading {}...", zone.Get<string>("Name")));
        TaskScheduler::QueueTask(zoneLoadTask, std::bind(&Territory::LoadZoneWorkerThread, this, zoneLoadTask, scene, zone, i, state));
        zoneLoadTasks.push_back(zoneLoadTask);
        i++;
    }

    //Wait for all workers to finish
    {
        PROFILER_SCOPED("Wait for territory load worker threads");
        while (!std::ranges::all_of(zoneLoadTasks, [](Handle<Task> task) { return task->Completed(); }))
        {
            for (Handle<Task> loadTask : zoneLoadTasks)
                loadTask->Wait();
        }
    }

    //Get zone name with most characters for UI purposes
    u32 longest = 0;
    for (ObjectHandle zone : Object.GetObjectList("Zones"))
        if (size_t len = zone.Property("ShortName").Get<string>().length(); len > longest)
            longest = (u32)len;
    LongestZoneName = longest;

    for (ObjectHandle zone : Object.GetObjectList("Zones"))
        Zones.push_back(zone);

    //Init object class data used for filtering and labelling
    EarlyStopCheck();
    InitObjectClassData();
    UpdateObjectClassInstanceCounts();

    Log->info("Done loading {}. Took {} seconds. {} objects in registry", territoryFilename_, LoadThreadTimer.ElapsedSecondsPrecise(), Registry::Get().NumObjects());
    state->ClearStatus();
    loadThreadRunning_ = false;
    ready_ = true;
}

void Territory::LoadZoneWorkerThread(Handle<Task> task, Handle<Scene> scene, ObjectHandle zone, size_t terrainIndex, GuiState* state)
{
    SubThreadEarlyStopCheck();
    PROFILER_FUNCTION();
    Registry& registry = Registry::Get();
    if (!zone.Has("Terrain") || !zone.IsType<ObjectHandle>("Terrain"))
        return;

    ObjectHandle terrain = zone.Get<ObjectHandle>("Terrain");
    TerrainInstance& terrainInstance = TerrainInstances[terrainIndex];
    terrainInstance.Name = terrain.Get<string>("Name");
    terrainInstance.Position = terrain.Get<Vec3>("Position");

    //Load low lod terrain
    {
        PROFILER_SCOPED("Load low lod terrain");
        ObjectHandle lowLodTerrain = terrain.Get<ObjectHandle>("LowLod");
        std::optional<Texture2D> texture0 = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, lowLodTerrain.Get<ObjectHandle>("CombTexture"));
        std::optional<Texture2D> texture1 = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, lowLodTerrain.Get<ObjectHandle>("OvlTexture"));
        if (!texture0 || !texture1)
        {
            LOG_ERROR("Failed to load textures for low lod terrain of zone '{}'", zone.Get<string>("Name"));
            return;
        }

        for (ObjectHandle mesh : lowLodTerrain.GetObjectList("Meshes"))
        {
            SubThreadEarlyStopCheck();
            //TODO: Change renderer so it can accept registry meshes directly
            std::optional<MeshInstanceData> meshData = LoadRegistryMesh(mesh);
            if (!meshData)
                continue;

            Handle<RenderObject> renderObject = scene->CreateRenderObject("TerrainLowLod", Mesh{ scene->d3d11Device_, meshData.value() }, terrain.Get<Vec3>("Position"));
            renderObject->Visible = !useHighLodTerrain_;
            renderObject->UseTextures = texture0.has_value() || texture1.has_value();
            renderObject->Textures[0] = texture0.value();
            renderObject->Textures[1] = texture1.value();
            TerrainLock.lock();
            terrainInstance.LowLodMeshes.push_back(renderObject);
            TerrainLock.unlock();
        }

        terrainInstance.DataLowLod = lowLodTerrain;
    }
    SubThreadEarlyStopCheck();

    //Load high lod terrain
    for (size_t subzoneIndex = 0; subzoneIndex < terrain.GetObjectList("HighLod").size(); subzoneIndex++)
    {
        SubThreadEarlyStopCheck();
        ObjectHandle highLodTerrain = terrain.GetObjectList("HighLod")[subzoneIndex];
        terrainInstance.Subzones.push_back(highLodTerrain);

        bool anyTextureValid = false;
        std::array<std::optional<Texture2D>, 10> textures;
        for (size_t i = 0; i < 10; i++)
        {
            ObjectHandle textureObject = highLodTerrain.Get<ObjectHandle>(fmt::format("Texture{}", i));
            std::optional<Texture2D> texture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, textureObject);
            textures[i] = texture;
            if (texture)
                anyTextureValid = true;
        }

        //Load terrain mesh
        {
            ObjectHandle mesh = highLodTerrain.Get<ObjectHandle>("Mesh");
            std::optional<MeshInstanceData> meshData = LoadRegistryMesh(mesh);
            if (!meshData)
                continue;

            Handle<RenderObject> renderObject = scene->CreateRenderObject("Terrain", Mesh{ scene->d3d11Device_, meshData.value() }, highLodTerrain.Get<Vec3>("Position"));
            renderObject->Visible = useHighLodTerrain_;
            renderObject->UseTextures = anyTextureValid;
            for (size_t i = 0; i < 10; i++)
                renderObject->Textures[i] = textures[i];

            TerrainLock.lock();
            terrainInstance.HighLodMeshes.push_back(renderObject);
            TerrainLock.unlock();
        }

        //Load stitch meshes if this instance has any
        if (highLodTerrain.Has("StitchMesh") && highLodTerrain.GetObjectList("StitchInstances").size() > 0)
        {
            ObjectHandle stitchMesh = highLodTerrain.Get<ObjectHandle>("StitchMesh");
            std::optional<MeshInstanceData> stitchMeshData = LoadRegistryMesh(stitchMesh);
            if (!stitchMeshData)
                continue;

            //Todo: Load other stitch meshes (a bunch of rocks). Currently only loads road stitch meshes since the rocks are stored in separate files.
            Vec3 stitchPos = highLodTerrain.GetObjectList("StitchInstances").back().Get<Vec3>("Position");
            Handle<RenderObject> stitchObject = scene->CreateRenderObject("TerrainStitch", Mesh{ scene->d3d11Device_, stitchMeshData.value() }, stitchPos);
            stitchObject->Visible = useHighLodTerrain_;
            stitchObject->UseTextures = anyTextureValid;
            for (size_t i = 0; i < 10; i++)
                stitchObject->Textures[i] = textures[i];

            TerrainLock.lock();
            terrainInstance.StitchMeshes.push_back(StitchMesh{ .Mesh = stitchObject, .SubzoneIndex = (u32)subzoneIndex });
            TerrainLock.unlock();
        }
        else if (highLodTerrain.Has("StitchMesh"))
        {
            Log->warn("Couldn't load stitch meshes for {}. Necessary data not found.", highLodTerrain.Get<string>("Name"));
            continue;
        }

        //Load road meshes
        for (ObjectHandle roadMesh : highLodTerrain.GetObjectList("RoadMeshes"))
        {
            std::optional<MeshInstanceData> meshData = LoadRegistryMesh(roadMesh);
            if (!meshData)
                continue;

            bool anyRoadTextureValid = false;
            std::array<std::optional<Texture2D>, 10> roadTextures;
            for (size_t i = 0; i < 10; i++)
            {
                ObjectHandle textureObject = roadMesh.Get<ObjectHandle>(fmt::format("Texture{}", i));
                std::optional<Texture2D> texture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, textureObject);
                roadTextures[i] = texture;
                if (texture)
                    anyRoadTextureValid = true;
            }

            Handle<RenderObject> renderObject = scene->CreateRenderObject("TerrainRoad", Mesh{ scene->d3d11Device_, meshData.value() }, roadMesh.Get<Vec3>("Position"));
            renderObject->Visible = useHighLodTerrain_;
            renderObject->UseTextures = anyRoadTextureValid;
            for (size_t i = 0; i < 10; i++)
                renderObject->Textures[i] = roadTextures[i];

            TerrainLock.lock();
            terrainInstance.RoadMeshes.push_back(RoadMesh{ .Mesh = renderObject, .SubzoneIndex = (u32)subzoneIndex });
            TerrainLock.unlock();
        }

        //Load rock meshes
        for (ObjectHandle rock : highLodTerrain.GetObjectList("Rocks"))
        {
            std::optional<MeshInstanceData> meshData = LoadRegistryMesh(rock.Get<ObjectHandle>("Mesh"));
            if (!meshData)
                continue;

            //Load diffuse texture
            std::optional<Texture2D> diffuseTexture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, rock.Get<ObjectHandle>("DiffuseTexture"));
            std::optional<Texture2D> normalTexture = LoadTexture(scene->d3d11Device_, state->TextureSearchIndex, rock.Get<ObjectHandle>("NormalTexture"));
            bool anyRockTextureValid = diffuseTexture.has_value() || normalTexture.has_value();

            //Create render object
            Handle<RenderObject> renderObject = scene->CreateRenderObject("Rock", Mesh{ scene->d3d11Device_, meshData.value() }, rock.Get<Vec3>("Position"), rock.Get<Mat3>("Rotation"));
            renderObject->Visible = useHighLodTerrain_;
            renderObject->UseTextures = anyRockTextureValid;
            if (diffuseTexture)
                renderObject->Textures[0] = diffuseTexture;
            if (normalTexture)
                renderObject->Textures[1] = normalTexture;

            TerrainLock.lock();
            terrainInstance.RockMeshes.push_back(RockMesh{ .Mesh = renderObject, .SubzoneIndex = (u32)subzoneIndex });
            TerrainLock.unlock();
        }
    }

    terrainInstance.Loaded = true;
}

std::optional<MeshInstanceData> Territory::LoadRegistryMesh(ObjectHandle mesh)
{
    MeshInstanceData data;
    BufferHandle indexBuffer = mesh.Get<BufferHandle>("Indices");
    BufferHandle vertexBuffer = mesh.Get<BufferHandle>("Vertices");
    if (!indexBuffer || !vertexBuffer)
    {
        LOG_ERROR("Failed to load registry mesh '{}'. Null buffer handle. UIDs: {}, {}", mesh.Get<string>("Name"), indexBuffer.UID(), vertexBuffer.UID());
        return {};
    }

    std::optional<std::vector<u8>> indices = indexBuffer.Load();
    std::optional<std::vector<u8>> vertices = vertexBuffer.Load();
    if (!indices || !vertices)
    {
        //TODO: Add option to reload the mesh from packfiles. Would allow people to delete buffers or exclude them from git repos (saves of 644MB on main map)
        LOG_ERROR("Failed to load registry mesh '{}'. Failed to locate meshes. UIDs: {}, {}", mesh.Get<string>("Name"), indexBuffer.UID(), vertexBuffer.UID());
        return {};
    }
    data.IndexBuffer = indices.value();
    data.VertexBuffer = vertices.value();

    MeshDataBlock& header = data.Info;
    header.Version = mesh.Get<u32>("Version");
    header.VerificationHash = mesh.Get<u32>("VerificationHash");
    header.CpuDataSize = mesh.Get<u32>("CpuDataSize");
    header.GpuDataSize = mesh.Get<u32>("GpuDataSize");
    header.NumSubmeshes = mesh.Get<u32>("NumSubmeshes");
    header.SubmeshesOffset = mesh.Get<u32>("SubmeshesOffset");
    header.NumVertices = mesh.Get<u32>("NumVertices");
    header.VertexStride0 = mesh.Get<u8>("VertexStride0");
    header.VertFormat = (VertexFormat)mesh.Get<u8>("VertexFormat");
    header.NumUvChannels = mesh.Get<u8>("NumUvChannels");
    header.VertexStride1 = mesh.Get<u8>("VertexStride1");
    header.VertexOffset = mesh.Get<u32>("VertexOffset");
    header.NumIndices = mesh.Get<u32>("NumIndices");
    header.IndicesOffset = mesh.Get<u32>("IndicesOffset");
    header.IndexSize = mesh.Get<u8>("IndexSize");
    header.PrimitiveType = (PrimitiveTopology)mesh.Get<u8>("PrimitiveType");
    header.NumRenderBlocks = mesh.Get<u16>("NumRenderBlocks");

    for (ObjectHandle submeshObj : mesh.GetObjectList("Submeshes"))
    {
        SubmeshData& submesh = header.Submeshes.emplace_back();
        submesh.NumRenderBlocks = submeshObj.Get<u32>("NumRenderBlocks");
        submesh.Offset = submeshObj.Get<Vec3>("Offset");
        submesh.Bmin = submeshObj.Get<Vec3>("Bmin");
        submesh.Bmax = submeshObj.Get<Vec3>("Bmax");
        submesh.RenderBlocksOffset = submeshObj.Get<u32>("RenderBlocksOffset");
    }

    for (ObjectHandle blockObj : mesh.GetObjectList("RenderBlocks"))
    {
        RenderBlock& renderBlock = header.RenderBlocks.emplace_back();
        renderBlock.MaterialMapIndex = blockObj.Get<u16>("MaterialMapIndex");
        renderBlock.StartIndex = blockObj.Get<u32>("StartIndex");
        renderBlock.NumIndices = blockObj.Get<u32>("NumIndices");
        renderBlock.MinIndex = blockObj.Get<u32>("MinIndex");
        renderBlock.MaxIndex = blockObj.Get<u32>("MaxIndex");
    }

    return data;
}

std::optional<Texture2D> Territory::LoadTexture(ComPtr<ID3D11Device> d3d11Device, TextureIndex* textureSearchIndex, ObjectHandle texture)
{
    PROFILER_FUNCTION();
    if (!texture)
        return {};

    std::lock_guard<std::mutex> lock(textureCacheLock_);

    //Check cache first
    auto cacheSearch = textureCache_.find(texture.Get<string>("Name"));
    if (cacheSearch != textureCache_.end())
        return cacheSearch->second;

    //Not cached yet, load it
    {
        PROFILER_SCOPED(fmt::format("Loading {}", texture.Get<string>("Name")).c_str());
        BufferHandle pixelBuffer = texture.Get<BufferHandle>("Pixels");
        if (!pixelBuffer)
        {
            LOG_ERROR("Failed to load registry texture '{}'. Null buffer handle. UID: {}", texture.Get<string>("Name"), pixelBuffer.UID());
            return {};
        }
        std::optional<std::vector<u8>> pixels = pixelBuffer.Load();
        if (!pixels)
        {
            LOG_ERROR("Failed to load registry texture '{}'. Empty. UID: {}", texture.Get<string>("Name"), pixelBuffer.UID());
            return {};
        }

        //One subresource data per mip level
        DXGI_FORMAT format = PegHelpers::PegFormatToDxgiFormat((PegFormat)texture.Get<u16>("Format"), texture.Get<u16>("Flags"));
        std::vector<D3D11_SUBRESOURCE_DATA> textureData = PegHelpers::CalcSubresourceData(texture, pixels.value());
        D3D11_SUBRESOURCE_DATA* subresourceData = textureData.size() > 0 ? textureData.data() : nullptr;

        //Create renderer texture
        Texture2D texture2d;
        texture2d.Name = texture.Get<string>("Name");
        texture2d.Create(d3d11Device, texture.Get<u16>("Width"), texture.Get<u16>("Height"), format, D3D11_BIND_SHADER_RESOURCE, subresourceData, texture.Get<u8>("MipLevels"));
        texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
        texture2d.CreateSampler(); //Need sampler too

        //Cache for future loads
        textureCache_[texture.Get<string>("Name")] = texture2d;
        return texture2d;
    }
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
    if (!Object)
        return;

    //Zero instance counts for each object class
    for (auto& objectClass : ZoneObjectClasses)
        objectClass.NumInstances = 0;

    //Update instance count for each object class in visible zones
    for (ObjectHandle zone : Object.GetObjectList("Zones"))
    {
        for (ObjectHandle object : zone.Property("Objects").GetObjectList())
        {
            for (auto& objectClass : ZoneObjectClasses)
            {
                if (objectClass.Hash == object.Property("ClassnameHash").Get<u32>())
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
        { "rfg_mover",                      2898847573, 0, Vec3{ 0.819f, 0.819f, 0.819f }, true , false, ICON_FA_HOME " ",            true  },
        { "cover_node",                     3322951465, 0, Vec3{ 1.0f, 0.0f, 0.0f       }, false, false, ICON_FA_SHIELD_ALT " ",      false },
        { "navpoint",                       4055578105, 0, Vec3{ 1.0f, 0.968f, 0.0f     }, false, false, ICON_FA_LOCATION_ARROW " ",  false },
        { "general_mover",                  1435016567, 0, Vec3{ 1.0f, 0.664f, 0.0f     }, true , false, ICON_FA_CUBES  " ",          false },
        { "player_start",                   1794022917, 0, Vec3{ 0.591f, 1.0f, 0.0f     }, true , false, ICON_FA_STREET_VIEW " ",     false },
        { "multi_object_marker",            1332551546, 0, Vec3{ 0.000f, 0.759f, 1.0f   }, true , false, ICON_FA_MAP_MARKER " ",      false },
        { "weapon",                         2760055731, 0, Vec3{ 1.0f, 0.0f, 0.0f       }, true , false, ICON_FA_CROSSHAIRS " ",      false },
        { "object_action_node",             2017715543, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_RUNNING " ",         false },
        { "object_squad_spawn_node",        311451949,  0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_USERS " ",           false },
        { "object_npc_spawn_node",          2305434277, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_USER " ",            false },
        { "object_guard_node",              968050919,  0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_SHIELD_ALT " ",      false },
        { "object_path_road",               3007680500, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_ROAD " ",            false },
        { "shape_cutter",                   753322256,  0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_CUT " ",             false },
        { "item",                           27482413,   0, Vec3{ 0.719f, 0.494f, 0.982f }, true , false, ICON_FA_TOOLS " ",           false },
        { "object_vehicle_spawn_node",      3057427650, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_CAR_SIDE " ",        false },
        { "ladder",                         1620465961, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_LEVEL_UP_ALT " ",    false },
        { "constraint",                     1798059225, 0, Vec3{ 0.975f, 0.407f, 1.0f   }, true , false, ICON_FA_LOCK " ",            false },
        { "object_effect",                  2663183315, 0, Vec3{ 1.0f, 0.45f, 0.0f      }, true , false, ICON_FA_FIRE " ",            false },
        { "trigger_region",                 2367895008, 0, Vec3{ 0.63f, 0.0f, 1.0f      }, true , false, ICON_FA_BORDER_NONE " ",     false },
        { "object_bftp_node",               3005715123, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, false, false, ICON_FA_BOMB " ",            false },
        { "object_bounding_box",            2575178582, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_CUBE " ",            false },
        { "object_turret_spawn_node",       96035668,   0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_CROSSHAIRS " ",      false },
        { "obj_zone",                       3740226015, 0, Vec3{ 0.935f, 0.0f, 1.0f     }, true , false, ICON_FA_SEARCH_LOCATION " ", false },
        { "object_patrol",                  3656745166, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_BINOCULARS " ",      false },
        { "object_dummy",                   2671133140, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_MEH_BLANK " ",       false },
        { "object_raid_node",               3006762854, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_CAR_CRASH " ",       false },
        { "object_delivery_node",           1315235117, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_SHIPPING_FAST " ",   false },
        { "marauder_ambush_region",         1783727054, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_USER_NINJA " ",      false },
        { "object_activity_spawn",          2219327965, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_SCROLL " ",          false },
        { "object_mission_start_node",      1536827764, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_MAP_MARKED " ",      false },
        { "object_demolitions_master_node", 3497250449, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_BOMB " ",            false },
        { "object_restricted_area",         3157693713, 0, Vec3{ 1.0f, 0.0f, 0.0f       }, true , true,  ICON_FA_USER_SLASH " ",      false },
        { "effect_streaming_node",          1742767984, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, true,  ICON_FA_SPINNER " ",         false },
        { "object_house_arrest_node",       227226529,  0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_USER_LOCK " ",       false },
        { "object_area_defense_node",       2107155776, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_USER_SHIELD " ",     false },
        { "object_safehouse",               3291687510, 0, Vec3{ 0.0f, 0.905f, 1.0f     }, true , false, ICON_FA_FIST_RAISED " ",     false },
        { "object_convoy_end_point",        1466427822, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_TRUCK_MOVING " ",    false },
        { "object_courier_end_point",       3654824104, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_FLAG_CHECKERED " ",  false },
        { "object_riding_shotgun_node",     1227520137, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_TRUCK_MONSTER " ",   false },
        { "object_upgrade_node",            2502352132, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, false, ICON_FA_ARROW_UP " ",        false },
        { "object_ambient_behavior_region", 2407660945, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_TREE " ",            false },
        { "object_roadblock_node",          2100364527, 0, Vec3{ 0.719f, 0.494f, 0.982f }, false, true,  ICON_FA_HAND_PAPER " ",      false },
        { "object_spawn_region",            1854373986, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , true,  ICON_FA_USER_PLUS " ",       false },
        { "obj_light",                      2915886275, 0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , true,  ICON_FA_LIGHTBULB " ",       false },
        { "unknown",                        0,          0, Vec3{ 1.0f, 1.0f, 1.0f       }, true , false, ICON_FA_QUESTION_CIRCLE " ", false },
    };

    for (ObjectHandle zone : Object.GetObjectList("Zones"))
    {
        for (ObjectHandle object : zone.Property("Objects").GetObjectList())
        {
            u32 outIndex = InvalidZoneIndex;
            if (!ObjectClassRegistered(object.Property("ClassnameHash").Get<u32>(), outIndex))
            {
                ZoneObjectClasses.push_back({ object.Property("Type").Get<string>(), object.Property("ClassnameHash").Get<u32>(), 0, Vec3{ 1.0f, 1.0f, 1.0f }, true });
                Log->warn("Found unknown object class with hash {} and name \"{}\"", object.Property("ClassnameHash").Get<u32>(), object.Property("Type").Get<string>());
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

void Territory::SetZoneShortName(ObjectHandle zone)
{
    const string& fullName = zone.Property("Name").Get<string>();
    zone.Property("ShortName").Set<string>(fullName); //Set shortname to fullname by default in case of failure
    const string expectedPostfix0 = ".rfgzone_pc";
    const string expectedPostfix1 = ".layer_pc";
    const string expectedPrefix = territoryShortname_ + "_";

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
        zone.Property("ShortName").Set<string>(fullName.substr(0, finalPostfixIndex)); //Remove the postfix, keep the rest
    }
    else //Otherwise we strip the prefix and postfix and use the remaining string as our shortname
    {
        zone.Property("ShortName").Set<string>(fullName.substr(expectedPrefix.length(), finalPostfixIndex - expectedPrefix.length())); //Keep string between prefix and postfix
    }

    //Add p_ prefix back onto persistent files
    if (zone.Property("Persistent").Get<bool>())
        zone.Property("ShortName").Set<string>("p_" + zone.Property("ShortName").Get<string>());
}