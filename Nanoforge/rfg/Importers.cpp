#include "Importers.h"
#include "common/string/String.h"
#include <RfgTools++/formats/zones/ZoneFile.h>
#include <RfgTools++/hashes/HashGuesser.h>
#include <RfgTools++/hashes/Hash.h>
#include <vector>
#include <mutex>

//Defines a zone property type (string, uint, bool, etc) and a loader function
struct ZonePropertyType
{
    const std::vector<std::string_view> PropertyNames;
    const std::function<bool(ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName)> Loader;

    ZonePropertyType(const std::vector<std::string_view>& propertyNames, std::function<bool(ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName)> loader)
        : PropertyNames(propertyNames), Loader(loader)
    {

    }
};
std::vector<ZonePropertyType> ZonePropertyTypes = {};
std::mutex ZonePropertyTypesMutex;

void InitZonePropertyLoaders();
bool ImportZoneObjectProperty(ZoneObjectProperty* prop, ObjectHandle zoneObject);

//Convert rfg zone file to a set of registry objects
ObjectHandle Importers::ImportZoneFile(ZoneFile& zoneFile)
{
    static bool firstRun = true;
    if (firstRun)
    {
        ZonePropertyTypesMutex.lock();
        if (firstRun)
        {
            InitZonePropertyLoaders();
            firstRun = false;
        }
        ZonePropertyTypesMutex.unlock();
    }

    Registry& registry = Registry::Get();
    ObjectHandle zone = registry.CreateObject(zoneFile.Name, "Zone");

    //Convert zone header
    zone.GetOrCreateProperty("Signature").Set<u32>(zoneFile.Header.Signature);
    zone.GetOrCreateProperty("Version").Set<u32>(zoneFile.Header.Version);
    zone.GetOrCreateProperty("NumObjects").Set<u32>(zoneFile.Header.NumObjects);
    zone.GetOrCreateProperty("NumHandles").Set<u32>(zoneFile.Header.NumHandles);
    zone.GetOrCreateProperty("DistrictHash").Set<u32>(zoneFile.Header.DistrictHash);
    zone.GetOrCreateProperty("DistrictFlags").Set<u32>(zoneFile.Header.DistrictFlags);
    zone.GetOrCreateProperty("Name").Set<string>(zoneFile.Name);
    zone.GetOrCreateProperty("Persistent").Set<bool>(String::StartsWith(zoneFile.Name, "p_"));
    zone.GetOrCreateProperty("Objects").SetObjectList({});

    //Convert zone object
    ZoneObject* current = zoneFile.Objects;
    u32 j = 0;
    while (current)
    {
        //Create zone object and add to zone object list
        auto maybeClassname = current->Classname();
        string classname = maybeClassname.has_value() ? maybeClassname.value() : "UnknownObjectClass";
        ObjectHandle zoneObject = registry.CreateObject(classname, "ZoneFile");
        zone.GetProperty("Objects").GetObjectList().push_back(zoneObject);
        //TODO: Strip properties not needed by the editor
        //TODO: Replace parent/child/sibling properties with ObjectHandle variables on Object
        zoneObject.GetOrCreateProperty("ClassnameHash").Set<u32>(current->ClassnameHash);
        zoneObject.GetOrCreateProperty("Handle").Set<u32>(current->Handle);
        zoneObject.GetOrCreateProperty("Bmin").Set<Vec3>(current->Bmin);
        zoneObject.GetOrCreateProperty("Bmax").Set<Vec3>(current->Bmax);
        zoneObject.GetOrCreateProperty("Flags").Set<u16>(current->Flags);
        zoneObject.GetOrCreateProperty("BlockSize").Set<u16>(current->BlockSize);
        zoneObject.GetOrCreateProperty("Parent").Set<u32>(current->Parent);
        zoneObject.GetOrCreateProperty("Sibling").Set<u32>(current->Sibling);
        zoneObject.GetOrCreateProperty("Child").Set<u32>(current->Child);
        zoneObject.GetOrCreateProperty("Num").Set<u32>(current->Num);
        zoneObject.GetOrCreateProperty("NumProps").Set<u16>(current->NumProps);
        zoneObject.GetOrCreateProperty("PropBlockSize").Set<u16>(current->PropBlockSize);
        zoneObject.GetOrCreateProperty("Classname").Set<string>(classname);
        zoneObject.GetOrCreateProperty("Properties").SetObjectList({});

        //Setup properties
        ZoneObjectProperty* currentProp = current->Properties();
        u32 i = 0;
        while (currentProp && i < current->NumProps)
        {
            //Create property object
            auto maybeName = currentProp->Name();
            string name = maybeName.has_value() ? maybeName.value() : "UnknownPropertyType";
            ObjectHandle prop = registry.CreateObject(name, "ZoneObjectProperty");
            prop.GetOrCreateProperty("Type").Set<u16>(currentProp->Type);
            prop.GetOrCreateProperty("Size").Set<u16>(currentProp->Size);
            prop.GetOrCreateProperty("NameHash").Set<u32>(currentProp->NameHash);
            prop.GetOrCreateProperty("Name").Set<string>(name);

            //Attempt to load property
            if (ImportZoneObjectProperty(currentProp, prop))
            {
                zoneObject.GetOrCreateProperty("Properties").GetObjectList().push_back(prop);
            }
            else
            {

            }

            currentProp = current->NextProperty(currentProp);
            i++;
        }

        current = zoneFile.NextObject(current);
        j++;
    }

    //Determine zone position
    ObjectHandle objZone = GetZoneObject(zone, "obj_zone");
    ObjectHandle op = objZone.Valid() ? GetZoneProperty(objZone, "op") : NullObjectHandle;
    if (objZone.Valid() && op.Valid())
    {
        //Use obj_zone position if available. They're always at the center of the zone
        zone.GetOrCreateProperty("Position").Set<Vec3>(op.GetOrCreateProperty("Position").Get<Vec3>());
    }
    else
    {
        //Otherwise take the average of all object positions
        Vec3 averagePosition;
        size_t numPositions = 0;
        for (ObjectHandle obj : zone.GetOrCreateProperty("Objects").GetObjectList())
        {
            ObjectHandle op2 = GetZoneProperty(obj, "op");
            if (op2.Valid())
            {
                averagePosition += op2.GetOrCreateProperty("Position").Get<Vec3>();
                numPositions++;
            }
        }
        zone.GetOrCreateProperty("Position").Set<Vec3>(averagePosition / (f32)numPositions);
    }

    //Todo: Make hierarchical set of objects either using reference properties or parent + sibling value

    return zone;
}

