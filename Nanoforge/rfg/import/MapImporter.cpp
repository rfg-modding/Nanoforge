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

//Used to lock territory.Zones while pushing. Limitation of registry object lists not yet being thread safe.
std::mutex ZoneFilesLock;

ObjectHandle Importers::ImportTerritory(std::string_view territoryFilename, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal)
{
    Registry& registry = Registry::Get();
    ObjectHandle territory = registry.CreateObject(territoryFilename, "Territory");

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
            const char* filename = packfile->EntryNames[i];
            string extension = Path::GetExtension(filename);
            if (extension != ".rfgzone_pc")
                continue;

            Handle<Task> zoneLoadTask = Task::Create(fmt::format("Importing {}...", filename));
            TaskScheduler::QueueTask(zoneLoadTask, std::bind(LoadZone, std::string_view(filename), territory, packfile, stopSignal));
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

    //Load terrain
    {
        Log->info("Starting loading terrain for {}...", territoryFilename);
        std::vector<Handle<Task>> terrainLoadTasks = {};
        for (ObjectHandle zone : territory.GetObjectList("Zones"))
        {
            Handle<Task> terrainLoadTask = Task::Create(fmt::format("Importing {} terrain...", zone.Property("Name").Get<string>()));
            TaskScheduler::QueueTask(terrainLoadTask, std::bind(LoadTerrain, zone, territory, packfileVFS, textureIndex, stopSignal));
            terrainLoadTasks.push_back(terrainLoadTask);
        }

        //Wait for terrain to finish loading
        {
            PROFILER_SCOPED("Wait for terrain load worker threads");
            while (!std::ranges::all_of(terrainLoadTasks, [](Handle<Task> task) { return task->Completed(); }))
            {
                for (Handle<Task> loadTask : terrainLoadTasks)
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
    if (zoneBytes.has_value())
    {
        PROFILER_SCOPED("Import zone");
        string territoryFilename = territory.Property("Name").Get<string>();
        zone = Importers::ImportZoneFile(zoneBytes.value(), zoneFilename, territoryFilename);
        if (zone)
        {
            ZoneFilesLock.lock();
            territory.Property("Zones").GetObjectList().push_back(zone);
            ZoneFilesLock.unlock();
        }
        else
        {
            LOG_ERROR("Failed to load zone {} in {}", zoneFilename, territoryFilename);
        }
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
    ObjectHandle zoneTerrain = Importers::ImportTerrain(terrainName, objZoneBbCenter, packfileVFS, textureIndex, stopSignal);
    if (zoneTerrain)
    {
        ZoneFilesLock.lock();
        zone.Property("Terrain").Set(zoneTerrain);
        ZoneFilesLock.unlock();
    }
    else
        LOG_ERROR("Failed to import terrain for {}", zone.Property("Name").Get<string>());
}