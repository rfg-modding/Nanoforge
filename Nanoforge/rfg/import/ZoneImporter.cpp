#include "Importers.h"
#include "common/string/String.h"
#include "common/filesystem/Path.h"
#include <RfgTools++/formats/zones/ZoneFile.h>
#include <RfgTools++/hashes/HashGuesser.h>
#include <RfgTools++/hashes/Hash.h>
#include <vector>
#include <mutex>

//Defines a zone property type (string, uint, bool, etc) and a loader function
struct ZonePropertyType
{
    const string Name;
    const std::vector<std::string_view> PropertyNames;
    const std::function<bool(ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName)> Loader;

    ZonePropertyType(const string& name, const std::vector<std::string_view>& propertyNames, std::function<bool(ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName)> loader)
        : Name(name), PropertyNames(propertyNames), Loader(loader)
    {

    }
};
std::vector<ZonePropertyType> ZonePropertyTypes = {};
std::mutex ZonePropertyTypesMutex;

void InitZonePropertyLoaders();
bool ImportZoneObjectProperty(ZoneObjectProperty* prop, ObjectHandle zoneObject);

//Load zone file and convert it to the editor data format
ObjectHandle Importers::ImportZoneFile(const std::vector<u8>& zoneBytes, std::string_view filename, std::string_view territoryName)
{
    //Convert zone file to editor data format
    std::optional<ZoneFile> parseResult = ZoneFile::Read(std::span<u8>{(u8*)zoneBytes.data(), zoneBytes.size()}, string(filename));
    if (!parseResult.has_value())
    {
        LOG_ERROR("Failed to parse zone file '{}' in '{}'", filename, territoryName);
        return NullObjectHandle;
    }

    return Importers::ImportZoneFile(parseResult.value());
}

