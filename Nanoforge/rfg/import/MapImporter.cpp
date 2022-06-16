#include "Importers.h"
#include "rfg/PackfileVFS.h"
#include "rfg/TextureIndex.h"
#include "util/Profiler.h"
#include "common/string/String.h"
#include "common/filesystem/Path.h"
#include "rfg/import/Importers.h"
#include "util/TaskScheduler.h"
#include "Log.h"
#include <RfgTools++/formats/packfiles/Packfile3.h>

//Used to stop the import process early by the calling code setting stopSignal to true. Does nothing if stopSignal is nullptr
#define EarlyStopCheck() if(stopSignal && *stopSignal) \
{ \
    return NullObjectHandle; \
} \
//Same as above but for worker threads which return void
#define WorkerEarlyStopCheck() if(stopSignal && *stopSignal) \
{ \
    return; \
} \

void LoadZone(std::string_view zoneFilename, ObjectHandle territory, Handle<Packfile3> packfile, bool* stopSignal);
void LoadTerrain(ObjectHandle zone, ObjectHandle territory, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal);
void LoadChunks(ObjectHandle zone, ObjectHandle territory, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal);

ObjectHandle Importers::ImportTerritory(std::string_view territoryFilename, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal)
{
    Registry& registry = Registry::Get();
    ObjectHandle territory = registry.CreateObject(territoryFilename, "Territory");
    territory.Set<ObjectHandle>("TextureCache", registry.CreateObject("TextureCache", "TextureCache")); //Stores list of textures used by the territory and its assets. Used to reduce repeat loads

    Handle<Packfile3> packfile = packfileVFS->GetPackfile(territoryFilename);
    if (!packfile)
    {
        LOG_ERROR("Could not find territory file {} in data folder. Required to load the territory.", territoryFilename);
        return NullObjectHandle;
    }
    EarlyStopCheck();

    //Load zones
    {
        //Start worker thread for each zone
        std::vector<Handle<Task>> zoneLoadTasks = {};
        for (u32 i = 0; i < packfile->Entries.size(); i++)
        {
            std::string_view filename = packfile->EntryNames[i];
            string extension = Path::GetExtension(filename);
            if (extension != ".rfgzone_pc" || String::StartsWith(Path::GetFileName(filename), "p_"))
                continue;

            Handle<Task> zoneLoadTask = Task::Create(fmt::format("Importing {}...", filename));
            TaskScheduler::QueueTask(zoneLoadTask, std::bind(LoadZone, filename, territory, packfile, stopSignal));
            zoneLoadTasks.push_back(zoneLoadTask);
        }
        EarlyStopCheck();

        //Wait for zones to finish loading
        {
            PROFILER_SCOPED("Wait for zone load worker threads");
            while (!std::ranges::all_of(zoneLoadTasks, [](Handle<Task> task) { return task->Completed(); }))
            {
                for (Handle<Task> loadTask : zoneLoadTasks)
                    loadTask->Wait();
            }
        }
        EarlyStopCheck();
        Log->info("Done loading zones for {}. Sorting...", territoryFilename);

        //Sort zones by object count for convenience
        std::vector<ObjectHandle>& zones = territory.Property("Zones").GetObjectList();
        std::sort(zones.begin(), zones.end(),
            [](ObjectHandle zoneA, ObjectHandle zoneB)
            {
                return zoneA.Property("NumObjects").Get<u32>() > zoneB.Property("NumObjects").Get<u32>();
            });
    }

    //Load terrain & chunks
    {
        PROFILER_SCOPED("Load terrain & chunks");

        std::vector<Handle<Task>> terrainAndChunkLoadTasks = {};
        Log->info("Starting loading terrain & chunks for {}...", territoryFilename);
        for (ObjectHandle zone : territory.GetObjectList("Zones"))
        {
            Handle<Task> terrainLoadTask = Task::Create(fmt::format("Importing {} terrain...", zone.Property("Name").Get<string>()));
            TaskScheduler::QueueTask(terrainLoadTask, std::bind(LoadTerrain, zone, territory, packfileVFS, textureIndex, stopSignal));
            terrainAndChunkLoadTasks.push_back(terrainLoadTask);
        }
        for (ObjectHandle zone : territory.GetObjectList("Zones"))
        {
            Handle<Task> terrainLoadTask = Task::Create(fmt::format("Importing {} chunks...", zone.Property("Name").Get<string>()));
            TaskScheduler::QueueTask(terrainLoadTask, std::bind(LoadChunks, zone, territory, packfileVFS, textureIndex, stopSignal));
            terrainAndChunkLoadTasks.push_back(terrainLoadTask);
        }

        //Wait for terrain to finish loading
        {
            PROFILER_SCOPED("Wait for terrain load worker threads");
            while (!std::ranges::all_of(terrainAndChunkLoadTasks, [](Handle<Task> task) { return task->Completed(); }))
            {
                for (Handle<Task> loadTask : terrainAndChunkLoadTasks)
                    loadTask->Wait();
            }
        }
    }

    return territory;
}

