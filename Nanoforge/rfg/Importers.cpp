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
ObjectHandle Importers::ImportZoneFile(const std::vector<u8>& zoneBytes, const std::string& filename, const std::string& territoryName)
{
    //Convert zone file to editor data format
    ObjectHandle zone = NullObjectHandle;
    if (auto result = ZoneFile::Read(std::span<u8>{(u8*)zoneBytes.data(), zoneBytes.size()}, filename); result.has_value())
    {
        ZoneFile& zoneFile = result.value();
        zone = Importers::ImportZoneFile(zoneFile);
        return zone;
    }
    else
    {
        LOG_ERROR("Failed to parse zone file \"{}\" from \"{}\".", Path::GetFileName(filename), territoryName);
        return NullObjectHandle;
    }
}

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
    zone.GetOrCreateProperty("DistrictName").Set<string>(zoneFile.DistrictName());
    zone.GetOrCreateProperty("Name").Set<string>(zoneFile.Name);
    zone.GetOrCreateProperty("Persistent").Set<bool>(String::StartsWith(zoneFile.Name, "p_"));
    zone.GetOrCreateProperty("Objects").SetObjectList({});
    zone.GetOrCreateProperty("ActivityLayer").Set<bool>(false);
    zone.GetOrCreateProperty("MissionLayer").Set<bool>(false);
    zone.GetOrCreateProperty("RenderBoundingBoxes").Set<bool>(true);

    //Convert zone object
    ZoneObject* current = zoneFile.Objects;
    u32 j = 0;
    while (current)
    {
        //Create zone object and add to zone object list
        string classname = current->Classname();
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
        zoneObject.GetOrCreateProperty("ParentHandle").Set<u32>(current->Parent);
        zoneObject.GetOrCreateProperty("SiblingHandle").Set<u32>(current->Sibling);
        zoneObject.GetOrCreateProperty("ChildHandle").Set<u32>(current->Child);
        zoneObject.GetOrCreateProperty("Num").Set<u32>(current->Num);
        zoneObject.GetOrCreateProperty("NumProps").Set<u16>(current->NumProps);
        zoneObject.GetOrCreateProperty("PropBlockSize").Set<u16>(current->PropBlockSize);
        zoneObject.GetOrCreateProperty("Classname").Set<string>(classname);
        zoneObject.GetOrCreateProperty("Properties").SetObjectList({});
        zoneObject.GetOrCreateProperty("Parent").Set<u64>(NullUID);
        zoneObject.GetOrCreateProperty("Child").Set<u64>(NullUID);
        zoneObject.GetOrCreateProperty("Sibling").Set<u64>(NullUID);

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

    std::vector<ObjectHandle> objects = zone.GetProperty("Objects").GetObjectList();
    auto getObject = [&](u32 handle) -> ObjectHandle
    {
        for (ObjectHandle object : objects)
            if (object.GetProperty("Handle").Get<u32>() == handle)
                return object;

        return NullObjectHandle;
    };

    //Set references to relative objects. Forms tree of object relations.
    const u32 InvalidHandle = 0xFFFFFFFF;
    for (ObjectHandle object : objects)
    {
        //Set parent references + add references child references to parent
        u32 parentHandle = object.GetProperty("ParentHandle").Get<u32>();
        if (parentHandle == InvalidHandle)
        {
            ObjectHandle parent = getObject(parentHandle);
            if (parent.Valid())
            {
                object.GetOrCreateProperty("Parent").Set<u64>(parent.UID());
                parent.SubObjects().push_back(object.UID());
            }
            else
            {
                //TODO: Try searching other zones for object parents
            }
        }

        //Set sibling references
        ObjectHandle current = object;
        u32 siblingHandle = current.GetOrCreateProperty("SiblingHandle").Get<u32>();
        while (siblingHandle != InvalidHandle)
        {
            ObjectHandle sibling = getObject(siblingHandle);
            if (!sibling.Valid())
                break;

            current.GetOrCreateProperty("Sibling").Set<u64>(sibling.UID());//.SetObjectRef(sibling);
            current = sibling;
            siblingHandle = current.GetOrCreateProperty("SiblingHandle").Get<u32>();
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
        std::vector<ObjectHandle> properties = object.GetOrCreateProperty("Properties").GetObjectList();
        for (ObjectHandle prop : properties)
        {
            for (const string& propertyName : customNameProperties)
            {
                if (prop.GetProperty("TypeName").Get<string>() == "String" && prop.GetProperty("Name").Get<string>() == propertyName)
                {
                    name = prop.GetProperty("String").Get<string>();
                    break;
                }
            }
        }
        object.GetOrCreateProperty("Name").Set<string>(name);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            const char* data = (const char*)prop->Data();
            propObject.GetOrCreateProperty("String").Set<string>(data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            bool* data = (bool*)prop->Data();
            propObject.GetOrCreateProperty("Bool").Set<bool>(*data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            f32* data = (f32*)prop->Data();
            propObject.GetOrCreateProperty("Float").Set<f32>(*data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            u32* data = (u32*)prop->Data();
            propObject.GetOrCreateProperty("Uint").Set<u32>(*data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            Vec3* data = (Vec3*)prop->Data();
            propObject.GetOrCreateProperty("Vec3").Set<Vec3>(*data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            Mat3* data = (Mat3*)prop->Data();
            propObject.GetOrCreateProperty("Matrix33").Set<Mat3>(*data);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            struct bb { Vec3 bmin; Vec3 bmax; };
            bb* data = (bb*)prop->Data();
            propObject.GetOrCreateProperty("Bmin").Set<Vec3>(data->bmin);
            propObject.GetOrCreateProperty("Bmax").Set<Vec3>(data->bmax);
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
        [](ZoneObjectProperty* prop, ObjectHandle propObject, std::string_view propertyName) -> bool
        {
            struct op { Vec3 Position; Mat3 Orient; };
            op* data = (op*)prop->Data();
            propObject.GetOrCreateProperty("Position").Set<Vec3>(data->Position);
            propObject.GetOrCreateProperty("Orient").Set<Mat3>(data->Orient);
            return true;
        }
    );

    //TODO: Implement DistrictFlags property
    //TODO: Implement list properties
    //TODO: Implement constraint properties
}

//Convert a zone object property to a registry
bool ImportZoneObjectProperty(ZoneObjectProperty* prop, ObjectHandle propObject)
{
    auto maybeName = prop->Name();
    if (!maybeName.has_value())
        return false;

    string name = maybeName.value();
    for (ZonePropertyType& propType : ZonePropertyTypes)
        for (const std::string_view propName : propType.PropertyNames)
            if (propName == name)
            {
                propObject.GetOrCreateProperty("TypeName").Set<string>(propType.Name);
                return propType.Loader(prop, propObject, name);
                //bool result = propType.Loader(prop, propObject, name);
                //if (result)
                //    propObject.GetOrCreateProperty("Name").Set<string>(string(propName));
                //return result;
            }

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