//Convert rfg zone file to a set of registry objects
ObjectHandle Importers::ImportZoneFile(ZoneFile& zoneFile)
{
    static bool firstRun = true;
    if (firstRun)
    {
        ZonePropertyTypesMutex.lock();
        if (firstRun) //Second check needed in case another thread handled init while this one was waiting for lock
        {
            InitZonePropertyLoaders();
            firstRun = false;
        }
        ZonePropertyTypesMutex.unlock();
    }

    Registry& registry = Registry::Get();
    ObjectHandle zone = registry.CreateObject(zoneFile.Name, "Zone");

    //Convert zone header
    zone.Property("Signature").Set<u32>(zoneFile.Header.Signature);
    zone.Property("Version").Set<u32>(zoneFile.Header.Version);
    zone.Property("NumObjects").Set<u32>(zoneFile.Header.NumObjects);
    zone.Property("NumHandles").Set<u32>(zoneFile.Header.NumHandles);
    zone.Property("DistrictHash").Set<u32>(zoneFile.Header.DistrictHash);
    zone.Property("DistrictFlags").Set<u32>(zoneFile.Header.DistrictFlags);
    zone.Property("DistrictName").Set<string>(zoneFile.DistrictName());
    zone.Property("Name").Set<string>(zoneFile.Name);
    zone.Property("Persistent").Set<bool>(String::StartsWith(zoneFile.Name, "p_"));
    zone.Property("ActivityLayer").Set<bool>(false);
    zone.Property("MissionLayer").Set<bool>(false);
    zone.Property("RenderBoundingBoxes").Set<bool>(true);

    //Convert zone object
    ZoneObject* current = zoneFile.Objects;
    u32 j = 0;
    while (current)
    {
        //Create zone object and add to zone object list
        string classname = current->Classname();
        ObjectHandle zoneObject = registry.CreateObject(classname);
        zone.Property("Objects").GetObjectList().push_back(zoneObject);
        //TODO: Strip properties not needed by the editor
        //TODO: Replace parent/child/sibling properties with ObjectHandle variables on Object
        zoneObject.Property("ClassnameHash").Set<u32>(current->ClassnameHash);
        zoneObject.Property("Handle").Set<u32>(current->Handle);
        zoneObject.Property("Bmin").Set<Vec3>(current->Bmin);
        zoneObject.Property("Bmax").Set<Vec3>(current->Bmax);
        zoneObject.Property("Flags").Set<u16>(current->Flags);
        zoneObject.Property("BlockSize").Set<u16>(current->BlockSize);
        zoneObject.Property("ParentHandle").Set<u32>(current->Parent);
        zoneObject.Property("SiblingHandle").Set<u32>(current->Sibling);
        zoneObject.Property("ChildHandle").Set<u32>(current->Child);
        zoneObject.Property("Num").Set<u32>(current->Num);
        zoneObject.Property("NumProps").Set<u16>(current->NumProps);
        zoneObject.Property("PropBlockSize").Set<u16>(current->PropBlockSize);
        if (classname != "Unknown")
            zoneObject.Property("Type").Set<string>(classname);
        zoneObject.Property("Parent").Set<ObjectHandle>(NullObjectHandle);
        zoneObject.Property("Child").Set<ObjectHandle>(NullObjectHandle);
        zoneObject.Property("Sibling").Set<ObjectHandle>(NullObjectHandle);

        //Load zone object properties
        ZoneObjectProperty* currentProp = current->Properties();
        u32 i = 0;
        while (currentProp && i < current->NumProps)
        {
            //Attempt to load property
            auto maybeName = currentProp->Name();
            string name = maybeName.has_value() ? maybeName.value() : "UnknownPropertyType";
            PropertyHandle prop0 = zoneObject.Property(name);
            if (!ImportZoneObjectProperty(currentProp, zoneObject))
            {
                //Todo: Add some kind of error handling here. Waiting until edit tracking + undo/redo is implemented since that'll make it simple.
            }

            currentProp = current->NextProperty(currentProp);
            i++;
        }

        current = zoneFile.NextObject(current);
        j++;
    }

    //Determine zone position
    ObjectHandle objZone = GetZoneObject(zone, "obj_zone");
    if (objZone.Has("Position"))
    {
        //Use obj_zone position if available. They're always at the center of the zone
        zone.Property("Position").Set(objZone.Property("Position").Get<Vec3>());
    }
    else
    {
        zone.Property("Position").Set<Vec3>({0.0f, 0.0f, 0.0f});
    }

    std::vector<ObjectHandle> objects = zone.Property("Objects").GetObjectList();
    auto getObject = [&](u32 handle) -> ObjectHandle
    {
        for (ObjectHandle object : objects)
            if (object.Property("Handle").Get<u32>() == handle)
                return object;

        return NullObjectHandle;
    };

    //Set references to relative objects. Forms tree of object relations.
    const u32 InvalidHandle = 0xFFFFFFFF;
    for (ObjectHandle object : objects)
    {
        //Set parent references + add references child references to parent
        u32 parentHandle = object.Property("ParentHandle").Get<u32>();
        ObjectHandle parent = getObject(parentHandle);
        if (parent)
        {
            object.Property("Parent").Set<ObjectHandle>(parent);
            parent.SubObjects().push_back(object);
        }
        else
        {
            //TODO: Try searching other zones for object parents
        }

        //Set sibling references
        ObjectHandle current = object;
        u32 siblingHandle = current.Property("SiblingHandle").Get<u32>();
        while (siblingHandle != InvalidHandle)
        {
            ObjectHandle sibling = getObject(siblingHandle);
            if (!sibling)
                break;

            current.Property("Sibling").Set<ObjectHandle>(sibling);//.SetObjectRef(sibling);
            current = sibling;
            siblingHandle = current.Property("SiblingHandle").Get<u32>();
        }
    }

    //Set custom object names based on properties
    const std::vector<string> customNameProperties =
    {
        "display_name", "chunk_name", "animation_type", "activity_type", "raid_type", "courier_type",
        "spawn_set", "item_type", "dummy_type", "weapon_type", "region_kill_type", "delivery_type",
        "squad_def", "mission_info"
    };
    for (ObjectHandle object : objects)
    {
        string name = "";
        for (PropertyHandle prop : object.Properties())
        {
            for (const string& propertyName : customNameProperties)
            {
                if (prop.IsType<string>() && prop.Name() == propertyName)
                {
                    name = prop.Get<string>();
                    break;
                }
            }
        }
        object.Property("Name").Set<string>(name);
    }

    return zone;
}

