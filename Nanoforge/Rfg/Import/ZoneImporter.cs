using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using Nanoforge.Misc;
using Nanoforge.Render;
using RFGM.Formats.Math;
using RFGM.Formats.Zones;
using Serilog;

namespace Nanoforge.Rfg.Import;

using ObjectRelationsTuple = (uint parentHandle, uint firstChildHandle, uint siblingHandle);

public class ZoneImporter
{
    private Dictionary<string, (Type type, ZoneObjectReaderFunc readerFunc)> _zoneObjectReaders = new();

    delegate bool ZoneObjectReaderFunc(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives);

    public ZoneImporter()
    {
        _zoneObjectReaders["object"] = (typeof(ZoneObject), ObjectReader);
        _zoneObjectReaders["obj_zone"] = (typeof(ObjZone), ObjZoneReader);
        _zoneObjectReaders["object_bounding_box"] = (typeof(ObjectBoundingBox), ObjBoundingBoxReader);
        _zoneObjectReaders["object_dummy"] = (typeof(ObjectDummy), ObjectDummyReader);
        _zoneObjectReaders["player_start"] = (typeof(PlayerStart), PlayerStartReader);
        _zoneObjectReaders["trigger_region"] = (typeof(TriggerRegion), TriggerRegionReader);
        _zoneObjectReaders["object_mover"] = (typeof(ObjectMover), ObjectMoverReader);
        _zoneObjectReaders["general_mover"] = (typeof(GeneralMover), GeneralMoverReader);
        _zoneObjectReaders["rfg_mover"] = (typeof(RfgMover), RfgMoverReader);
        _zoneObjectReaders["shape_cutter"] = (typeof(ShapeCutter), ShapeCutterReader);
        _zoneObjectReaders["object_effect"] = (typeof(ObjectEffect), ObjectEffectReader);
        _zoneObjectReaders["item"] = (typeof(Item), ItemReader);
        _zoneObjectReaders["weapon"] = (typeof(Weapon), WeaponReader);
        _zoneObjectReaders["ladder"] = (typeof(Ladder), LadderReader);
        _zoneObjectReaders["obj_light"] = (typeof(ObjLight), ObjLightReader);
        _zoneObjectReaders["multi_object_marker"] = (typeof(MultiMarker), MultiObjectMarkerReader);
        _zoneObjectReaders["multi_object_flag"] = (typeof(MultiFlag), MultiObjectFlagReader);
        _zoneObjectReaders["navpoint"] = (typeof(NavPoint), NavPointReader);
        _zoneObjectReaders["cover_node"] = (typeof(CoverNode), CoverNodeReader);
        _zoneObjectReaders["constraint"] = (typeof(RfgConstraint), ConstraintReader);
        _zoneObjectReaders["object_action_node"] = (typeof(ActionNode), ActionNodeReader);
    }

