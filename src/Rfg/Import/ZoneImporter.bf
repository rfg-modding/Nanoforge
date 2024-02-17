using RfgTools.Formats.Zones;
using Nanoforge.App.Project;
using System.Diagnostics;
using System.Collections;
using Nanoforge.Misc;
using Common.Math;
using Common;
using System;
using RfgTools.Types;
using Nanoforge.App;

namespace Nanoforge.Rfg.Import
{
	public class ZoneImporter
	{
        private append Dictionary<StringView, ZoneObjectReaderFunc> _zoneObjectReaders;

        //Typed object readers. preExisting can be null to indicate that the reader should create a new instance.
        //When non null it's being used to read inherited properties. E.g. ItemReader calls ObjectReader since item inherits object and object properties.
        typealias ZoneObjectReaderFunc = function Result<ZoneObject, StringView>(ZoneImporter this, ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes);
        typealias ObjectRelationsTuple = (u32 parentHandle, u32 firstChildHandle, u32 siblingHandle);

        public this()
        {
            //Initialize zone object readers
            _zoneObjectReaders["object"] = => ObjectReader;
            _zoneObjectReaders["obj_zone"] = => ObjZoneReader;
            _zoneObjectReaders["object_bounding_box"] = => ObjBoundingBoxReader;
            _zoneObjectReaders["object_dummy"] = => ObjectDummyReader;
            _zoneObjectReaders["player_start"] = => PlayerStartReader;
            _zoneObjectReaders["trigger_region"] = => TriggerRegionReader;
            _zoneObjectReaders["object_mover"] = => ObjectMoverReader;
            _zoneObjectReaders["general_mover"] = => GeneralMoverReader;
            _zoneObjectReaders["rfg_mover"] = => RfgMoverReader;
            _zoneObjectReaders["shape_cutter"] = => ShapeCutterReader;
            _zoneObjectReaders["object_effect"] = => ObjectEffectReader;
            _zoneObjectReaders["item"] = => ItemReader;
            _zoneObjectReaders["weapon"] = => WeaponReader;
            _zoneObjectReaders["ladder"] = => LadderReader;
            _zoneObjectReaders["obj_light"] = => ObjLightReader;
            _zoneObjectReaders["multi_object_marker"] = => MultiObjectMarkerReader;
            _zoneObjectReaders["multi_object_flag"] = => MultiObjectFlagReader;
            _zoneObjectReaders["navpoint"] = => NavPointReader;
            _zoneObjectReaders["cover_node"] = => CoverNodeReader;
            _zoneObjectReaders["constraint"] = => ConstraintReader;
            _zoneObjectReaders["object_action_node"] = => ActionNodeReader;
        }

        public Result<Zone, StringView> ImportZone(Span<u8> fileBytes, Span<u8> persistentFileBytes, StringView zoneFilename, DiffUtil changes)
        {
            //Parse zone files
            ZoneFile36 zoneFile = scope .();
            if (zoneFile.Load(fileBytes, useInPlace: true) case .Err(let err))
            {
                Logger.Error(scope $"Failed to parse zone file '{zoneFilename}'. {err}");
                return .Err("Failed to parse zone file");
            }
            ZoneFile36 persistentZoneFile = scope .();
            if (persistentZoneFile.Load(persistentFileBytes, useInPlace: true) case .Err(let err)) //Note: useInPlace is true so the Span<u8> must stay alive as long as the ZoneFile is alive
            {
                Logger.Error(scope $"Failed to parse persistent zone file 'p_{zoneFilename}'. {err}");
                return .Err("Failed to parse persistent zone file");
            }

            //Convert zone file to an editor object
            Zone zone = changes.CreateObject<Zone>();
            zone.Name.Set(zoneFilename);
            zone.District.Set(zoneFile.DistrictName);
            zone.DistrictHash = zoneFile.Header.DistrictHash;
            zone.DistrictFlags = zoneFile.Header.DistrictFlags;
            zone.ActivityLayer = false;
            zone.MissionLayer = false;

            Dictionary<ZoneObject, ObjectRelationsTuple> relatives = scope .();
            for (RfgZoneObject* rfgObj in zoneFile.Objects)
            {
                if (_zoneObjectReaders.ContainsKey(rfgObj.Classname))
                {
                    switch (_zoneObjectReaders[rfgObj.Classname](this, null, rfgObj, relatives, changes))
                    {
                        case .Ok(let obj):
                            zone.Objects.Add(obj);
                        case .Err(let err):
                            Logger.Error(scope $"Zone object reader for class '{rfgObj.Classname}' failed. {err}");
                            return .Err("Zone object read error.");
                    }
                }
                else
                {
                    Logger.Error(scope $"Encountered an unsupported zone object class '{rfgObj.Classname}'. Please write an importer for that class and try again.");
                    return .Err("Encountered an unsupported zone object class.");
                }
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
            for (RfgZoneObject* rfgObj in persistentZoneFile.Objects)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    if (obj.Handle == rfgObj.Handle && obj.Num == rfgObj.Num)
                    {
#if DEBUG
                        if (!StringView.Equals(obj.Classname, rfgObj.Classname, true))
                        {
                            Logger.Error(scope $"Object type mismatch between primary and persistent zone files for {zone.Name}");
                            return .Err("Object type mismatch between primary and persistent zone files");
                        }
#endif

                        obj.Persistent = true;
                        break;
                    }
                }
            }