//Define supported zone object properties and their loader functions
void InitZonePropertyLoaders()
{
    //String or enum properties
    ZonePropertyTypes.emplace_back
    (
        "String",
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
            zoneObject.Property(propertyName).Set<string>(data);
            return true;
        }
    );

    //Bool properties
    ZonePropertyTypes.emplace_back
    (
        "Bool",
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
            zoneObject.Property(propertyName).Set<bool>(*data);
            return true;
        }
    );

    //Float properties
    ZonePropertyTypes.emplace_back
    (
        "Float",
        std::vector<std::string_view>
        {
            "wind_min_speed", "wind_max_speed", "spawn_prob", "night_spawn_prob", "angle_left", "angle_right", "rotation_limit", "game_destroyed_pct", "outer_radius",
            "night_trigger_prob", "day_trigger_prob", "speed_limit", "hotspot_falloff_size", "atten_range", "aspect", "hotspot_size", "atten_start", "control",
            "control_max", "morale", "morale_max"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            f32* data = (f32*)prop->Data();
            zoneObject.Property(propertyName).Set<f32>(*data);
            return true;
        }
    );

    //Unsigned int properties
    ZonePropertyTypes.emplace_back
    (
        "Uint",
        std::vector<std::string_view>
        {
            "gm_flags", "dest_checksum", "uid", "next", "prev", "mtype", "group_id", "ladder_rungs", "min_ambush_squads", "max_ambush_squads", "host_index",
            "child_index", "child_alt_hk_body_index", "host_handle", "child_handle", "path_road_flags", "patrol_start", "yellow_num_points", "yellow_num_triangles",
            "warning_num_points", "warning_num_triangles", "pair_number", "group", "priority", "num_backpacks"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            u32* data = (u32*)prop->Data();
            zoneObject.Property(propertyName).Set<u32>(*data);
            return true;
        }
   );

    //Vec3 properties
    ZonePropertyTypes.emplace_back
    (
        "Vec3",
        std::vector<std::string_view>
        {
            "just_pos", "min_clip", "max_clip", "clr_orig"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            Vec3* data = (Vec3*)prop->Data();
            if (propertyName == "just_pos")
            {
#ifdef DEBUG_BUILD
                if (zoneObject.Has("Position"))
                    Log->warn("Found just_pos in a zone object that already has a position set!"); //RFG zone objects should have either op (other prop that sets Position) or just_pos, not both.
#endif
                zoneObject.Property("Position").Set<Vec3>(*data); //Game uses multiple properties for position. Nanoforge only stores in Position
            }
            else
            {
                zoneObject.Property(propertyName).Set<Vec3>(*data);
            }
            return true;
        }
    );

    //Matrix33 properties
    ZonePropertyTypes.emplace_back
    (
        "Matrix33",
        std::vector<std::string_view>
        {
            "nav_orient"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            Mat3* data = (Mat3*)prop->Data();
            zoneObject.Property(propertyName).Set<Mat3>(*data);
            return true;
        }
    );

    //Bounding box property
    ZonePropertyTypes.emplace_back
    (
        "BoundingBox",
        std::vector<std::string_view>
        {
            "bb"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            struct bb { Vec3 bmin; Vec3 bmax; };
            bb* data = (bb*)prop->Data();
            zoneObject.Property("BBmin").Set<Vec3>(data->bmin);
            zoneObject.Property("BBmax").Set<Vec3>(data->bmax);
            return true;
        }
    );

    //Op property
    ZonePropertyTypes.emplace_back
    (
        "Op",
        std::vector<std::string_view>
        {
            "op"
        },
        [](ZoneObjectProperty* prop, ObjectHandle zoneObject, std::string_view propertyName) -> bool
        {
            struct op { Vec3 Position; Mat3 Orient; };
            op* data = (op*)prop->Data();
            zoneObject.Property("Position").Set<Vec3>(data->Position);
            zoneObject.Property("Orient").Set<Mat3>(data->Orient);
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
            {
                return propType.Loader(prop, zoneObject, name);
            }

    return false;
}

ObjectHandle GetZoneObject(ObjectHandle zone, std::string_view type)
{
    for (ObjectHandle obj : zone.Property("Objects").GetObjectList())
        if (obj.Property("Type").Get<string>() == type)
            return obj;

    return NullObjectHandle;
}

PropertyHandle GetZoneProperty(ObjectHandle obj, std::string_view propertyName)
{
    for (PropertyHandle prop : obj.Properties())
        if (prop.Name() == propertyName)
            return prop;

    return NullPropertyHandle;
}