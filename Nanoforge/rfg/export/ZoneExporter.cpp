#include "Exporters.h"
#include "BinaryTools/BinaryWriter.h"
#include "common/filesystem/File.h"
#include <RfgTools++/hashes/Hash.h>
#include <RfgTools++/formats/zones/ZoneFile.h>
#include <RfgTools++/formats/zones/ZoneProperties.h>
#include <vector>
#include <mutex>

//Defines a zone property type (string, uint, bool, etc) and a loader function
struct PropertyWriter
{
	const string Name;
	const std::vector<std::string_view> PropertyNames;
	const std::function<bool(BinaryWriter& writer, ObjectHandle object, std::string_view propertyName)> Writer;

	PropertyWriter(const string& name, const std::vector<std::string_view>& propertyNames, std::function<bool(BinaryWriter& writer, ObjectHandle object, std::string_view propertyName)> writer)
		: Name(name), PropertyNames(propertyNames), Writer(writer)
	{

	}
};
std::vector<PropertyWriter> ZonePropertyWriters = {};
std::mutex ZoneWriterTypesMutex;

void InitZonePropertyWriters();
bool WriteZoneObjectProperty(BinaryWriter& writer, ObjectHandle object, std::string_view propertyName);

constexpr u32 RFG_ZONE_VERSION = 36;