//Define supported zone object properties and their loader functions
void InitZonePropertyLoaders()
{
    //String or enum properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "district", "terrain_file_name", "ambient_spawn", "mission_info", "mp_team", "item_type", "default_orders", "squad_def", "respawn_speed", "vehicle_type",
            "spawn_set", "chunk_name", "props", "building_type", "gameplay_props", "chunk_flags", "display_name", "turret_type", "animation_type", "weapon_type",
            "bounding_box_type", "trigger_shape", "trigger_flags", "region_kill_type", "region_type", "convoy_type", "home_district", "raid_type", "house_arrest_type",
            "activity_type", "delivery_type", "courier_type", "streamed_effect", "display_name_tag", "upgrade_type", "riding_shotgun_type", "area_defense_type",
            "dummy_type", "demolitions_master_type", "team", "sound_alr", "sound", "visual", "behavior", "roadblock_type", "type_enum", "clip_mesh", "light_flags",
            "backpack_type", "marker_type", "area_type", "spawn_resource_data", "parent_name"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            const char* data = (const char*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<string>(data);
            return true;
        }
    );

    //Bool properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "respawn", "respawns", "checkpoint_respawns", "initial_spawn", "activity_respawn", "special_npc", "safehouse_vip", "special_vehicle", "hands_off_raid_squad",
            "radio_operator", "squad_vehicle", "miner_persona", "raid_spawn", "no_reassignment", "disable_ambient_parking", "player_vehicle_respawn", "no_defensive_combat",
            "preplaced", "enabled", "indoor", "no_stub", "autostart", "high_priority", "run_to", "infinite_duration", "no_check_in", "combat_ready", "looping_patrol",
            "marauder_raid", "ASD_truck_partol" /*Note: Typo also in game. Keeping for compatibility sake.*/, "courier_patrol", "override_patrol", "allow_ambient_peds",
            "disabled", "tag_node", "start_node", "end_game_only", "visible", "vehicle_only", "npc_only", "dead_body", "looping", "use_object_orient", "random_backpacks",
            "liberated", "liberated_play_line"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            bool* data = (bool*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<bool>(*data);
            return true;
        }
    );

    //Float properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "wind_min_speed", "wind_max_speed", "spawn_prob", "night_spawn_prob", "angle_left", "angle_right", "rotation_limit", "game_destroyed_pct", "outer_radius",
            "night_trigger_prob", "day_trigger_prob", "speed_limit", "hotspot_falloff_size", "atten_range", "aspect", "hotspot_size", "atten_start", "control",
            "control_max", "morale", "morale_max"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            f32* data = (f32*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<f32>(*data);
            return true;
        }
    );

    //Unsigned int properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "gm_flags", "dest_checksum", "uid", "next", "prev", "mtype", "group_id", "ladder_rungs", "min_ambush_squads", "max_ambush_squads", "host_index",
            "child_index", "child_alt_hk_body_index", "host_handle", "child_handle", "path_road_flags", "patrol_start", "yellow_num_points", "yellow_num_triangles",
            "warning_num_points", "warning_num_triangles", "pair_number", "group", "priority", "num_backpacks"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            u32* data = (u32*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<u32>(*data);
            return true;
        }
   );

    //Vec3 properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "just_pos", "min_clip", "max_clip", "clr_orig"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            Vec3* data = (Vec3*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<Vec3>(*data);
            return true;
        }
    );

    //Matrix33 properties
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "nav_orient"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            Mat3* data = (Mat3*)prop->Data();
            zoneObject.GetOrCreateProperty(propertyName).Set<Mat3>(*data);
            return true;
        }
    );

    //Bounding box property
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "bb"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            struct bb { Vec3 bmin; Vec3 bmax; };
            bb* data = (bb*)prop->Data();
            zoneObject.GetOrCreateProperty("bmin").Set<Vec3>(data->bmin);
            zoneObject.GetOrCreateProperty("bmax").Set<Vec3>(data->bmax);
            return true;
        }
    );

    //Op property
    ZonePropertyTypes.emplace_back
    (
        std::vector<std::string_view>
        {
            "op"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            struct op { Vec3 Position; Mat3 Orient; };
            op* data = (op*)prop->Data();
            zoneObject.GetOrCreateProperty("Position").Set<Vec3>(data->Position);
            zoneObject.GetOrCreateProperty("Orient").Set<Mat3>(data->Orient);
            return true;
        }
    );

    //TODO: Implement DistrictFlags property
    //TODO: Implement list properties
    //TODO: Implement constraint properties
}

//Convert a zone object property to a registry
bool ImportZoneObjectProperty(ZoneObjectProperty* prop, ObjectHandle zoneObject)
{
    auto maybeName = prop->Name();
    if (!maybeName.has_value())
        return false;

    string name = maybeName.value();
    for (ZonePropertyType& propType : ZonePropertyTypes)
        for (const std::string_view propName : propType.PropertyNames)
            if (propName == name)
                return propType.Loader(prop, zoneObject, name);

    return false;
}

ObjectHandle GetZoneObject(ObjectHandle zone, std::string_view classname)
{
    for (ObjectHandle obj : zone.GetOrCreateProperty("Objects").GetObjectList())
        if (obj.GetOrCreateProperty("Classname").Get<string>() == classname)
            return obj;

    return NullObjectHandle;
}

ObjectHandle GetZoneProperty(ObjectHandle obj, std::string_view propertyName)
{
    for (ObjectHandle prop : obj.GetOrCreateProperty("Properties").GetObjectList())
        if (prop.GetOrCreateProperty("Name").Get<string>() == propertyName)
            return prop;

    return NullObjectHandle;
}