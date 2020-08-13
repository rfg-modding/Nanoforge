#include "ZoneManager.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "Log.h"

//Todo: Separate gui specific code into a different file or class
#include <IconsFontAwesome5_c.h>

void ZoneManager::Init(PackfileVFS* packfileVFS)
{
    packfileVFS_ = packfileVFS;
}

void ZoneManager::LoadZoneData()
{
    Packfile3* zonescriptVpp = packfileVFS_->GetPackfile("zonescript_terr01.vpp_pc");
    if (!zonescriptVpp)
        THROW_EXCEPTION("Error! Could not find zonescript_terr01.vpp_pc in data folder. Required for the program to function.");

    //Todo: Add search function with filters to packfile. Can base off of search functions in PackfileVFS
    for (u32 i = 0; i < zonescriptVpp->Entries.size(); i++)
    {
        const char* path = zonescriptVpp->EntryNames[i];
        string extension = Path::GetExtension(path);
        if (extension != ".rfgzone_pc" && extension != ".layer_pc")
            continue;

        auto fileBuffer = zonescriptVpp->ExtractSingleFile(path);
        if (!fileBuffer)
            THROW_EXCEPTION("Error! Failed to extract a zone file from zonescript_terr01.vpp_pc");

        //Todo: It'd be safer to do this all in a temporary vector and then .swap() it into the real one
        BinaryReader reader(fileBuffer.value());
        ZoneFile& zoneFile = ZoneFiles.emplace_back();
        zoneFile.Name = Path::GetFileName(std::filesystem::path(path));
        zoneFile.Zone.SetName(zoneFile.Name);
        zoneFile.Zone.Read(reader);
        zoneFile.Zone.GenerateObjectHierarchy();
        delete[] fileBuffer.value().data();
    }

    //Sort vector by object count for convenience
    std::sort(ZoneFiles.begin(), ZoneFiles.end(),
    [](const ZoneFile& a, const ZoneFile& b)
    {
        return a.Zone.Header.NumObjects > b.Zone.Header.NumObjects;
    });


    //Make first zone visible for convenience when debugging
    ZoneFiles[0].RenderBoundingBoxes = true;
    //Init object class data used for filtering and labelling
    InitObjectClassData();
}

bool ZoneManager::ShouldShowObjectClass(u32 classnameHash)
{
    for (const auto& objectClass : ZoneObjectClasses)
    {
        if (objectClass.Hash == classnameHash)
            return objectClass.Show;
    }
    return true;
}

bool ZoneManager::ObjectClassRegistered(u32 classnameHash, u32& outIndex)
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

void ZoneManager::UpdateObjectClassInstanceCounts(u32 selectedZone)
{
    //Zero instance counts for each object class
    for (auto& objectClass : ZoneObjectClasses)
        objectClass.NumInstances = 0;

    //Update instance count for each object class
    auto& zone = ZoneFiles[selectedZone].Zone;
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

    //Sort vector by instance count for convenience
    std::sort(ZoneObjectClasses.begin(), ZoneObjectClasses.end(),
    [](const ZoneObjectClass& a, const ZoneObjectClass& b)
    {
        return a.NumInstances > b.NumInstances;
    });
}