bool Exporters::ExportZone(ObjectHandle zone, std::string_view outputPath, bool persistent)
{
    static bool firstRun = true;
    if (firstRun)
    {
        ZoneWriterTypesMutex.lock();
        if (firstRun) //Second check needed in case another thread handled init while this one was waiting for lock
        {
            InitZonePropertyWriters();
            firstRun = false;
        }
        ZoneWriterTypesMutex.unlock();
    }

    std::vector<ObjectHandle> outObjects = {}; //Collect objects that we're writing
    for (ObjectHandle object : zone.GetObjectList("Objects"))
    {
        if (persistent && !object.Get<bool>("Persistent"))
            continue;
        if (object.Has("Deleted") && object.Get<bool>("Deleted"))
            continue;

        outObjects.push_back(object);
    }

    //Check if object limit is exceeded. This limit is based on the max object count of the relation data section.
    //The necessity of that section is still in question, so it may be possible to exceed this. Just playing it safe for now. No zone in the SP map exceeds 3000 objects
    //TODO: See if this limit can be exceeded. Likely depends on how necessary the relation data hash table is
    const size_t maxObjects = 7280;//std::numeric_limits<u16>::max();
    if (outObjects.size() > maxObjects)
    {
        LOG_ERROR("Failed to export {}. Zone has {} objects. This exceeds the maximum per zone object limit of {}", zone.Get<string>("Name"), outObjects.size(), maxObjects);
        return false;
    }

    //Write header
    BinaryWriter writer(outputPath);
    writer.WriteFixedLengthString("ZONE");
    writer.WriteUint32(RFG_ZONE_VERSION);
    writer.WriteUint32(outObjects.size());
    writer.WriteUint32(zone.Get<u32>("NumHandles"));
    writer.WriteUint32(zone.Get<u32>("DistrictHash"));
    writer.WriteUint32(zone.Get<u32>("DistrictFlags"));

    //Write relation data
    const bool hasRelationData = (zone.Get<u32>("DistrictFlags") & 5) == 0;
    if (hasRelationData)
    {
        //Write nothing initially. Generated after writing every object since we need to object offsets
        writer.WriteNullBytes(87368);
    }

    //Write objects
    std::vector<size_t> objectOffsets = {}; //Offset of each object in bytes relative to the start of object block
    const size_t objectBlockStart = writer.Position();
	for (ObjectHandle object : outObjects)
	{
        if (persistent && !object.Get<bool>("Persistent"))
            continue;
        if (object.Has("Deleted") && object.Get<bool>("Deleted"))
            continue;

        //Get object offset + validate
        const size_t objectStartPos = writer.Position();
        const size_t offset = objectStartPos - objectBlockStart;
        if (offset > (size_t)std::numeric_limits<u32>::max())
        {
            //Just a sanity check. If the zone file got to even half this size (2GB) it'd probably break the game
            LOG_ERROR("Failed to export {}. Object data exceeded limit of {} bytes", zone.Get<string>("Name"), std::numeric_limits<u32>::max());
            return false;
        }
        objectOffsets.push_back(offset);

        //Write null bytes for object header initially. We won't have its values until after we write the properties
        writer.WriteNullBytes(56);
        size_t numProps = 0;

		//Write object properties
		for (const string& propertyName : object.GetStringList("RfgPropertyNames"))
		{
            if (WriteZoneObjectProperty(writer, object, propertyName))
            {
                numProps++;
            }
            else
            {
                LOG_ERROR("Failed to write zone object property '{}' in '{}', object uid {}", propertyName, zone.Get<string>("Name"), object.UID());
            }
			writer.Align(4); //Align after each property
		}

        //Calculate size of object block and prop block + make sure they're within allowed limits
        const size_t objectEndPos = writer.Position();
        const u16 ObjectHeaderSize = 56;
        const size_t blockSize = objectEndPos - objectStartPos;
        const size_t propBlockSize = blockSize - ObjectHeaderSize;
        if (blockSize > std::numeric_limits<u16>::max() || propBlockSize > std::numeric_limits<u16>::max() || numProps > std::numeric_limits<u16>::max())
        {
            Log->error("Zone export error. Object {} exceeded a size limit. block size = {}, prop block size = {}, num props = {}", object.UID(), blockSize, propBlockSize, numProps);
            return false;
        }

        //Write object header
        writer.SeekBeg(objectStartPos);
        writer.Write(object.Get<u32>("ClassnameHash"));
        writer.Write(object.Get<u32>("Handle"));
        writer.Write(object.Get<Vec3>("Bmin"));
        writer.Write(object.Get<Vec3>("Bmax"));
        writer.Write(object.Get<u16>("Flags"));
        writer.Write<u16>((u16)blockSize);
        writer.Write(object.Get<u32>("ParentHandle"));
        writer.Write(object.Get<u32>("SiblingHandle"));
        writer.Write(object.Get<u32>("ChildHandle"));
        writer.Write(object.Get<u32>("Num"));
        writer.Write<u16>((u16)numProps);
        writer.Write<u16>((u16)propBlockSize);
        writer.SeekBeg(objectEndPos); //Jump to end of object data so next object can be written
	}

    if (hasRelationData)
    {
        //u8 Padding0[4];
        //u16 Free;
        //u16 Slot[7280];
        //u16 Next[7280];
        //u8 Padding1[2];
        //u32 Keys[7280];
        //u32 Values[7280];

        auto GetSlotIndex = [](u32 key) -> u32
        {
            u32 result = 0;
            result = ~(key << 15) + key;
            result = ((u32)((i32)result >> 10) ^ result) * 9;
            result ^= (u32)((i32)result >> 6);
            result += ~(result << 11);
            result = ((u32)((i32)result >> 16) ^ result) % 7280;
            return result;
        };

        ZoneFileRelationData hashTable;
        hashTable.Free = outObjects.size(); //Next free key/value index
        for (size_t i = 0; i < 7280; i++)
        {
            hashTable.Slot[i] = 65535;
            hashTable.Next[i] = i + 1;
            hashTable.Keys[i] = 0;
            hashTable.Values[i] = 0;
        }

        for (size_t i = 0; i < outObjects.size(); i++)
        {
            ObjectHandle object = outObjects[i];
            u32 key = object.Get<u32>("Handle");
            u32 value = objectOffsets[i]; //Offset in bytes relative to the start of the object list
            u32 slotIndex = GetSlotIndex(key);
            hashTable.Slot[slotIndex] = i; //Note: Slot[slotIndex] doesn't always equal i in the vanilla files. Doesn't seem to be an issue with GetSlotIndex() since the games version of that function returns the same values
            hashTable.Keys[i] = key;
            hashTable.Values[i] = value;
        }

        //Writer hash table to file
        writer.SeekBeg(sizeof(ZoneFileHeader)); //This section is always right after the header
        writer.Write<ZoneFileRelationData>(hashTable);
    }

	return true;
}