void LoadZone(std::string_view zoneFilename, ObjectHandle territory, Handle<Packfile3> packfile, bool* stopSignal)
{
    ObjectHandle zone = NullObjectHandle;
    std::optional<std::vector<u8>> zoneBytes = packfile->ExtractSingleFile(zoneFilename);
    std::optional<std::vector<u8>> persistentZoneBytes = packfile->ExtractSingleFile(fmt::format("p_{}", zoneFilename));
    if (zoneBytes.has_value() && persistentZoneBytes.has_value())
    {
        PROFILER_SCOPED("Import zone");
        string territoryFilename = territory.Property("Name").Get<string>();
        zone = Importers::ImportZoneFile(zoneBytes.value(), persistentZoneBytes.value(), zoneFilename, territoryFilename);
        if (zone)
        {
            territory.AppendObjectList("Zones", zone);
        }
        else
        {
            LOG_ERROR("Failed to load zone {} in {}", zoneFilename, territoryFilename);
        }
    }
    else
    {
        LOG_ERROR("Failed to extract zone files. State: [{}, {}]", zoneBytes.has_value(), persistentZoneBytes.has_value());
    }
}

void LoadTerrain(ObjectHandle zone, ObjectHandle territory, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal)
{
    WorkerEarlyStopCheck();

    //Get terrain filename from obj_zone object
    ObjectHandle objZone = GetZoneObject(zone, "obj_zone");
    PropertyHandle terrainFilenameProperty = objZone ? GetZoneProperty(objZone, "terrain_file_name") : NullPropertyHandle;
    if (!objZone || !terrainFilenameProperty)
        return; //No terrain

    string terrainName = terrainFilenameProperty.Get<string>();
    if (terrainName.ends_with('\0'))
        terrainName.pop_back(); //Remove extra null terminators that some RFG string properties have

    Vec3 objZoneBmin = objZone.Property("Bmin").Get<Vec3>();
    Vec3 objZoneBmax = objZone.Property("Bmax").Get<Vec3>();
    Vec3 objZoneBbCenter = objZoneBmin + (objZoneBmax - objZoneBmin); //Terrain base position

    //Holds terrain info for a single zone (low + high lod terrain stored in sub-objects)
    ObjectHandle zoneTerrain = Importers::ImportTerrain(territory, terrainName, objZoneBbCenter, packfileVFS, textureIndex, stopSignal);
    if (zoneTerrain)
    {
        zone.Set<ObjectHandle>("Terrain", zoneTerrain);
    }
    else
        LOG_ERROR("Failed to import terrain for {}", zone.Property("Name").Get<string>());
}

void LoadChunks(ObjectHandle zone, ObjectHandle territory, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal)
{
    PROFILER_FUNCTION();

    for (ObjectHandle obj : zone.GetObjectList("Objects"))
    {
        const string objType = obj.Get<string>("Type");
        if (objType == "rfg_mover" || objType == "general_mover")
        {
            if (!obj.Has("chunk_name") && !obj.IsType<string>("chunk_name"))
            {
                LOG_ERROR("Failed to load chunk mesh for {}, chunk_name property missing.", obj.Get<string>("Name"));
                continue;
            }

            //Get chunk name and replace .rfgchunk_pc extension with .cchk_pc (the real extension)
            string chunkFilename = obj.Get<string>("chunk_name");
            chunkFilename = String::Replace(chunkFilename, ".rfgchunk_pc", ".cchk_pc");

            //See if chunk was already loaded to prevent repeat loads
            ObjectHandle chunkMesh = NullObjectHandle;
            std::vector<ObjectHandle> chunkCache = territory.GetObjectList("Chunks");
            for (ObjectHandle chunk : chunkCache)
            {
                if (chunk.Get<string>("Name") == chunkFilename)
                {
                    chunkMesh = chunk;
                    break;
                }
            }
             
            if (!chunkMesh) //First import
            {
                chunkMesh = Importers::ImportChunk(chunkFilename, packfileVFS, textureIndex, stopSignal, territory.Get<ObjectHandle>("TextureCache"));
                if (chunkMesh)
                {
                    territory.AppendObjectList("Chunks", chunkMesh);
                }
                else
                {
                    LOG_ERROR("Failed to import chunk mesh for {}", obj.UID());
                    continue;
                }
            }

            obj.Set<ObjectHandle>("ChunkMesh", chunkMesh);
        }
    }
}