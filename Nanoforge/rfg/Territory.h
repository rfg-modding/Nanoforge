#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "rfg/TerrainHelpers.h"
#include <RfgTools++\types\Vec3.h>
#include "render/resources/Texture2D.h"
#include "application/Registry.h"
#include <unordered_map>
#include <shared_mutex>

class Task;
class Scene;
class TextureIndex;
class PackfileVFS;
class Packfile3;
class GuiState;
struct ZoneObjectClass;
constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

//Loads all zone files for a territory and tracks info about them and their contents
class Territory
{
public:
    //Set values needed for it to function
    void Init(PackfileVFS* packfileVFS, const string& territoryFilename, const string& territoryShortname, bool useHighLodTerrain);
    bool Ready() { return ready_; } //Returns true if the territory is loaded and ready for use

    Handle<Task> LoadAsync(Handle<Scene> scene, GuiState* state); //Starts a thread that loads the territory zones, terrain, etc
    void StopLoadThread() { loadThreadShouldStop_ = true; }
    bool LoadThreadRunning() { return loadThreadRunning_; }

    bool ShouldShowObjectClass(u32 classnameHash); //Returns true if the object should be visible. Based on zone object list filtering options
    bool ObjectClassRegistered(u32 classnameHash, u32& outIndex); //Returns true if the object type with this classname hash is known
    void UpdateObjectClassInstanceCounts(); //Update number of instances of each object class for visible zones
    void InitObjectClassData(); //Initialize object class data. Used for identification and filtering.
    ZoneObjectClass& GetObjectClass(u32 classnameHash);

    ObjectHandle Object = NullObjectHandle;
    std::vector<ObjectHandle> Zones = {};
    std::vector<ZoneObjectClass> ZoneObjectClasses = {};
    std::vector<TerrainInstance> TerrainInstances = {};
    std::shared_mutex ZoneFilesLock;
    std::shared_mutex TerrainLock;

    u32 LongestZoneName = 0;
    Timer LoadThreadTimer;

private:
    void LoadThread(Handle<Task> task, Handle<Scene> scene, GuiState* state); //Top level loading thread. Imports territory on first load
    void LoadZoneWorkerThread(Handle<Task> task, Handle<Scene> scene, ObjectHandle zone, size_t terrainIndex, GuiState* state); //Loads a single zone and its assets
    std::optional<MeshInstanceData> LoadRegistryMesh(ObjectHandle mesh); //Load mesh stored in registry as a renderer ready format
    //Load texture (xxx.tga) and create a render texture from it. Textures are cached to prevent repeat loads
    std::optional<Texture2D> LoadTexture(ComPtr<ID3D11Device> d3d11Device, TextureIndex* textureSearchIndex, ObjectHandle texture);

    void SetZoneShortName(ObjectHandle zone); //Attempts to shorten zone name. E.g. terr01_07_02.rfgzone_pc -> 07_02

    PackfileVFS* packfileVFS_ = nullptr;
    string territoryFilename_; //Name of the vpp_pc file that zone data is loaded from at startup
    string territoryShortname_;
    bool ready_ = false;
    bool loadThreadShouldStop_ = false;
    bool loadThreadRunning_ = false;
    bool useHighLodTerrain_ = true;
    std::mutex textureCacheLock_;
    std::unordered_map<string, Texture2D> textureCache_; //Textures loaded during territory load are cached to prevent repeat loads. Cleared once territory is done loading.
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
    bool DrawSolid = false; //If true bounding boxes are drawn fully solid instead of wireframe
};