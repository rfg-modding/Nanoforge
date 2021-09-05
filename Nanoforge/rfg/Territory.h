#pragma once
#include "common/Typedefs.h"
#include "PackfileVFS.h"
#include "common/timing/Timer.h"
#include "rfg/TerrainHelpers.h"
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\types\Vec4.h>
#include <future>
#include <mutex>

//Wrapper around ZonePc36 used by Territory
struct ZoneData
{
    string Name;
    string ShortName;
    ZonePc36 Zone;
    bool RenderBoundingBoxes = false;
    bool Persistent = false;
    bool MissionLayer = false; //If true the zone is from a mission layer file
    bool ActivityLayer = false; //If true the zone is from a activity layer file
};

//Used by Territory to filter objects list by class type
struct ZoneObjectClass
{
    string Name;
    u32 Hash = 0;
    u32 NumInstances = 0;
    //Todo: This is mixing ui logic with data. I'd like to separate this stuff out of Territory if possible
    Vec3 Color = { 1.0f, 1.0f, 1.0f };
    bool Show = true;
    bool ShowLabel = false;
    const char* LabelIcon = "";
};

constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

class GuiState;

//Loads all zone files for a territory and tracks info about them and their contents
class Territory
{
public:
    //Set values needed for it to function
    void Init(PackfileVFS* packfileVFS, const string& territoryFilename, const string& territoryShortname);
    std::future<void> LoadAsync(GuiState* state); //Starts a thread that loads the territory zones, terrain, etc
    void StopLoadThread() { loadThreadShouldStop_ = true; }
    void ClearLoadThreadData(); //Clear temporary data accrued by the load threads
    bool LoadThreadRunning() { return loadThreadRunning_; }
    bool Ready() { return ready_; } //Returns true if the territory is loaded and ready for use

    bool ShouldShowObjectClass(u32 classnameHash); //Returns true if the object should be visible. Based on zone object list filtering options
    bool ObjectClassRegistered(u32 classnameHash, u32& outIndex); //Returns true if the object type with this classname hash is known
    void UpdateObjectClassInstanceCounts(); //Update number of instances of each object class for visible zones
    void InitObjectClassData(); //Initialize object class data. Used for identification and filtering.
    ZoneObjectClass& GetObjectClass(u32 classnameHash);

    std::vector<ZoneData> ZoneFiles;
    std::vector<ZoneObjectClass> ZoneObjectClasses = {};
    std::mutex ZoneFilesLock;
    std::vector<TerrainInstance> TerrainInstances = {};
    std::mutex TerrainLock;

    u32 LongestZoneName = 0;
    Timer LoadThreadTimer;

private:
    void LoadThread(GuiState* state); //Top level loading thread
    void LoadWorkerThread(GuiState* state, Packfile3* packfile, const char* zoneFilename); //Loads a single zone and its assets
    bool FindTexture(PackfileVFS* vfs, const string& textureName, PegFile10& peg, std::span<u8>& textureBytes, u32& textureWidth, u32& textureHeight);
    void SetZoneShortName(ZoneData& zone); //Attempts to shorten zone name. E.g. terr01_07_02.rfgzone_pc -> 07_02

    PackfileVFS* packfileVFS_ = nullptr;
    string territoryFilename_; //Name of the vpp_pc file that zone data is loaded from at startup
    string territoryShortname_;
    bool ready_ = false;
    bool loadThreadShouldStop_ = false;
    bool loadThreadRunning_ = false;
};