            //Set relative references
            for (var kv in relatives)
            {
                ZoneObject primaryObj = kv.key;
                (u32 parentHandle, u32 firstChildHandle, u32 siblingHandle) = kv.value;

                //Find parent
                if (parentHandle != RfgZoneObject.InvalidHandle)
                {
                    for (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == parentHandle)
                        {
                            primaryObj.Parent = secondaryObj;
                            break;
                        }
                    }

                    if (primaryObj.Parent == null)
                    {
                        Logger.Warning("Couldn't find parent with handle {} for object [{}, {}]", parentHandle, kv.key.Handle, kv.key.Num);
                    }
                }

                //Find first child
                if (firstChildHandle != RfgZoneObject.InvalidHandle)
                {
                    ZoneObject child = null;
                    for (ZoneObject secondaryObj in zone.Objects)
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
                        Logger.Warning("Couldn't find first child with handle {} for object [{}, {}]", firstChildHandle, kv.key.Handle, kv.key.Num);
                    }

                    //Fill children list
                    while (child != null)
                    {
                        if (!relatives.ContainsKey(child))
                            break;

                        (u32 _, u32 __, u32 childSiblingHandle) = relatives[child];
                        if (childSiblingHandle == RfgZoneObject.InvalidHandle)
                            break;

                        for (ZoneObject secondaryObj in zone.Objects)
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
                    ZoneObject sibling = null;
                    for (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == siblingHandle)
                        {
                            sibling = secondaryObj;
                            break;
                        }
                    }