void ZoneManager::InitObjectClassData()
{
    ZoneObjectClasses =
    {
        {"rfg_mover",                      2898847573, 0, Vec4{ 0.923f, 0.648f, 0.0f, 1.0f }, true ,   false, ICON_FA_HOME " "},
        {"cover_node",                     3322951465, 0, Vec4{ 1.0f, 0.0f, 0.0f, 1.0f },     false,   false, ICON_FA_SHIELD_ALT " "},
        {"navpoint",                       4055578105, 0, Vec4{ 1.0f, 0.968f, 0.0f, 1.0f },   false,   false, ICON_FA_LOCATION_ARROW " "},
        {"general_mover",                  1435016567, 0, Vec4{ 0.738f, 0.0f, 0.0f, 1.0f },   true ,   false, ICON_FA_CUBES  " "},
        {"player_start",                   1794022917, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_STREET_VIEW " "},
        {"multi_object_marker",            1332551546, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_MAP_MARKER " "},
        {"weapon",                         2760055731, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_CROSSHAIRS " "},
        {"object_action_node",             2017715543, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_RUNNING " "},
        {"object_squad_spawn_node",        311451949,  0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_USERS " "},
        {"object_guard_node",              968050919,  0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_SHIELD_ALT " "},
        {"object_path_road",               3007680500, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_ROAD " "},
        {"shape_cutter",                   753322256,  0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_CUT " "},
        {"item",                           27482413,   0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TOOLS " "},
        {"object_vehicle_spawn_node",      3057427650, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_CAR_SIDE " "},
        {"ladder",                         1620465961, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_LEVEL_UP_ALT " "},
        {"constraint",                     1798059225, 0, Vec4{ 0.958f, 0.0f, 1.0f, 1.0f },   true ,   false, ICON_FA_LOCK " "},
        {"object_effect",                  2663183315, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_FIRE " "},
        //Todo: Want a better icon for this one
        {"trigger_region",                 2367895008, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BORDER_STYLE " "},
        {"object_bftp_node",               3005715123, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     false,   false, ICON_FA_BOMB " "},
        {"object_bounding_box",            2575178582, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BORDER_NONE " "},
        {"object_turret_spawn_node",       96035668,   0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_CROSSHAIRS " "},
        //Todo: Want a better icon for this one
        {"obj_zone",                       3740226015, 0, Vec4{ 0.935f, 0.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_SEARCH_LOCATION " "},
        {"object_patrol",                  3656745166, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_BINOCULARS " "},
        {"object_dummy",                   2671133140, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_MEH_BLANK " "},
        {"object_raid_node",               3006762854, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_CAR_CRASH " "},
        {"object_delivery_node",           1315235117, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_SHIPPING_FAST " "},
        {"marauder_ambush_region",         1783727054, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_USER_NINJA " "},
        {"unknown",                        0, 0,          Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_QUESTION_CIRCLE " "},
        {"object_activity_spawn",          2219327965, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_SCROLL " "},
        {"object_mission_start_node",      1536827764, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_MAP_MARKED " "},
        {"object_demolitions_master_node", 3497250449, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_BOMB " "},
        {"object_restricted_area",         3157693713, 0, Vec4{ 1.0f, 0.0f, 0.0f, 1.0f },     true ,   true,  ICON_FA_USER_SLASH " "},
        {"effect_streaming_node",          1742767984, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   true,  ICON_FA_SPINNER " "},
        {"object_house_arrest_node",       227226529,  0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_USER_LOCK " "},
        {"object_area_defense_node",       2107155776, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_USER_SHIELD " "},
        {"object_safehouse",               3291687510, 0, Vec4{ 0.0f, 0.905f, 1.0f, 1.0f },     true ,   false, ICON_FA_FIST_RAISED " "},
        {"object_convoy_end_point",        1466427822, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TRUCK_MOVING " "},
        {"object_courier_end_point",       3654824104, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_FLAG_CHECKERED " "},
        {"object_riding_shotgun_node",     1227520137, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_TRUCK_MONSTER " "},
        {"object_upgrade_node",            2502352132, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   false, ICON_FA_ARROW_UP " "},
        {"object_ambient_behavior_region", 2407660945, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   false, ICON_FA_TREE " "},
        {"object_roadblock_node",          2100364527, 0, Vec4{ 0.25f, 0.177f, 1.0f, 1.0f },  false,   true,  ICON_FA_HAND_PAPER " "},
        {"object_spawn_region",            1854373986, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   true,  ICON_FA_USER_PLUS " "},
        {"obj_light",                      2915886275, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f },     true ,   true,  ICON_FA_LIGHTBULB " "}
    };

    for (auto& zone : ZoneFiles)
    {
        for (auto& object : zone.Zone.Objects)
        {
            u32 outIndex = InvalidZoneIndex;
            if (!ObjectClassRegistered(object.ClassnameHash, outIndex))
            {
                ZoneObjectClasses.push_back({ object.Classname, object.ClassnameHash, 0, Vec4{ 1.0f, 1.0f, 1.0f, 1.0f }, true });
                Log->warn("Found unknown object class with hash {} and name \"{}\"", object.ClassnameHash, object.Classname);
            }
        }
    }
}

ZoneObjectClass& ZoneManager::GetObjectClass(u32 classnameHash)
{
    for (auto& objectClass : ZoneObjectClasses)
    {
        if (objectClass.Hash == classnameHash)
            return objectClass;
    }
    //Todo: Handle case of invalid hash. Returning std::optional would work
}