//Init writers for each RFG property by name
void InitZonePropertyWriters()
{
    //String or enum properties
    ZonePropertyWriters.emplace_back
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
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const string& str = object.Get<string>(propertyName);
            writer.Write<u16>(4); //Property type
            writer.Write<u16>(str.size() + 1); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.WriteNullTerminatedString(str);
            return true;
        }
    );

    //Bool properties
    ZonePropertyWriters.emplace_back
    (
        "Bool",
        std::vector<std::string_view>
        {
            "respawn", "respawns", "checkpoint_respawn", "initial_spawn", "activity_respawn", "special_npc", "safehouse_vip", "special_vehicle", "hands_off_raid_squad",
            "radio_operator", "squad_vehicle", "miner_persona", "raid_spawn", "no_reassignment", "disable_ambient_parking", "player_vehicle_respawn", "no_defensive_combat",
            "preplaced", "enabled", "indoor", "no_stub", "autostart", "high_priority", "run_to", "infinite_duration", "no_check_in", "combat_ready", "looping_patrol",
            "marauder_raid", "ASD_truck_partol" /*Note: Typo also in game. Must keep to be compatible with the game*/, "courier_patrol", "override_patrol", "allow_ambient_peds",
            "disabled", "tag_node", "start_node", "end_game_only", "visible", "vehicle_only", "npc_only", "dead_body", "looping", "use_object_orient", "random_backpacks",
            "liberated", "liberated_play_line"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const bool data = object.Get<bool>(propertyName);
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(bool)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<bool>(data);
            return true;
        }
    );

    //Float properties
    ZonePropertyWriters.emplace_back
    (
        "Float",
        std::vector<std::string_view>
        {
            "wind_min_speed", "wind_max_speed", "spawn_prob", "night_spawn_prob", "angle_left", "angle_right", "rotation_limit", "game_destroyed_pct", "outer_radius",
            "night_trigger_prob", "day_trigger_prob", "speed_limit", "hotspot_falloff_size", "atten_range", "aspect", "hotspot_size", "atten_start", "control",
            "control_max", "morale", "morale_max"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const f32 data = object.Get<f32>(propertyName);
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(f32)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<f32>(data);
            return true;
        }
    );

    //Unsigned int properties
    ZonePropertyWriters.emplace_back
    (
        "Uint",
        std::vector<std::string_view>
        {
            "gm_flags", "dest_checksum", "uid", "next", "prev", "mtype", "group_id", "ladder_rungs", "min_ambush_squads", "max_ambush_squads", "host_index",
            "child_index", "child_alt_hk_body_index", "host_handle", "child_handle", "path_road_flags", "patrol_start", "yellow_num_points", "yellow_num_triangles",
            "warning_num_points", "warning_num_triangles", "pair_number", "group", "priority", "num_backpacks"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const u32 data = object.Get<u32>(propertyName);
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(u32)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<u32>(data);
            return true;
        }
    );

    //Vec3 properties
    ZonePropertyWriters.emplace_back
    (
        "Vec3",
        std::vector<std::string_view>
        {
            "min_clip", "max_clip", "clr_orig"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const Vec3& data = object.Get<Vec3>(propertyName);
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(Vec3)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<Vec3>(data);
            return true;
        }
    );

    //Matrix33 properties
    ZonePropertyWriters.emplace_back
    (
        "Matrix33",
        std::vector<std::string_view>
        {
            "nav_orient"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            const Mat3& data = object.Get<Mat3>(propertyName);
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(Mat3)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<Mat3>(data);
            return true;
        }
    );

    //Bounding box property
    ZonePropertyWriters.emplace_back
    (
        "BoundingBox",
        std::vector<std::string_view>
        {
            "bb"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("BBmin") || !object.Has("BBmax"))
                return false;

            Vec3 bmin = object.Get<Vec3>("BBmin");
            Vec3 bmax = object.Get<Vec3>("BBmax");
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(Vec3) * 2); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<Vec3>(bmin);
            writer.Write<Vec3>(bmax);
            return true;
        }
    );

    //Op property
    ZonePropertyWriters.emplace_back
    (
        "Op",
        std::vector<std::string_view>
        {
            "op"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("Position") || !object.Has("Orient"))
                return false;

            const Vec3& pos = object.Get<Vec3>("Position");
            const Mat3& orient = object.Get<Mat3>("Orient");
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(Vec3) + sizeof(Mat3)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<Vec3>(pos);
            writer.Write<Mat3>(orient);
            return true;
        }
    );

    //Special case for just_pos
    ZonePropertyWriters.emplace_back
    (
        "just_pos",
        std::vector<std::string_view>
        {
            "just_pos"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("Position"))
                return false;

            const Vec3& data = object.Get<Vec3>("Position");
            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(Vec3)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC("just_pos", 0)); //Property name hash
            writer.Write<Vec3>(data);
            return true;
        }
    );

    ZonePropertyWriters.emplace_back
    (
        "DistrictFlags",
        std::vector<std::string_view>
        {
            "district_flags"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            //Calculate district flags
            u32 flags = (u32)DistrictFlags::None; //Todo: Add operator overload so this can be done without casting
            if (object.Has("AllowCough") && object.Get<bool>("AllowCough"))
                flags |= (u32)DistrictFlags::AllowCough;
            if (object.Has("AllowAmbEdfCivilianDump") && object.Get<bool>("AllowAmbEdfCivilianDump"))
                flags |= (u32)DistrictFlags::AllowAmbEdfCivilianDump;
            if (object.Has("PlayCapstoneUnlockedLines") && object.Get<bool>("PlayCapstoneUnlockedLines"))
                flags |= (u32)DistrictFlags::PlayCapstoneUnlockedLines;
            if (object.Has("DisableMoraleChange") && object.Get<bool>("DisableMoraleChange"))
                flags |= (u32)DistrictFlags::DisableMoraleChange;
            if (object.Has("DisableControlChange") && object.Get<bool>("DisableControlChange"))
                flags |= (u32)DistrictFlags::DisableControlChange;

            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(u32)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<u32>(flags);
            return true;
        }
    );

    //Buffer properties
    ZonePropertyWriters.emplace_back
    (
        "Buffer",
        std::vector<std::string_view>
        {
            "obj_links", "world_anchors", "dynamic_links", "path_road_data", "yellow_triangles", "warning_triangles",
            "yellow_polygon", "warning_polygon"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has(propertyName))
                return false;

            std::optional<std::vector<u8>> buffer = object.Get<BufferHandle>(propertyName).Load();
            if (!buffer)
            {
                LOG_ERROR("Failed to load buffer for property '{}'", propertyName);
                return false;
            }

            writer.Write<u16>(6); //Property type
            writer.Write<u16>(buffer.value().size()); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.WriteSpan<u8>(buffer.value());
            return true;
        }
    );

    ZonePropertyWriters.emplace_back
    (
        "PathRoadStruct",
        std::vector<std::string_view>
        {
            "path_road_struct"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("NumPoints"))
            {
                Log->warn("Failed to write PathRoadStruct property. NumPoints missing.");
                return false;
            }
            if (!object.Has("PointsOffset"))
            {
                Log->warn("Failed to write PathRoadStruct property. PointsOffset missing.");
                return false;
            }
            if (!object.Has("NumConnections"))
            {
                Log->warn("Failed to write PathRoadStruct property. NumConnections missing.");
                return false;
            }
            if (!object.Has("ConnectionsOffset"))
            {
                Log->warn("Failed to write PathRoadStruct property. ConnectionsOffset missing.");
                return false;
            }

            RoadSplineHeader data;
            data.NumPoints = object.Get<i32>("NumPoints");
            data.PointsOffset = object.Get<u32>("PointsOffset");
            data.NumConnections = object.Get<i32>("NumConnections");
            data.ConnectionsOffset = object.Get<u32>("ConnectionsOffset");

            writer.Write<u16>(6); //Property type
            writer.Write<u16>(sizeof(RoadSplineHeader)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<RoadSplineHeader>(data);
            return true;
        }
    );

    ZonePropertyWriters.emplace_back
    (
        "CovernodeData",
        std::vector<std::string_view>
        {
            "covernode_data"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("CovernodeVersion") || !object.Has("Heading") || !object.Has("DefaultAngleLeft") || !object.Has("DefaultAngleRight") || !object.Has("AngleLeft") || !object.Has("AngleRight") || !object.Has("CovernodeFlags"))
                return false;

            CovernodeData data;
            data.Version = object.Get<i32>("CovernodeVersion");
            data.Heading = object.Get<f32>("Heading");
            data.DefaultAngleLeft = object.Get<i8>("DefaultAngleLeft");
            data.DefaultAngleRight = object.Get<i8>("DefaultAngleRight");
            data.AngleLeft = object.Get<i8>("AngleLeft");
            data.AngleRight = object.Get<i8>("AngleRight");
            data.Flags = object.Get<u16>("CovernodeFlags");

            writer.Write<u16>(6); //Property type
            writer.Write<u16>(sizeof(CovernodeData)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<CovernodeData>(data);
            return true;
        }
    );

    ZonePropertyWriters.emplace_back
    (
        "NavpointData",
        std::vector<std::string_view>
        {
            "navpoint_data"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("NavpointVersion") || !object.Has("NavpointType") || !object.Has("DontFollowRoad") || !object.Has("NavpointRadius") || !object.Has("SpeedLimit") || !object.Has("IgnoreLanes") || !object.Has("BranchType"))
                return false;

            NavpointData data;
            data.Version = object.Get<i32>("NavpointVersion");
            data.Type = object.Get<i32>("NavpointType");
            data.DontFollowRoad = object.Get<bool>("DontFollowRoad");
            data.Radius = object.Get<f32>("NavpointRadius");
            data.SpeedLimit = object.Get<f32>("SpeedLimit");
            data.IgnoreLanes = object.Get<bool>("IgnoreLanes");
            const string branchType = object.Get<string>("BranchType");
            if (branchType == "None")
                data.BranchType = NavpointBranchType::None;
            else if (branchType == "Start")
                data.BranchType = NavpointBranchType::Start;
            else if (branchType == "Bridge")
                data.BranchType = NavpointBranchType::Bridge;
            else if (branchType == "End")
                data.BranchType = NavpointBranchType::End;

            writer.Write<u16>(6); //Property type
            writer.Write<u16>(sizeof(NavpointData)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<NavpointData>(data);
            return true;
        }
    );

    //Physics constraints
    ZonePropertyWriters.emplace_back
    (
        "Constraint",
        std::vector<std::string_view>
        {
            "template"
        },
        [](BinaryWriter& writer, ObjectHandle object, std::string_view propertyName) -> bool
        {
            if (!object.Has("Constraint"))
                return false;

            ObjectHandle constraint = object.Get<ObjectHandle>("Constraint");

            ConstraintTemplate constraintData;
            constraintData.Body1Index = constraint.Get<u32>("Body1Index");
            constraintData.Body1Orient = constraint.Get<Mat3>("Body1Orient");
            constraintData.Body1Pos = constraint.Get<Vec3>("Body1Pos");
            constraintData.Body2Index = constraint.Get<u32>("Body2Index");
            constraintData.Body2Orient = constraint.Get<Mat3>("Body2Orient");
            constraintData.Body2Pos = constraint.Get<Vec3>("Body2Pos");
            constraintData.Threshold = constraint.Get<f32>("Threshold");

            const string constraintType = constraint.Get<string>("ConstraintType");
            if (constraintType == "PointConstraint")
            {
                constraintData.Type = ConstraintType::Point;
                constraintData.Data.Point.Type = constraint.Get<i32>("PointConstraintType");
                constraintData.Data.Point.xLimitMin = constraint.Get<f32>("xLimitMin");
                constraintData.Data.Point.xLimitMax = constraint.Get<f32>("xLimitMax");
                constraintData.Data.Point.yLimitMin = constraint.Get<f32>("yLimitMin");
                constraintData.Data.Point.yLimitMax = constraint.Get<f32>("yLimitMax");
                constraintData.Data.Point.zLimitMin = constraint.Get<f32>("zLimitMin");
                constraintData.Data.Point.zLimitMax = constraint.Get<f32>("zLimitMax");
                constraintData.Data.Point.StiffSpringLength = constraint.Get<f32>("StiffSpringLength");
            }
            else if (constraintType == "HingeConstraint")
            {
                constraintData.Type = ConstraintType::Hinge;
                constraintData.Data.Hinge.Limited = constraint.Get<i32>("Limited");
                constraintData.Data.Hinge.LimitMinAngle = constraint.Get<f32>("LimitMinAngle");
                constraintData.Data.Hinge.LimitMaxAngle = constraint.Get<f32>("LimitMaxAngle");
                constraintData.Data.Hinge.LimitFriction = constraint.Get<f32>("LimitFriction");
            }
            else if (constraintType == "PrismaticConstraint")
            {
                constraintData.Type = ConstraintType::Prismatic;
                constraintData.Data.Prismatic.Limited = constraint.Get<i32>("Limited");
                constraintData.Data.Prismatic.LimitMinAngle = constraint.Get<f32>("LimitMinAngle");
                constraintData.Data.Prismatic.LimitMaxAngle = constraint.Get<f32>("LimitMaxAngle");
                constraintData.Data.Prismatic.LimitFriction = constraint.Get<f32>("LimitFriction");
            }
            else if (constraintType == "RagdollConstraint")
            {
                constraintData.Type = ConstraintType::Ragdoll;
                constraintData.Data.Ragdoll.TwistMin = constraint.Get<f32>("TwistMin");
                constraintData.Data.Ragdoll.TwistMax = constraint.Get<f32>("TwistMax");
                constraintData.Data.Ragdoll.ConeMin = constraint.Get<f32>("ConeMin");
                constraintData.Data.Ragdoll.ConeMax = constraint.Get<f32>("ConeMax");
                constraintData.Data.Ragdoll.PlaneMin = constraint.Get<f32>("PlaneMin");
                constraintData.Data.Ragdoll.PlaneMax = constraint.Get<f32>("PlaneMax");
            }
            else if (constraintType == "MotorConstraint")
            {
                constraintData.Type = ConstraintType::Motor;
                constraintData.Data.Motor.AngularSpeed = constraint.Get<f32>("AngularSpeed");
                constraintData.Data.Motor.Gain = constraint.Get<f32>("Gain");
                constraintData.Data.Motor.Axis = constraint.Get<i32>("Axis");
                constraintData.Data.Motor.AxisInBodySpaceX = constraint.Get<f32>("AxisInBodySpaceX");
                constraintData.Data.Motor.AxisInBodySpaceY = constraint.Get<f32>("AxisInBodySpaceY");
                constraintData.Data.Motor.AxisInBodySpaceZ = constraint.Get<f32>("AxisInBodySpaceZ");
            }
            else if (constraintType == "FakeConstraint")
            {
                constraintData.Type = ConstraintType::Fake;
            }

            writer.Write<u16>(5); //Property type
            writer.Write<u16>(sizeof(ConstraintTemplate)); //Property size
            writer.Write<u32>(Hash::HashVolitionCRC(propertyName, 0)); //Property name hash
            writer.Write<ConstraintTemplate>(constraintData);
            return true;
        }
    );
}

bool WriteZoneObjectProperty(BinaryWriter& writer, ObjectHandle object, std::string_view propertyName)
{
	for (PropertyWriter& propType : ZonePropertyWriters)
		for (const std::string_view propName : propType.PropertyNames)
			if (propName == propertyName)
			{
				return propType.Writer(writer, object, propName);
			}

	Log->warn("Failed to write zone object property '{}'. None of the property writers support this property.", propertyName);
	return false;
}