                    if (sibling == null)
                    {
                        Logger.Warning("Couldn't find sibling with handle {} for object [{}, {}] in {}", siblingHandle, kv.key.Handle, kv.key.Num, zone.Name);
                    }
                }
            }

            return .Ok(zone);
        }

        private static mixin CreateObject<T>(ZoneObject preExisting, DiffUtil changes) where T : ZoneObject
        {
            preExisting != null ? (T)preExisting : changes.CreateObject<T>()
        }

        //object - base class inherited by all other zone object classes
        private Result<ZoneObject, StringView> ObjectReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ZoneObject obj = CreateObject!<ZoneObject>(preExisting, changes);
            obj.Classname.Set(rfgObj.Classname);
            obj.Handle = rfgObj.Handle;
            obj.Num = rfgObj.Num;
            obj.Flags = (ZoneObject.Flags)rfgObj.Flags;
            obj.BBox = .(rfgObj.Bmin, rfgObj.Bmax);
            relatives[obj] = (rfgObj.Parent, rfgObj.Child, rfgObj.Sibling); //Store relations in temporary dictionary. They get applied to the related properties once all objects are loaded

            //Position & orient
            if (rfgObj.GetVec3("just_pos") case .Ok(Vec3 val))
            {
                obj.Position = val;
                obj.Orient.Enabled = false;
            }
            else if (rfgObj.GetPositionOrient("op") case .Ok(PositionOrient val)) //Only check op if just_pos is missing
            {
                obj.Position = val.Position;
                obj.Orient.SetAndEnable(val.Orient);

                //Grab the euler angles when the orientation is first set. We track these to generate rotation delta matrices when time pitch/yaw/roll changes.
                //This avoids issues with gimbal lock and indeterminate angles that we'd get if we tried to extract the euler angles from the matrix every time we edited it.
                obj.Orient.Value.GetEulerAngles(ref obj.Pitch, ref obj.Yaw, ref obj.Roll);
                obj.Pitch = Math.ToDegrees(obj.Pitch);
                obj.Yaw = Math.ToDegrees(obj.Yaw);
                obj.Roll = Math.ToDegrees(obj.Roll);
            }
            else
            {
                //Position is missing. Set to the center of the bounding box. Which all objects have as part of the zone format
                obj.Position = obj.BBox.Center();
                obj.Orient.Enabled = false;
            }

            //Optional display name. No vanilla objects have this
            if (rfgObj.GetString("display_name") case .Ok(StringView val))
            {
                obj.RfgDisplayName.Value.Set(val);
                obj.RfgDisplayName.Enabled = true;
                obj.Name.Set(val);
            }

            return obj;
        }

        //obj_zone : object
        private Result<ZoneObject, StringView> ObjZoneReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjZone obj = CreateObject!<ObjZone>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            List<StringView> propertyNames = scope .();
            for (RfgZoneObject.Property* prop in rfgObj.Properties)
            {
                propertyNames.Add(prop.Name);
            }

            if (rfgObj.GetString("ambient_spawn") case .Ok(StringView val))
            {
				obj.AmbientSpawn.Value.Set(val);
                obj.AmbientSpawn.Enabled = true;
			}

            if (rfgObj.GetString("spawn_resource") case .Ok(StringView val))
            {
				obj.SpawnResource.Value.Set(val);
                obj.SpawnResource.Enabled = true;
			}

            if (rfgObj.GetString("terrain_file_name") case .Ok(StringView val))
            {
				obj.TerrainFileName.Set(val);
			}

            if (rfgObj.GetF32("wind_min_speed") case .Ok(f32 val))
            {
                obj.WindMinSpeed.Value = val;
                obj.WindMinSpeed.Enabled = true;
            }

            if (rfgObj.GetF32("wind_max_speed") case .Ok(f32 val))
            {
                obj.WindMaxSpeed.Value = val;
                obj.WindMaxSpeed.Enabled = true;
            }

            return obj;
        }

        //object_bounding_box : object
        private Result<ZoneObject, StringView> ObjBoundingBoxReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectBoundingBox obj = CreateObject!<ObjectBoundingBox>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBBox("bb") case .Ok(BoundingBox val))
                obj.LocalBBox = val;

            if (rfgObj.GetString("bounding_box_type") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<ObjectBoundingBox.BoundingBoxType>(val) case .Ok(let enumVal))
                {
                    obj.BBType.Value = enumVal;
                    obj.BBType.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing BoundingBoxType enum");
                }
            }
            else
            {
                obj.BBType.Enabled = false;
            }

            return obj;
        }

        //object_dummy : object
        private Result<ZoneObject, StringView> ObjectDummyReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectDummy obj = CreateObject!<ObjectDummy>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("dummy_type") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<ObjectDummy.DummyType>(val) case .Ok(let enumVal))
                {
                    obj.DummyType = enumVal;
                }
                else
                {
                    return .Err("Error deserializing DummyType enum");
                }

                obj.Name.Set(val);
            }

            return obj;
        }

        //player_start : object
        private Result<ZoneObject, StringView> PlayerStartReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            PlayerStart obj = CreateObject!<PlayerStart>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBool("indoor") case .Ok(bool val))
                obj.Indoor = val;

            if (rfgObj.GetString("mp_team") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<Team>(val) case .Ok(let enumVal))
                {
                    obj.MpTeam.Value = enumVal;
                    obj.MpTeam.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing Team enum");
                }
            }
            else
            {
                obj.MpTeam.Value = .None;
                obj.MpTeam.Enabled = false;
            }

            if (rfgObj.GetBool("initial_spawn") case .Ok(bool val))
	            obj.InitialSpawn = val;

            if (rfgObj.GetBool("respawn") case .Ok(bool val))
	            obj.Respawn = val;

            if (rfgObj.GetBool("checkpoint_respawn") case .Ok(bool val))
	            obj.CheckpointRespawn = val;

            if (rfgObj.GetBool("activity_respawn") case .Ok(bool val))
	            obj.ActivityRespawn = val;

            if (rfgObj.GetString("mission_info") case .Ok(StringView val))
            {
				obj.MissionInfo.Value.Set(val);
                obj.MissionInfo.Enabled = true;
                obj.Name.Set(val);
			}

            //Use mp team as name instead of mission_info if no mission is set
            if (obj.Name == "none")
            {
                if (obj.MpTeam.Value == .None)
                {
                    obj.Name.Set("player start");
                }
                else
                {
                    obj.Name.Set(scope $"{obj.MpTeam.Value.ToString(.. scope .())} player start");
                }
            }    

            return obj;
        }

        //trigger_region : object
        private Result<ZoneObject, StringView> TriggerRegionReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            TriggerRegion obj = CreateObject!<TriggerRegion>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("trigger_shape") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<TriggerRegion.Shape>(val) case .Ok(let enumVal))
                {
                    obj.TriggerShape.Value = enumVal;
                    obj.TriggerShape.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing TriggerRegion.Shape enum");
                }
            }
            else
            {
                obj.TriggerShape.Enabled = false;
            }

            if (rfgObj.GetBBox("bb") case .Ok(BoundingBox val))
                obj.LocalBBox = val;

            if (rfgObj.GetF32("outer_radius") case .Ok(f32 val))
                obj.OuterRadius = val;

            if (rfgObj.GetBool("enabled") case .Ok(bool val))
                obj.Enabled = val;

            if (rfgObj.GetString("region_type") case .Ok(StringView val))
		    {
                if (Enum.FromRfgName<TriggerRegion.RegionTypeEnum>(val) case .Ok(let enumVal))
                {
                    obj.RegionType.Value = enumVal;
                    obj.RegionType.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing TriggerRegion.RegionTypeEnum");
                }
			}
            else
            {
                obj.RegionType.Enabled = false;
            }

            if (rfgObj.GetString("region_kill_type") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<TriggerRegion.KillTypeEnum>(val) case .Ok(let enumVal))
                {
                    obj.KillType.Value = enumVal;
                    obj.KillType.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing TriggerRegion.KillTypeEnum");
                }
                obj.Name.Set(val);
            }
            else
            {
                obj.KillType.Enabled = false;
            }

            if (rfgObj.GetString("trigger_flags") case .Ok(StringView val))
            {
                if (Enum.FromRfgFlagsString<TriggerRegion.TriggerRegionFlags>(val) case .Ok(let enumVal))
                {
                    obj.TriggerFlags.Value = enumVal;
                    obj.TriggerFlags.Enabled = true;
                }
                else
                {
                    return .Err("Failed to convert flags string to TriggerRegion.TriggerRegionFlags");
                }
            }
            else
            {
                obj.TriggerFlags.Enabled = false;
            }

            return obj;
        }

        //object_mover : object
        private Result<ZoneObject, StringView> ObjectMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectMover obj = CreateObject!<ObjectMover>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("building_type") case .Ok(StringView val))
            {
                if (Enum.FromRfgFlagsString<ObjectMover.BuildingTypeFlagsEnum>(val) case .Ok(let enumVal))
                {
                    obj.BuildingType = enumVal;
                }
                else
                {
                    return .Err("Failed to convert flags string to ObjectMover.BuildingTypeFlagsEnum");
                }
            }
            else
            {
                obj.BuildingType = .None;
            }

            if (rfgObj.GetU32("dest_checksum") case .Ok(u32 val))
                obj.DestructionChecksum = val;

            if (rfgObj.GetString("gameplay_props") case .Ok(StringView val))
	        {
				obj.GameplayProps.Value.Set(val);
                obj.GameplayProps.Enabled = true;
			}
            else
            {
				obj.GameplayProps = null;
			}

            //if (rfgObj.GetU32("flags") case .Ok(u32 val)) //Disabled. See ObjectMover.InternalFlags comments for reason
            //    obj.InternalFlags = val;

            if (rfgObj.GetString("chunk_flags") case .Ok(StringView val))
            {
                if (Enum.FromRfgFlagsString<ObjectMover.ChunkFlagsEnum>(val) case .Ok(let enumVal))
                {
                    if (enumVal != .None)
                    {
                        obj.ChunkFlags.Value = enumVal;
                        obj.ChunkFlags.Enabled = true;
                    }
                }
                else
                {
                    return .Err("Failed to convert flags string to ObjectMover.ChunkFlagsEnum");
                }
            }
            else
            {
                obj.ChunkFlags.Enabled = false;
            }

            if (rfgObj.GetBool("dynamic_object") case .Ok(bool val))
            {
                obj.Dynamic.SetAndEnable(val);
            }

            if (rfgObj.GetU32("chunk_uid") case .Ok(u32 val))
            {
                obj.ChunkUID.SetAndEnable(val);
            }

            if (rfgObj.GetString("props") case .Ok(StringView val))
            {
				obj.Props.Value.Set(val);
                obj.Props.Enabled = true;
			}

            if (rfgObj.GetString("chunk_name") case .Ok(StringView val))
            {
				obj.ChunkName.Value.Set(val);
                obj.ChunkName.Enabled = true;
                obj.Name.Set(val);
			}

            if (rfgObj.GetU32("uid") case .Ok(u32 val))
	            obj.DestroyableUID = val;

            if (rfgObj.GetU32("shape_uid") case .Ok(u32 val))
            {
                obj.ShapeUID.SetAndEnable(val);
            }

            if (rfgObj.GetString("team") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<Team>(val) case .Ok(let enumVal))
                {
                    obj.Team.Value = enumVal;
                    obj.Team.Enabled = true;
                }
                else
                {
                    return .Err("Error deserializing Team enum");
                }
            }
            else
            {
                obj.Team.Value = .None;
                obj.Team.Enabled = false;
            }

            if (rfgObj.GetF32("control") case .Ok(f32 val))
            {
                obj.Control.Value = val;
                obj.Control.Enabled = true;
            }

            return obj;
        }

        //general_mover : object_mover
        private Result<ZoneObject, StringView> GeneralMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            GeneralMover obj = CreateObject!<GeneralMover>(preExisting, changes);
            if (ObjectMoverReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetU32("gm_flags") case .Ok(u32 val))
                obj.GmFlags = val;

            if (rfgObj.GetU32("original_object") case .Ok(u32 val))
            {
                obj.OriginalObject.SetAndEnable(val);
            }

            if (rfgObj.GetU32("ctype") case .Ok(u32 val))
            {
                obj.CollisionType.SetAndEnable(val);
            }

            if (rfgObj.GetU32("idx") case .Ok(u32 val))
            {
                obj.Idx.Value = val;
                obj.Idx.Enabled = true;
            }

            if (rfgObj.GetU32("mtype") case .Ok(u32 val))
            {
                obj.MoveType.Value = val;
                obj.MoveType.Enabled = true;
            }

            if (rfgObj.GetU32("destruct_uid") case .Ok(u32 val))
            {
                obj.DestructionUID.SetAndEnable(val);
            }

            if (rfgObj.GetI32("hitpoints") case .Ok(i32 val))
            {
                obj.Hitpoints.Value = val;
                obj.Hitpoints.Enabled = true;
            }

            if (rfgObj.GetI32("dislodge_hitpoints") case .Ok(i32 val))
            {
                obj.DislodgeHitpoints.Value = val;
                obj.DislodgeHitpoints.Enabled = true;
            }

            return obj;
        }

        //rfg_mover : object_mover
        private Result<ZoneObject, StringView> RfgMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            RfgMover obj = CreateObject!<RfgMover>(preExisting, changes);
            if (ObjectMoverReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetU32("mtype") case .Ok(u32 val))
	            obj.MoveType = (RfgMover.MoveTypeEnum)val;

            if (rfgObj.GetF32("damage_percent") case .Ok(f32 val))
            {
                obj.DamagePercent.SetAndEnable(val);
            }

            if (rfgObj.GetF32("game_destroyed_pct") case .Ok(f32 val))
            {
                obj.GameDestroyedPercentage.SetAndEnable(val);
            }

            //Note: Didn't implement readers for several disabled properties. See RfgMover definition

            if (rfgObj.GetBuffer("world_anchors") case .Ok(Span<u8> bytes))
            {
                obj.WorldAnchors.Value = bytes.CreateArrayCopy();
                obj.WorldAnchors.Enabled = true;
            }

            if (rfgObj.GetBuffer("dynamic_links") case .Ok(Span<u8> bytes))
            {
                obj.DynamicLinks.Value = bytes.CreateArrayCopy();
                obj.DynamicLinks.Enabled = true;
            }

            return obj;
        }

        //shape_cutter : object
        private Result<ZoneObject, StringView> ShapeCutterReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ShapeCutter obj = CreateObject!<ShapeCutter>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetI16("flags") case .Ok(let val))
            {
                obj.ShapeCutterFlags.SetAndEnable(val);
            }

            if (rfgObj.GetF32("outer_radius") case .Ok(let val))
            {
                obj.OuterRadius.Value = val;
                obj.OuterRadius.Enabled = true;
            }

            if (rfgObj.GetU32("source") case .Ok(let val))
            {
                obj.Source.Value = val;
                obj.Source.Enabled = true;
            }

            if (rfgObj.GetU32("owner") case .Ok(let val))
            {
                obj.Owner.Value = val;
                obj.Owner.Enabled = true;
            }

            if (rfgObj.GetU8("exp_info") case .Ok(let val))
            {
                obj.ExplosionId.Value = val;
                obj.ExplosionId.Enabled = true;
            }

            return obj;
        }

        //object_effect : object
        private Result<ZoneObject, StringView> ObjectEffectReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectEffect obj = CreateObject!<ObjectEffect>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("effect_type") case .Ok(StringView val))
            {
				obj.EffectType.Value.Set(val);
                obj.EffectType.Enabled = true;
			}

            if (rfgObj.GetString("sound_alr") case .Ok(StringView val))
            {
                obj.SoundAlr.Value.Set(val);
                obj.SoundAlr.Enabled = true;
            }

            if (rfgObj.GetString("sound") case .Ok(StringView val))
            {
                obj.Sound.Value.Set(val);
                obj.Sound.Enabled = true;
            }

            if (rfgObj.GetString("visual") case .Ok(StringView val))
            {
                obj.Visual.Value.Set(val);
                obj.Visual.Enabled = true;
            }

            if (rfgObj.GetBool("looping") case .Ok(bool val))
            {
                obj.Looping.SetAndEnable(val);
            }

            return obj;
        }

        //item : object
        private Result<ZoneObject, StringView> ItemReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Item obj = CreateObject!<Item>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("item_type") case .Ok(StringView val))
            {
				obj.ItemType.Value.Set(val);
                obj.ItemType.Enabled = true;
                obj.Name.Set(val);
            }

            //The game looks for two possible properties "respawn" or "respawns" that both get put into the same field in game.
            if (rfgObj.GetBool("respawn") case .Ok(bool val))
            {
                obj.Respawns.SetAndEnable(val);
            }
            else if (rfgObj.GetBool("respawns") case .Ok(bool val))
            {
                obj.Respawns.SetAndEnable(val);
            }

            if (rfgObj.GetBool("preplaced") case .Ok(bool val))
            {
                obj.Preplaced.SetAndEnable(val);
            }

            return obj;
        }

        //weapon : item
        private Result<ZoneObject, StringView> WeaponReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Weapon obj = CreateObject!<Weapon>(preExisting, changes);
            if (ItemReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("item_type") case .Err)
            {
                if (rfgObj.GetString("weapon_type") case .Ok(StringView val)) //Only loaded if the object doesn't have item_type
                {
                    obj.WeaponType.Set(val);
                    obj.Name.Set(val);
                }
            }

            return obj;
        }

        //ladder : object
        private Result<ZoneObject, StringView> LadderReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Ladder obj = CreateObject!<Ladder>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetI32("ladder_rungs") case .Ok(i32 val))
                obj.LadderRungs = val;

            if (rfgObj.GetBool("ladder_enabled") case .Ok(bool val))
            {
                obj.LadderEnabled.SetAndEnable(val);
            }

            return obj;
        }

        //obj_light : object
        private Result<ZoneObject, StringView> ObjLightReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjLight obj = CreateObject!<ObjLight>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetF32("atten_start") case .Ok(f32 val))
            {
                obj.AttenuationStart.Value = val;
                obj.AttenuationStart.Enabled = true;
            }

            if (rfgObj.GetF32("atten_end") case .Ok(f32 val))
            {
                obj.AttenuationEnd.Value = val;
                obj.AttenuationEnd.Enabled = true;
            }
            else if (rfgObj.GetF32("atten_range") case .Ok(f32 attenRange))
            {
                if (!obj.AttenuationStart.Enabled)
                    obj.AttenuationStart.Value = 1.0f;

                obj.AttenuationRange.Value = attenRange;
                obj.AttenuationRange.Enabled = true;
                obj.AttenuationEnd.Value = obj.AttenuationStart.Value + obj.AttenuationRange.Value;
                obj.AttenuationEnd.Enabled = true;
            }

            if (rfgObj.GetString("light_flags") case .Ok(StringView val))
			{
                if (Enum.FromRfgFlagsString<ObjLight.ObjLightFlags>(val) case .Ok(let enumVal))
                {
                    obj.LightFlags.Value = enumVal;
                    obj.LightFlags.Enabled = true;
                }
                else
                {
                    return .Err("Failed to convert flags string to ObjLight.ObjLightFlags");
                }
            }
            else
            {
                obj.LightFlags.Enabled = false;
            }

            if (rfgObj.GetString("type_enum") case .Ok(StringView val))
            {
                if (Enum.FromRfgFlagsString<ObjLight.LightTypeEnum>(val) case .Ok(let enumVal))
                {
                    obj.LightType.Value = enumVal;
                    obj.LightType.Enabled = true;
                }
                else
                {
                    return .Err("Failed to convert flags string to ObjLight.ObjLightFlags");
                }
            }
            else
            {
                obj.LightType.Enabled = false;
            }

            if (rfgObj.GetVec3("clr_orig") case .Ok(let val))
                obj.Color = val;

            if (rfgObj.GetF32("hotspot_size") case .Ok(f32 val))
            {
                obj.HotspotSize.Value = val;
                obj.HotspotSize.Enabled = true;
            }    

            if (rfgObj.GetF32("hotspot_falloff_size") case .Ok(f32 val))
	        {
                obj.HotspotFalloffSize.Value = val;
                obj.HotspotFalloffSize.Enabled = true;
            }    

            if (rfgObj.GetVec3("min_clip") case .Ok(let val))
	            obj.MinClip = val;

            if (rfgObj.GetVec3("max_clip") case .Ok(let val))
	            obj.MaxClip = val;

            /*if (rfgObj.GetI32("clip_mesh") case .Ok(i32 val))
                obj.ClipMesh = val;*/

            return obj;
        }

        //multi_object_marker : object
        private Result<ZoneObject, StringView> MultiObjectMarkerReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            MultiMarker obj = CreateObject!<MultiMarker>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBBox("bb") case .Ok(let val))
                obj.LocalBBox = val;

            if (rfgObj.GetString("marker_type") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<MultiMarker.MultiMarkerType>(val) case .Ok(let enumVal))
                {
                    obj.MarkerType = enumVal;
                }
                else
                {
                    return .Err("Error deserializing MultiMarker.MultiMarkerType enum");
                }

                obj.Name.Set(val);
            }

            if (rfgObj.GetString("mp_team") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<Team>(val) case .Ok(let enumVal))
                {
                    obj.MpTeam = enumVal;
                }
                else
                {
                    return .Err("Error deserializing Team enum");
                }
            }

            if (rfgObj.GetI32("priority") case .Ok(i32 val))
                obj.Priority = val;
            
            if (rfgObj.GetString("backpack_type") case .Ok(StringView val))
            {
                obj.BackpackType.Value.Set(val);
                obj.BackpackType.Enabled = true;
            }

            if (rfgObj.GetI32("num_backpacks") case .Ok(i32 val))
            {
                obj.NumBackpacks.SetAndEnable(val);
            }

            if (rfgObj.GetBool("random_backpacks") case .Ok(bool val))
            {
                obj.RandomBackpacks.SetAndEnable(val);
            }

            if (rfgObj.GetI32("group") case .Ok(i32 val))
            {
                obj.Group.SetAndEnable(val);
            }

            return obj;
        }

        //multi_object_flag : item
        private Result<ZoneObject, StringView> MultiObjectFlagReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            MultiFlag obj = CreateObject!<MultiFlag>(preExisting, changes);
            if (ItemReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("mp_team") case .Ok(StringView val))
            {
                if (Enum.FromRfgName<Team>(val) case .Ok(let enumVal))
                {
                    obj.MpTeam = enumVal;
                }
                else
                {
                    return .Err("Error deserializing Team enum");
                }
            }
            return obj;
        }

        //navpoint : object
        private Result<ZoneObject, StringView> NavPointReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            NavPoint obj = CreateObject!<NavPoint>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBuffer("navpoint_data") case .Ok(Span<u8> bytes))
            {
                obj.NavpointData.Value = bytes.CreateArrayCopy();
                obj.NavpointData.Enabled = true;
            }
            else
            {
                if (rfgObj.GetU8("nav_type") case .Ok(u8 val))
                {
                    obj.NavpointType.SetAndEnable((NavPoint.NavpointType)val);
                }
                else if (rfgObj.GetString("navpoint_type") case .Ok(StringView val))
                {
                    switch (Enum.FromRfgName<NavPoint.NavpointType>(val))
                    {
                        case .Ok(let enumVal):
                            obj.NavpointType.SetAndEnable(enumVal);
                        case .Err:
                            obj.NavpointType.Enabled = false;
                            Logger.Error("Failed to convert '{}' to Navpoint.NavpointType enum for rfg object {}, {}", val, obj.Handle, obj.Num);
                            return .Err(scope $"Failed to convert string to Navpoint.NavpointType enum for zone object property 'navpoint_type");
                    }

                }

                if (rfgObj.GetF32("outer_radius") case .Ok(f32 val))
                {
                    obj.OuterRadius.SetAndEnable(val);
                }
                
                if (rfgObj.GetF32("speed_limit") case .Ok(f32 val))
                {
                    obj.SpeedLimit.SetAndEnable(val);
                }

                if (obj.NavpointType.Enabled && obj.NavpointType.Value != .Patrol)
                {
                    if (rfgObj.GetBool("dont_follow_road") case .Ok(bool val))
                    {
                        obj.DontFollowRoad.SetAndEnable(val);
                    }

                    if (rfgObj.GetBool("ignore_lanes") case .Ok(bool val))
                    {
                        obj.IgnoreLanes.SetAndEnable(val);
                    }
                }
            }

            if (rfgObj.GetBuffer("obj_links") case .Ok(Span<u8> bytes))
            {
                obj.Links.Value = bytes.CreateArrayCopy();
                obj.Links.Enabled = true;
            }
            else if (rfgObj.GetBuffer("navlinks") case .Ok(Span<u8> bytes))
            {
                obj.NavLinks.Value = bytes.CreateArrayCopy();
                obj.NavLinks.Enabled = true;
            }

            if (obj.NavpointType.Enabled && obj.NavpointType.Value == .RoadCheckpoint)
            {
                if (rfgObj.GetMat3("nav_orient") case .Ok(Mat3 val))
                {
                    obj.NavOrient.SetAndEnable(val);
                }
            }

            return obj;
        }

        //cover_node : object
        private Result<ZoneObject, StringView> CoverNodeReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            CoverNode obj = CreateObject!<CoverNode>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBuffer("covernode_data") case .Ok(Span<u8> bytes))
            {
                obj.CovernodeData.Value = bytes.CreateArrayCopy();
                obj.CovernodeData.Enabled = true;
            }
            else if (rfgObj.GetBuffer("cnp") case .Ok(Span<u8> bytes))
            {
                obj.OldCovernodeData.Value = bytes.CreateArrayCopy();
                obj.OldCovernodeData.Enabled = true;
            }

            if (rfgObj.GetF32("def_angle_left") case .Ok(f32 val))
            {
                obj.DefensiveAngleLeft.SetAndEnable(val);
            }

            if (rfgObj.GetF32("def_angle_right") case .Ok(f32 val))
            {
                obj.DefensiveAngleRight.SetAndEnable(val);
            }

            if (rfgObj.GetF32("off_angle_left") case .Ok(f32 val))
            {
                obj.OffensiveAngleLeft.SetAndEnable(val);
            }

            if (rfgObj.GetF32("angle_left") case .Ok(f32 val))
            {
                obj.AngleLeft.SetAndEnable(val);
            }

            if (rfgObj.GetF32("off_angle_right") case .Ok(f32 val))
            {
                obj.OffensiveAngleRight.SetAndEnable(val);
            }

            if (rfgObj.GetF32("angle_right") case .Ok(f32 val))
            {
                obj.AngleRight.SetAndEnable(val);
            }

            if (rfgObj.GetU16("binary_flags") case .Ok(u16 val))
            {
                obj.CoverNodeFlags.SetAndEnable((CoverNode.CoverNodeFlags)val);
            }

            if (rfgObj.GetString("stance") case .Ok(StringView val))
            {
                obj.Stance.Value.Set(val);
                obj.Stance.Enabled = true;
            }

            if (rfgObj.GetString("firing_flags") case .Ok(StringView val))
            {
                obj.FiringFlags.Value.Set(val);
                obj.FiringFlags.Enabled = true;
            }

            return obj;
        }

        //constraint : object
        private Result<ZoneObject, StringView> ConstraintReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            RfgConstraint obj = CreateObject!<RfgConstraint>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetConstraintTemplate() case .Ok(Span<u8> bytes))
            {
                if (bytes.Length != 156)
                {
                    Logger.Error("Constraint template data isn't the right size for rfg object {}, {}. It should be 156 bytes. Its actual size is {} bytes", obj.Handle, obj.Num, bytes.Length);
                    return .Err("Constraint template field isn't the correct size.");
                }

                obj.Template.Value = bytes.CreateArrayCopy();
                obj.Template.Enabled = true;
            }

            if (rfgObj.GetU32("host_handle") case .Ok(u32 val))
            {
                obj.HostHandle.SetAndEnable(val);
            }

            if (rfgObj.GetU32("child_handle") case .Ok(u32 val))
            {
                obj.ChildHandle.SetAndEnable(val);
            }

            if (rfgObj.GetU32("host_index") case .Ok(u32 val))
            {
                obj.HostIndex.SetAndEnable(val);
            }

            if (rfgObj.GetU32("child_index") case .Ok(u32 val))
            {
                obj.ChildIndex.SetAndEnable(val);
            }

            if (rfgObj.GetU32("host_hk_alt_index") case .Ok(u32 val))
            {
                obj.HostHavokAltIndex.SetAndEnable(val);
            }

            if (rfgObj.GetU32("child_hk_alt_index") case .Ok(u32 val))
            {
                obj.ChildHavokAltIndex.SetAndEnable(val);
            }

            return obj;
        }

        //object_action_node : object
        private Result<ZoneObject, StringView> ActionNodeReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ActionNode obj = CreateObject!<ActionNode>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("animation_type") case .Ok(StringView val))
            {
                obj.ActionNodeType.Value.Set(val);
                obj.ActionNodeType.Enabled = true;
            }

            if (rfgObj.GetBool("high_priority") case .Ok(bool val))
            {
                obj.HighPriority.SetAndEnable(val);
            }

            if (rfgObj.GetBool("infinite_duration") case .Ok(bool val))
            {
                obj.InfiniteDuration.SetAndEnable(val);
            }

            if (rfgObj.GetI32("disabled") case .Ok(i32 val))
            {
                obj.DisabledBy.SetAndEnable(val);
            }

            if (rfgObj.GetBool("run_to") case .Ok(bool val))
            {
                obj.RunTo.SetAndEnable(val);
            }

            if (rfgObj.GetF32("outer_radius") case .Ok(f32 val))
            {
                obj.OuterRadius.SetAndEnable(val);
            }

            if (rfgObj.GetBuffer("obj_links") case .Ok(Span<u8> bytes))
            {
                obj.ObjLinks.Value = bytes.CreateArrayCopy();
                obj.ObjLinks.Enabled = true;
            }
            else if (rfgObj.GetBuffer("links") case .Ok(Span<u8> bytes))
            {
                obj.Links.Value = bytes.CreateArrayCopy();
                obj.Links.Enabled = true;
            }

            return obj;
        }
	}
}