    public Zone? ImportZone(Stream zoneFileStream, Stream persistentZoneFileStream, string zoneFilename)
    {
        try
        {
            //Load zone files
            RfgZoneFile zoneFile = new(zoneFilename);
            if (!zoneFile.Read(zoneFileStream, out string error))
            {
                Log.Error($"Failed to read zone file: {error}");
                return null;
            }

            RfgZoneFile persistentZoneFile = new($"p_{zoneFilename}");
            if (!persistentZoneFile.Read(persistentZoneFileStream, out error))
            {
                Log.Error($"Failed to read persistent zone file: {error}");
            }

            //Convert zone files to editor object
            Zone zone = new Zone();
            zone.Name = zoneFilename;
            zone.District = zoneFile.DistrictName;
            zone.DistrictHash = zoneFile.Header.DistrictHash;
            zone.DistrictFlags = zoneFile.Header.DistrictFlags;
            zone.ActivityLayer = false;
            zone.MissionLayer = false;

            Dictionary<ZoneObject, ObjectRelationsTuple> relatives = new();
            foreach (RfgZoneObject rfgObj in zoneFile.Objects)
            {
                if (!_zoneObjectReaders.ContainsKey(rfgObj.Classname))
                {
                    Log.Error($"Encountered an unsupported zone object class '{rfgObj.Classname}'. Please write an importer for that class and try again.");
                    return null;
                }

                (Type type, ZoneObjectReaderFunc readerFunc) = _zoneObjectReaders[rfgObj.Classname];
                ZoneObject? zoneObject = (ZoneObject?)Activator.CreateInstance(type);
                if (zoneObject is null)
                {
                    Log.Error($"Failed to create ZoneObject instance. Classname: '{rfgObj.Classname}', Type: '{type.Name}'");
                    return null;
                }
                if (!readerFunc(zoneObject, rfgObj, relatives))
                {
                    Log.Error($"Zone object reader for class '{rfgObj.Classname}' failed.");
                    return null;    
                }

                zone.Objects.Add(zoneObject);
            }
            
            //TODO: Add some sanity checks in another function:
            //         - Check for unknown/unsupported properties & object classes
            //         - Check for objects which are their own parent or sibling
            //         - Check for loops in parent/child or sibling linked lists
            //         - Check for parent/sibling/child references to non-existent objects
            //         - Return error enums from RfgZoneObject.GetProperty() functions.
            //               - Useful to know if a property just wasn't found or if it's present but has the wrong type or size. Map import should halt in case of corrupt/poorly formed data.
            //         - Benchmark these on all MP + WC zones. If it takes a significant amount of time make it toggleable
            //TODO: Implement the remaining object class importers used by multiplayer and wrecking crew maps.

            //Mark persistent objects by finding matching objects in the persistent zone file
            foreach (RfgZoneObject rfgObj in persistentZoneFile.Objects)
            {
                foreach (ZoneObject obj in zone.Objects)
                {
                    if (obj.Handle == rfgObj.Handle && obj.Num == rfgObj.Num)
                    {
                        if (!string.Equals(obj.Classname, rfgObj.Classname, StringComparison.OrdinalIgnoreCase))
                        {
                            throw new Exception($"Object type mismatch between primary and persistent zone files for {zone.Name}. Handle: {rfgObj.Handle}, Num: {rfgObj.Num}");
                            return null;
                        }

                        obj.Persistent = true;
                        break;
                    }
                }
            }
            
            //TODO: Next: Fix object relation setup code
            //TODO: Next: (MAYBE) (TEMPORARY) Put some code in RfgZoneObject to check which properties have been read and/or how many times. Report if any haven't been read
            //TODO: Next: Write code to get all MP and WC zone files and run the importer on them - Can ignore missions, activities, and SP / SP DLC ones since we don't have SP zone object types defined
            //TODO: Next: Port the exporter, run all those zone files again and compare input to output. Ignore junk hash table at start of zone files
            
            //Set relative references (ZoneObject references instead of uint handles. much easier to code around)
            foreach (var kv in relatives)
            {
                ZoneObject primaryObj = kv.Key;
                (uint parentHandle, uint firstChildHandle, uint siblingHandle) = kv.Value;
                
                //Find parent
                if (parentHandle != RfgZoneObject.InvalidHandle)
                {
                    foreach (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == parentHandle)
                        {
                            primaryObj.Parent = secondaryObj;
                            break;
                        }
                    }

                    if (primaryObj.Parent == null)
                    {
                        Log.Warning("Couldn't find parent with handle {} for object [{}, {}]", parentHandle, kv.Key.Handle, kv.Key.Num);
                    }
                }

                //Find first child
                if (firstChildHandle != RfgZoneObject.InvalidHandle)
                {
                    ZoneObject? child = null;
                    foreach (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == firstChildHandle)
                        {
                            child = secondaryObj;
                            primaryObj.Children.Add(child);
                            break;
                        }
                    }

                    if (child == null)
                    {
                        Log.Warning("Couldn't find first child with handle {} for object [{}, {}]", firstChildHandle, kv.Key.Handle, kv.Key.Num);
                    }

                    //Fill children list
                    while (child != null)
                    {
                        if (!relatives.TryGetValue(child, out ObjectRelationsTuple relative))
                            break;

                        (_, _, uint childSiblingHandle) = relative;
                        if (childSiblingHandle == RfgZoneObject.InvalidHandle)
                            break;

                        foreach (ZoneObject secondaryObj in zone.Objects)
                        {
                            if (secondaryObj.Handle == childSiblingHandle)
                            {
                                primaryObj.Children.Add(secondaryObj);
                                child = secondaryObj;
                                break;
                            }
                        }
                    }
                }
                
                //Check if sibling exists
                if (siblingHandle != RfgZoneObject.InvalidHandle)
                {
                    ZoneObject? sibling = null;
                    foreach (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == siblingHandle)
                        {
                            sibling = secondaryObj;
                            break;
                        }
                    }

                    if (sibling == null)
                    {
                        Log.Warning("Couldn't find sibling with handle {} for object [{}, {}] in {}", siblingHandle, kv.Key.Handle, kv.Key.Num, zone.Name);
                    }
                }
            }
            
            return zone;
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error importing zone file {zoneFilename}");
            return null;
        }
    }

    //object - base class inherited by all other zone object classes
    private bool ObjectReader(ZoneObject obj, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        obj.Classname = rfgObj.Classname;
        obj.Handle = rfgObj.Handle;
        obj.Num = rfgObj.Num;
        obj.Flags = (ZoneObject.ZoneObjectFlags)rfgObj.Flags;
        obj.BBox = rfgObj.BBox;
        relatives[obj] = (rfgObj.Parent, rfgObj.Child, rfgObj.Sibling); //Store relations in temporary dictionary. They get applied to the related properties once all objects are loaded

        //Position & orient
        if (rfgObj.GetVec3("just_pos", out Vector3 pos))
        {
            obj.Position = pos;
            obj.Orient.Enabled = false;
        }
        else if (rfgObj.GetPositionOrient("op", out PositionOrient positionOrient)) //Only check op if just_pos is missing
        {
            obj.Position = positionOrient.Position;
            obj.Orient.SetAndEnable(positionOrient.Orient.ToMatrix4x4());

            //Grab the euler angles when the orientation is first set. We track these to generate rotation delta matrices when time pitch/yaw/roll changes.
            //This avoids issues with gimbal lock and indeterminate angles that we'd get if we tried to extract the euler angles from the matrix every time we edited it.
            //obj.Orient.Value.GetEulerAngles(ref obj.Pitch, ref obj.Yaw, ref obj.Roll); //TODO: Re-enable. Disabled for now since the original rotation controls had issues
            obj.Pitch = MathHelpers.ToDegrees(obj.Pitch);
            obj.Yaw = MathHelpers.ToDegrees(obj.Yaw);
            obj.Roll = MathHelpers.ToDegrees(obj.Roll);
        }
        else
        {
            //Position is missing. Set to the center of the bounding box. Which all objects have as part of the zone format
            obj.Position = obj.BBox.Center();
            obj.Orient.Enabled = false;
        }

        //Optional display name. No vanilla objects have this
        if (rfgObj.GetString("display_name", out string displayName))
        {
            obj.RfgDisplayName.SetAndEnable(displayName);
            obj.Name = displayName;
        }

        return true;
    }

    //obj_zone : object
    private bool ObjZoneReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjZone obj = (ObjZone)objBase;
        if (!ObjectReader(obj, rfgObj, relatives)) //Read inherited properties
            return false;

        if (rfgObj.GetString("ambient_spawn", out string ambientSpawn))
        {
            obj.AmbientSpawn.SetAndEnable(ambientSpawn);
        }

        if (rfgObj.GetString("spawn_resource", out string spawnResource))
        {
            obj.SpawnResource.SetAndEnable(spawnResource);
        }

        if (rfgObj.GetString("terrain_file_name", out string terrainFileName))
        {
            obj.TerrainFileName = terrainFileName;
        }

        if (rfgObj.GetFloat("wind_min_speed", out float windMinSpeed))
        {
            obj.WindMinSpeed.SetAndEnable(windMinSpeed);
        }

        if (rfgObj.GetFloat("wind_max_speed", out float windMaxSpeed))
        {
            obj.WindMaxSpeed.SetAndEnable(windMaxSpeed);
        }

        return true;
    }

    //object_bounding_box : object
    private bool ObjBoundingBoxReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjectBoundingBox obj = (ObjectBoundingBox)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetBBox("bb", out BoundingBox bbox))
        {
            obj.LocalBBox = bbox;
        }

        if (rfgObj.GetString("bounding_box_type", out string boundingBoxType))
        {
            if (DataHelper.FromRfgName<ObjectBoundingBox.BoundingBoxType>(boundingBoxType, out ObjectBoundingBox.BoundingBoxType bbTypeEnum))
            {
                obj.BBType.SetAndEnable(bbTypeEnum);
            }
            else
            {
                Log.Error($"Unknown bounding box type: {boundingBoxType}");
                return false;
            }
        }
        else
        {
            obj.BBType.Enabled = false;
        }

        return true;
    }

    //object_dummy : object
    private bool ObjectDummyReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjectDummy obj = (ObjectDummy)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("dummy_type", out string dummyType))
        {
            if (DataHelper.FromRfgName<ObjectDummy.DummyTypeEnum>(dummyType, out var dummyTypeEnum))
            {
                obj.DummyType = dummyTypeEnum;
            }
            else
            {
                Log.Error($"Unknown DummyTypeEnum: {dummyType}");
            }

            obj.Name = dummyType;
        }

        return true;
    }

    //player_start : object
    private bool PlayerStartReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        PlayerStart obj = (PlayerStart)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetBool("indoor", out bool indoor))
        {
            obj.Indoor = indoor;
        }

        if (rfgObj.GetString("mp_team", out string mpTeam))
        {
            if (DataHelper.FromRfgName<RfgTeam>(mpTeam, out RfgTeam teamEnum))
            {
                obj.MpTeam.SetAndEnable(teamEnum);
            }
            else
            {
                Log.Error($"Unknown value for RfgTeam enum: {mpTeam}");
                return false;
            }
        }
        else
        {
            obj.MpTeam.Value = RfgTeam.None;
            obj.MpTeam.Enabled = false;
        }

        if (rfgObj.GetBool("initial_spawn", out bool initialSpawn))
        {
            obj.InitialSpawn = initialSpawn;
        }

        if (rfgObj.GetBool("respawn", out bool respawn))
        {
            obj.Respawn = respawn;
        }

        if (rfgObj.GetBool("checkpoint_respawn", out bool checkpointRespawn))
        {
            obj.CheckpointRespawn = checkpointRespawn;
        }

        if (rfgObj.GetBool("activity_respawn", out bool activityRespawn))
        {
            obj.ActivityRespawn = activityRespawn;
        }

        if (rfgObj.GetString("mission_info", out string missionInfo))
        {
            obj.MissionInfo.SetAndEnable(missionInfo);
            obj.Name = missionInfo;
        }

        //Use mp team as name instead of mission_info if no mission is set
        if (obj.Name == "none")
        {
            if (obj.MpTeam.Value == RfgTeam.None)
            {
                obj.Name = "player start";
            }
            else
            {
                obj.Name = $"{obj.MpTeam.Value} player start";
            }
        }

        return true;
    }

    //trigger_region : object
    private bool TriggerRegionReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        TriggerRegion obj = (TriggerRegion)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("trigger_shape", out string triggerShapeString))
        {
            if (DataHelper.FromRfgName<TriggerRegion.Shape>(triggerShapeString, out TriggerRegion.Shape triggerShape))
            {
                obj.TriggerShape.SetAndEnable(triggerShape);
            }
            else
            {
                Log.Error($"Error deserializing TriggerRegion.Shape enum from string '{triggerShapeString}'");
                return false;
            }
        }
        else
        {
            obj.TriggerShape.Enabled = false;
        }

        if (rfgObj.GetBBox("bb", out BoundingBox bbox))
        {
            obj.LocalBBox = bbox;
        }

        if (rfgObj.GetFloat("outer_radius", out float outerRadius))
        {
            obj.OuterRadius = outerRadius;
        }

        if (rfgObj.GetBool("enabled", out bool enabled))
        {
            obj.Enabled = enabled;
        }

        if (rfgObj.GetString("region_type", out string regionTypeString))
        {
            if (DataHelper.FromRfgName<TriggerRegion.RegionTypeEnum>(regionTypeString, out TriggerRegion.RegionTypeEnum regionType))
            {
                obj.RegionType.SetAndEnable(regionType);
            }
            else
            {
                Log.Error($"Error deserializing TriggerRegion.RegionTypeEnum from string '{regionTypeString}'");
                return false;
            }
        }
        else
        {
            obj.RegionType.Enabled = false;
        }

        if (rfgObj.GetString("region_kill_type", out string killTypeString))
        {
            if (DataHelper.FromRfgName<TriggerRegion.KillTypeEnum>(killTypeString, out TriggerRegion.KillTypeEnum killType))
            {
                obj.KillType.SetAndEnable(killType);
            }
            else
            {
                Log.Error($"Error deserializing TriggerRegion.KillTypeEnum from string '{killTypeString}'");
                return false;
            }

            obj.Name = killTypeString;
        }
        else
        {
            obj.KillType.Enabled = false;
        }

        if (rfgObj.GetString("trigger_flags", out string triggerFlagsString))
        {
            if (DataHelper.FromRfgFlagsString<TriggerRegion.TriggerRegionFlags>(triggerFlagsString, out TriggerRegion.TriggerRegionFlags triggerFlags))
            {
                obj.TriggerFlags.SetAndEnable(triggerFlags);
            }
            else
            {
                Log.Error($"Failed to convert flags string to TriggerRegion.TriggerRegionFlags from string '{triggerFlagsString}'");
                return false;
            }
        }
        else
        {
            obj.TriggerFlags.Enabled = false;
        }

        return true;
    }

    //object_mover : object
    private bool ObjectMoverReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjectMover obj = (ObjectMover)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("building_type", out string buildingTypeString))
        {
            if (DataHelper.FromRfgFlagsString<ObjectMover.BuildingTypeFlagsEnum>(buildingTypeString, out ObjectMover.BuildingTypeFlagsEnum buildingType))
            {
                obj.BuildingType = buildingType;
            }
            else
            {
                Log.Error($"Failed to convert flags string to ObjectMover.BuildingTypeFlagsEnum from string '{buildingTypeString}'");
                return false;
            }
        }
        else
        {
            obj.BuildingType = ObjectMover.BuildingTypeFlagsEnum.None;
        }

        if (rfgObj.GetUInt32("dest_checksum", out uint destChecksum))
        {
            obj.DestructionChecksum = destChecksum;
        }

        if (rfgObj.GetString("gameplay_props", out string gameplayProps))
        {
            obj.GameplayProps.SetAndEnable(gameplayProps);
        }
        else
        {
            obj.GameplayProps.Value = "";
            obj.GameplayProps.Enabled = false;
        }

        //if (rfgObj.GetU32("flags") case .Ok(u32 val)) //Disabled. See ObjectMover.InternalFlags comments for reason
        //    obj.InternalFlags = val;

        if (rfgObj.GetString("chunk_flags", out string chunkFlagsString))
        {
            if (DataHelper.FromRfgFlagsString<ObjectMover.ChunkFlagsEnum>(chunkFlagsString, out ObjectMover.ChunkFlagsEnum chunkFlags))
            {
                if (chunkFlags != ObjectMover.ChunkFlagsEnum.None)
                {
                    obj.ChunkFlags.Value = chunkFlags;
                    obj.ChunkFlags.Enabled = true;
                }
            }
            else
            {
                Log.Error($"Failed to convert flags string to ObjectMover.ChunkFlagsEnum from string '{chunkFlagsString}'");
                return false;
            }
        }
        else
        {
            obj.ChunkFlags.Enabled = false;
        }

        if (rfgObj.GetBool("dynamic_object", out bool dynamicObject))
        {
            obj.Dynamic.SetAndEnable(dynamicObject);
        }

        if (rfgObj.GetUInt32("chunk_uid", out uint chunkUid))
        {
            obj.ChunkUID.SetAndEnable(chunkUid);
        }

        if (rfgObj.GetString("props", out string props))
        {
            obj.Props.SetAndEnable(props);
        }

        if (rfgObj.GetString("chunk_name", out string chunkName))
        {
            obj.ChunkName.SetAndEnable(chunkName);
            obj.Name = chunkName;
        }

        if (rfgObj.GetUInt32("uid", out uint uid))
        {
            obj.DestroyableUID = uid;
        }

        if (rfgObj.GetUInt32("shape_uid", out uint shapeUid))
        {
            obj.ShapeUID.SetAndEnable(shapeUid);
        }

        if (rfgObj.GetString("team", out string teamString))
        {
            if (DataHelper.FromRfgName<RfgTeam>(teamString, out RfgTeam team))
            {
                obj.Team.SetAndEnable(team);
            }
            else
            {
                Log.Error($"Failed to convert string '{chunkFlagsString}' to RfgTeam enum");
                return false;
            }
        }
        else
        {
            obj.Team.Value = RfgTeam.None;
            obj.Team.Enabled = false;
        }

        if (rfgObj.GetFloat("control", out float control))
        {
            obj.Control.SetAndEnable(control);
        }

        return true;
    }

    //general_mover : object_mover
    private bool GeneralMoverReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        GeneralMover obj = (GeneralMover)objBase;
        if (!ObjectMoverReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetUInt32("gm_flags", out uint gmFlags))
        {
            obj.GmFlags = gmFlags;
        }

        if (rfgObj.GetUInt32("original_object", out uint originalObject))
        {
            obj.OriginalObject.SetAndEnable(originalObject);
        }

        if (rfgObj.GetUInt32("ctype", out uint ctype))
        {
            obj.CollisionType.SetAndEnable(ctype);
        }

        if (rfgObj.GetUInt32("idx", out uint idx))
        {
            obj.Idx.SetAndEnable(idx);
        }

        if (rfgObj.GetUInt32("mtype", out uint mtype))
        {
            obj.MoveType.SetAndEnable(mtype);
        }

        if (rfgObj.GetUInt32("destruct_uid", out uint destructionUid))
        {
            obj.DestructionUID.SetAndEnable(destructionUid);
        }

        if (rfgObj.GetInt32("hitpoints", out int hitpoints))
        {
            obj.Hitpoints.SetAndEnable(hitpoints);
        }

        if (rfgObj.GetInt32("dislodge_hitpoints", out int dislodgeHitpoints))
        {
            obj.DislodgeHitpoints.SetAndEnable(dislodgeHitpoints);
        }

        return true;
    }

    //rfg_mover : object_mover
    private bool RfgMoverReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        RfgMover obj = (RfgMover)objBase;
        if (!ObjectMoverReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetUInt32("mtype", out uint mtype))
        {
            obj.MoveType = (RfgMover.MoveTypeEnum)mtype;
        }

        if (rfgObj.GetFloat("damage_percent", out float damagePercent))
        {
            obj.DamagePercent.SetAndEnable(damagePercent);
        }

        if (rfgObj.GetFloat("game_destroyed_pct", out float gameDestroyedPercentage))
        {
            obj.GameDestroyedPercentage.SetAndEnable(gameDestroyedPercentage);
        }

        //Note: Didn't implement readers for several disabled properties. See RfgMover definition on github

        if (rfgObj.GetBuffer("world_anchors", out byte[] worldAnchors))
        {
            obj.WorldAnchors.SetAndEnable(worldAnchors);
        }

        if (rfgObj.GetBuffer("dynamic_links", out byte[] dynamicLinks))
        {
            obj.DynamicLinks.SetAndEnable(dynamicLinks);
        }

        return true;
    }

    //shape_cutter : object
    private bool ShapeCutterReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ShapeCutter obj = (ShapeCutter)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetInt16("flags", out short flags))
        {
            obj.ShapeCutterFlags.SetAndEnable(flags);
        }

        if (rfgObj.GetFloat("outer_radius", out float outerRadius))
        {
            obj.OuterRadius.SetAndEnable(outerRadius);
        }

        if (rfgObj.GetUInt32("source", out uint source))
        {
            obj.Source.SetAndEnable(source);
        }

        if (rfgObj.GetUInt32("owner", out uint owner))
        {
            obj.Owner.SetAndEnable(owner);
        }

        if (rfgObj.GetUInt8("exp_info", out byte explosionId))
        {
            obj.ExplosionId.SetAndEnable(explosionId);
        }

        return true;
    }

    //object_effect : object
    private bool ObjectEffectReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjectEffect obj = (ObjectEffect)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("effect_type", out string effectType))
        {
            obj.EffectType.SetAndEnable(effectType);
        }

        if (rfgObj.GetString("sound_alr", out string soundAlr))
        {
            obj.SoundAlr.SetAndEnable(soundAlr);
        }

        if (rfgObj.GetString("sound", out string sound))
        {
            obj.Sound.SetAndEnable(sound);
        }

        if (rfgObj.GetString("visual", out string visual))
        {
            obj.Visual.SetAndEnable(visual);
        }

        if (rfgObj.GetBool("looping", out bool looping))
        {
            obj.Looping.SetAndEnable(looping);
        }

        return true;
    }

    //item : object
    private bool ItemReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        Item obj = (Item)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("item_type", out string itemType))
        {
            obj.ItemType.SetAndEnable(itemType);
            obj.Name = itemType;
        }

        //The game looks for two possible properties "respawn" or "respawns" that both get put into the same field in game.
        if (rfgObj.GetBool("respawn", out bool respawn))
        {
            obj.Respawns.SetAndEnable(respawn);
        }
        else if (rfgObj.GetBool("respawns", out bool respawns))
        {
            obj.Respawns.SetAndEnable(respawns);
        }

        if (rfgObj.GetBool("preplaced", out bool preplaced))
        {
            obj.Preplaced.SetAndEnable(preplaced);
        }

        return true;
    }

    //weapon : item
    private bool WeaponReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        Weapon obj = (Weapon)objBase;
        if (!ItemReader(obj, rfgObj, relatives))
            return false;

        if (!rfgObj.GetString("item_type", out _))
        {
            if (rfgObj.GetString("weapon_type", out string weaponType)) //Only loaded if the object doesn't have item_type
            {
                obj.WeaponType = weaponType;
                obj.Name = weaponType;
            }
        }

        return true;
    }

    //ladder : object
    private bool LadderReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        Ladder obj = (Ladder)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetInt32("ladder_rungs", out int ladderRungs))
        {
            obj.LadderRungs = ladderRungs;
        }

        if (rfgObj.GetBool("ladder_enabled", out bool ladderEnabled))
        {
            obj.LadderEnabled.SetAndEnable(ladderEnabled);
        }

        return true;
    }

    //obj_light : object
    private bool ObjLightReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ObjLight obj = (ObjLight)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetFloat("atten_start", out float attenStart))
        {
            obj.AttenuationStart.SetAndEnable(attenStart);
        }

        if (rfgObj.GetFloat("atten_end", out float attenEnd))
        {
            obj.AttenuationEnd.SetAndEnable(attenEnd);
        }
        else if (rfgObj.GetFloat("atten_range", out float attenRange))
        {
            if (!obj.AttenuationStart.Enabled)
                obj.AttenuationStart.Value = 1.0f;

            obj.AttenuationRange.Value = attenRange;
            obj.AttenuationRange.Enabled = true;
            obj.AttenuationEnd.Value = obj.AttenuationStart.Value + obj.AttenuationRange.Value;
            obj.AttenuationEnd.Enabled = true;
        }

        if (rfgObj.GetString("light_flags", out string lightFlagsString))
        {
            if (DataHelper.FromRfgFlagsString<ObjLight.ObjLightFlags>(lightFlagsString, out ObjLight.ObjLightFlags lightFlags))
            {
                obj.LightFlags.SetAndEnable(lightFlags);
            }
            else
            {
                Log.Error($"Failed to convert flags string '{lightFlagsString}' to ObjLight.ObjLightFlags flag enum");
                return false;
            }
        }
        else
        {
            obj.LightFlags.Enabled = false;
        }

        if (rfgObj.GetString("type_enum", out string lightTypeString))
        {
            if (DataHelper.FromRfgFlagsString<ObjLight.LightTypeEnum>(lightTypeString, out ObjLight.LightTypeEnum lightType))
            {
                obj.LightType.SetAndEnable(lightType);
            }
            else
            {
                Log.Error($"Failed to convert flags string '{lightTypeString}' to ObjLight.LightTypeEnum enum");
                return false;
            }
        }
        else
        {
            obj.LightType.Enabled = false;
        }

        if (rfgObj.GetVec3("clr_orig", out Vector3 clrOrig))
        {
            obj.Color = clrOrig;
        }

        if (rfgObj.GetFloat("hotspot_size", out float hotspotSize))
        {
            obj.HotspotSize.SetAndEnable(hotspotSize);
        }

        if (rfgObj.GetFloat("hotspot_falloff_size", out float hotspotFalloffSize))
        {
            obj.HotspotFalloffSize.SetAndEnable(hotspotFalloffSize);
        }

        if (rfgObj.GetVec3("min_clip", out Vector3 minClip))
        {
            obj.MinClip = minClip;
        }

        if (rfgObj.GetVec3("max_clip", out Vector3 maxClip))
        {
            obj.MaxClip = maxClip;
        }

        //if (rfgObj.GetInt32("clip_mesh", out int val))
        //    obj.ClipMesh = val;

        return true;
    }

    //multi_object_marker : object
    private bool MultiObjectMarkerReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        MultiMarker obj = (MultiMarker)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetBBox("bb", out BoundingBox bbox))
        {
            obj.LocalBBox = bbox;
        }

        if (rfgObj.GetString("marker_type", out string markerTypeString))
        {
            if (DataHelper.FromRfgName<MultiMarker.MultiMarkerType>(markerTypeString, out MultiMarker.MultiMarkerType markerType))
            {
                obj.MarkerType = markerType;
            }
            else
            {
                Log.Error($"Error converting marker type '{markerTypeString}' to MultiMarker.MultiMarkerType enum");
                return false;
            }

            obj.Name = markerTypeString;
        }

        if (rfgObj.GetString("mp_team", out string mpTeamString))
        {
            if (DataHelper.FromRfgName<RfgTeam>(mpTeamString, out RfgTeam mpTeam))
            {
                obj.MpTeam = mpTeam;
            }
            else
            {
                Log.Error($"Error converting mp_team '{mpTeamString}' to RfgTeam enum");
            }
        }

        if (rfgObj.GetInt32("priority", out int priority))
        {
            obj.Priority = priority;
        }

        if (rfgObj.GetString("backpack_type", out string backpackType))
        {
            obj.BackpackType.SetAndEnable(backpackType);
        }

        if (rfgObj.GetInt32("num_backpacks", out int numBackpacks))
        {
            obj.NumBackpacks.SetAndEnable(numBackpacks);
        }

        if (rfgObj.GetBool("random_backpacks", out bool randomBackpacks))
        {
            obj.RandomBackpacks.SetAndEnable(randomBackpacks);
        }

        if (rfgObj.GetInt32("group", out int group))
        {
            obj.Group.SetAndEnable(group);
        }

        return true;
    }

    //multi_object_flag : item
    private bool MultiObjectFlagReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        MultiFlag obj = (MultiFlag)objBase;
        if (!ItemReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("mp_team", out string mpTeamString))
        {
            if (DataHelper.FromRfgName<RfgTeam>(mpTeamString, out RfgTeam mpTeam))
            {
                obj.MpTeam = mpTeam;
            }
            else
            {
                Log.Error($"Error converting mp_team '{mpTeamString}' to RfgTeam enum");
            }
        }

        return true;
    }

    //navpoint : object
    private bool NavPointReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        NavPoint obj = (NavPoint)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetBuffer("navpoint_data", out byte[] navpointData))
        {
            obj.NavpointData.SetAndEnable(navpointData);
        }
        else
        {
            if (rfgObj.GetUInt8("nav_type", out byte navType))
            {
                obj.NavpointType.SetAndEnable((NavPoint.NavpointTypeEnum)navType);
            }
            else if (rfgObj.GetString("navpoint_type", out string navpointTypeString))
            {
                if (DataHelper.FromRfgName<NavPoint.NavpointTypeEnum>(navpointTypeString, out NavPoint.NavpointTypeEnum navpointType))
                {
                    obj.NavpointType.SetAndEnable(navpointType);
                }
                else
                {
                    Log.Error($"Error converting navpoint_type type '{navpointTypeString}' to NavpointTypeEnum enum");
                    return false;
                }
            }

            if (rfgObj.GetFloat("outer_radius", out float outerRadius))
            {
                obj.OuterRadius.SetAndEnable(outerRadius);
            }

            if (rfgObj.GetFloat("speed_limit", out float speedLimit))
            {
                obj.SpeedLimit.SetAndEnable(speedLimit);
            }

            if (obj.NavpointType.Enabled && obj.NavpointType.Value != NavPoint.NavpointTypeEnum.Patrol)
            {
                if (rfgObj.GetBool("dont_follow_road", out bool dontFollowRoad))
                {
                    obj.DontFollowRoad.SetAndEnable(dontFollowRoad);
                }

                if (rfgObj.GetBool("ignore_lanes", out bool ignoreLanes))
                {
                    obj.IgnoreLanes.SetAndEnable(ignoreLanes);
                }
            }
        }

        if (rfgObj.GetBuffer("obj_links", out byte[] objLinks))
        {
            obj.Links.SetAndEnable(objLinks);
        }
        else if (rfgObj.GetBuffer("navlinks", out byte[] navLinks))
        {
            obj.NavLinks.SetAndEnable(navLinks);
        }

        if (obj.NavpointType.Enabled && obj.NavpointType.Value == NavPoint.NavpointTypeEnum.RoadCheckpoint)
        {
            if (rfgObj.GetMat3("nav_orient", out Matrix3x3 navOrient))
            {
                obj.NavOrient.SetAndEnable(navOrient.ToMatrix4x4());
            }
        }

        return true;
    }

    //cover_node : object
    private bool CoverNodeReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        CoverNode obj = (CoverNode)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;
        
        if (rfgObj.GetBuffer("covernode_data", out byte[] covernodeData))
        {
            obj.CovernodeData.SetAndEnable(covernodeData);
        }
        else if (rfgObj.GetBuffer("cnp", out byte[] cnpBytes))
        {
            obj.OldCovernodeData.SetAndEnable(cnpBytes);
        }

        if (rfgObj.GetFloat("def_angle_left", out float defAngleLeft))
        {
            obj.DefensiveAngleLeft.SetAndEnable(defAngleLeft);
        }

        if (rfgObj.GetFloat("def_angle_right", out float defAngleRight))
        {
            obj.DefensiveAngleRight.SetAndEnable(defAngleRight);
        }

        if (rfgObj.GetFloat("off_angle_left", out float offAngleLeft))
        {
            obj.OffensiveAngleLeft.SetAndEnable(offAngleLeft);
        }

        if (rfgObj.GetFloat("angle_left", out float angleLeft))
        {
            obj.AngleLeft.SetAndEnable(angleLeft);
        }

        if (rfgObj.GetFloat("off_angle_right", out float offAngleRight))
        {
            obj.OffensiveAngleRight.SetAndEnable(offAngleRight);
        }

        if (rfgObj.GetFloat("angle_right", out float angleRight))
        {
            obj.AngleRight.SetAndEnable(angleRight);
        }

        if (rfgObj.GetUInt16("binary_flags", out ushort binaryFlags))
        {
            obj.CoverNodeFlags.SetAndEnable((CoverNode.CoverNodeFlagsEnum)binaryFlags);
        }

        if (rfgObj.GetString("stance", out string stance))
        {
            obj.Stance.SetAndEnable(stance);
        }

        if (rfgObj.GetString("firing_flags", out string firingFlags))
        {
            obj.FiringFlags.SetAndEnable(firingFlags);
        }

        return true;
    }

    //constraint : object
    private bool ConstraintReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        RfgConstraint obj = (RfgConstraint)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetConstraintTemplate("template", out byte[] bytes))
        {
            if (bytes.Length != 156)
            {
                Log.Error("Constraint template data isn't the right size for rfg object {0}, {1}. It should be 156 bytes. Its actual size is {2} bytes", obj.Handle, obj.Num, bytes.Length);
                return false;
            }

            obj.Template.SetAndEnable(bytes);
        }

        if (rfgObj.GetUInt32("host_handle", out uint hostHandle))
        {
            obj.HostHandle.SetAndEnable(hostHandle);
        }

        if (rfgObj.GetUInt32("child_handle", out uint childHandle))
        {
            obj.ChildHandle.SetAndEnable(childHandle);
        }

        if (rfgObj.GetUInt32("host_index", out uint hostIndex))
        {
            obj.HostIndex.SetAndEnable(hostIndex);
        }

        if (rfgObj.GetUInt32("child_index", out uint childIndex))
        {
            obj.ChildIndex.SetAndEnable(childIndex);
        }

        if (rfgObj.GetUInt32("host_hk_alt_index", out uint hostHavokAltIndex))
        {
            obj.HostHavokAltIndex.SetAndEnable(hostHavokAltIndex);
        }

        if (rfgObj.GetUInt32("child_hk_alt_index", out uint childHavokAltIndex))
        {
            obj.ChildHavokAltIndex.SetAndEnable(childHavokAltIndex);
        }

        return true;
    }

    //object_action_node : object
    private bool ActionNodeReader(ZoneObject objBase, RfgZoneObject rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives)
    {
        ActionNode obj = (ActionNode)objBase;
        if (!ObjectReader(obj, rfgObj, relatives))
            return false;

        if (rfgObj.GetString("animation_type", out string animationType))
        {
            obj.ActionNodeType.SetAndEnable(animationType);
        }

        if (rfgObj.GetBool("high_priority", out bool highPriority))
        {
            obj.HighPriority.SetAndEnable(highPriority);
        }

        if (rfgObj.GetBool("infinite_duration", out bool infiniteDuration))
        {
            obj.InfiniteDuration.SetAndEnable(infiniteDuration);
        }

        if (rfgObj.GetInt32("disabled", out int disabled))
        {
            obj.DisabledBy.SetAndEnable(disabled);
        }

        if (rfgObj.GetBool("run_to", out bool runTo))
        {
            obj.RunTo.SetAndEnable(runTo);
        }

        if (rfgObj.GetFloat("outer_radius", out float outerRadius))
        {
            obj.OuterRadius.SetAndEnable(outerRadius);
        }

        if (rfgObj.GetBuffer("obj_links", out byte[] objLinks))
        {
            obj.ObjLinks.SetAndEnable(objLinks);
        }
        else if (rfgObj.GetBuffer("links", out byte[] links))
        {
            obj.Links.SetAndEnable(links);
        }

        return true;
    }
}