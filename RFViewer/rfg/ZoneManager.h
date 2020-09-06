#pragma once
#include "common/Typedefs.h"
#include "PackfileVFS.h"
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\types\Vec4.h>

//Wrapper around ZonePc36 used by ZoneManager
struct ZoneFile
{
    string Name;
    string ShortName;
    ZonePc36 Zone;
    bool RenderBoundingBoxes = false;
    bool Persistent = false;
};

//Used by ZoneManager to filter objects list by class type
struct ZoneObjectClass
{
    string Name;
    u32 Hash = 0;
    u32 NumInstances = 0;
    //Todo: This is mixing ui logic with data. I'd like to separate this stuff out of ZoneManager if possible
    Vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool Show = true;
    bool ShowLabel = false;
    const char* LabelIcon = "";
};

constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

//Loads all zone files and tracks info about them and their contents
class ZoneManager
{
public:
    //Set values needed for it to function
    void Init(PackfileVFS* packfileVFS, const string& territoryFilename, const string& territoryShortname);
    //Load all zone files and gather info about them
    void LoadZoneData();
    //Reset / clear data in preparation for territory reload
    void ResetTerritoryData();

    //Whether or not this object should be shown based on filtering settings
    bool ShouldShowObjectClass(u32 classnameHash);
    //Checks if a object class is in the selected zones class list
    bool ObjectClassRegistered(u32 classnameHash, u32& outIndex);
    //Update number of instances of each object class for selected zone
    void UpdateObjectClassInstanceCounts(u32 selectedZone);
    //Scans all zone objects for any object class types that aren't known. Used for filtering and coloring purposes
    void InitObjectClassData();
    ZoneObjectClass& GetObjectClass(u32 classnameHash);

    std::vector<ZoneFile> ZoneFiles;
    std::vector<ZoneObjectClass> ZoneObjectClasses = {};

    u32 LongestZoneName = 0;

private:
    //Determine short name for zone if possible. E.g. terr01_07_02.rfgzone_pc -> 07_02
    //Goal is to hide unecessary info such as the territory, prefix, and extension where possible
    //Still has full name for situations where that info is useful or for users who prefer that format
    //Note: Assumes persistence prefix "p_" has already been checked for and that Name has already been set.
    void SetZoneShortName(ZoneFile& zone);

    PackfileVFS* packfileVFS_ = nullptr;
    //Name of the vpp_pc file that zone data is loaded from at startup
    string territoryFilename_;
    string territoryShortname_;
};