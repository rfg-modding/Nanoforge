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
	public static class ZoneImporter
	{
        private static append Dictionary<StringView, ZoneObjectReaderFunc> _zoneObjectReaders;

        //Typed object readers. preExisting can be null to indicate that the reader should create a new instance.
        //When non null it's being used to read inherited properties. E.g. ItemReader calls ObjectReader since item inherits object and object properties.
        typealias ZoneObjectReaderFunc = function Result<ZoneObject, StringView>(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes);
        typealias ObjectRelationsTuple = (u32 parentHandle, u32 firstChildHandle, u32 siblingHandle);

        public static this()
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

        public static Result<Zone, StringView> ImportZone(Span<u8> fileBytes, Span<u8> persistentFileBytes, StringView zoneFilename, DiffUtil changes)
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
            zone.DistrictFlags = zoneFile.Header.DistrictFlags;
            zone.ActivityLayer = false;
            zone.MissionLayer = false;

            Dictionary<ZoneObject, ObjectRelationsTuple> relatives = scope .();
            for (RfgZoneObject* rfgObj in zoneFile.Objects)
            {
                if (_zoneObjectReaders.ContainsKey(rfgObj.Classname))
                {
                    switch (_zoneObjectReaders[rfgObj.Classname](null, rfgObj, relatives, changes))
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
                            }
                        }
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
        private static Result<ZoneObject, StringView> ObjectReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
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
                obj.Orient = .Identity;
            }
            else if (rfgObj.GetPositionOrient("op") case .Ok(PositionOrient val)) //Only check op if just_pos is missing
            {
                obj.Position = val.Position;
                obj.Orient = val.Orient;
            }
            else
            {
                //Position is missing. Set to the center of the bounding box. Which all objects have as part of the zone format
                obj.Position = obj.BBox.Center();
                obj.Orient = .Identity;
            }

            //Optional display name. No vanilla objects have this
            if (rfgObj.GetString("display_name") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                obj.Name.Set(val);
            }

            return obj;
        }

        //obj_zone : object
        private static Result<ZoneObject, StringView> ObjZoneReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjZone obj = CreateObject!<ObjZone>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("ambient_spawn") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.AmbientSpawn.Set(val);
			}

            if (rfgObj.GetString("spawn_resource") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.SpawnResource.Set(val);
			}

            if (rfgObj.GetString("terrain_file_name") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.TerrainFileName.Set(val);
			}

            if (rfgObj.GetF32("wind_min_speed") case .Ok(f32 val))
                obj.WindMinSpeed = val;

            if (rfgObj.GetF32("wind_max_speed") case .Ok(f32 val))
                obj.WindMaxSpeed = val;

            return obj;
        }

        //object_bounding_box : object
        private static Result<ZoneObject, StringView> ObjBoundingBoxReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectBoundingBox obj = CreateObject!<ObjectBoundingBox>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBBox("bb") case .Ok(BoundingBox val))
                obj.LocalBBox = val;

            if (rfgObj.GetString("bounding_box_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "GPS Target":
                        obj.BBType = .GpsTarget;
                    case default:
                        obj.BBType = .None;
                }
            }

            return obj;
        }

        //object_dummy : object
        private static Result<ZoneObject, StringView> ObjectDummyReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectDummy obj = CreateObject!<ObjectDummy>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("dummy_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "Tech Reponse Pos": //Response typo is intentional
                        obj.DummyType = .TechResponsePos;
                    case "VRail Spawn":
                        obj.DummyType = .VRailSpawn;
                    case "Demo Master":
                        obj.DummyType = .DemoMaster;
                    case "Cutscene":
                        obj.DummyType = .Cutscene;
                    case "Air Bomb":
                        obj.DummyType = .AirBomb;
                    case "Rally":
                        obj.DummyType = .Rally;
                    case "Barricade":
                        obj.DummyType = .Barricade;
                    case "Reinforced_Fence":
                        obj.DummyType = .ReinforcedFence;
                    case "Smoke Plume":
                        obj.DummyType = .SmokePlume;
                    case "Demolition":
                        obj.DummyType = .Demolition;
                    case default:
                        obj.DummyType = .None;
                }
                obj.Name.Set(val);
            }

            return obj;
        }

        //player_start : object
        private static Result<ZoneObject, StringView> PlayerStartReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            PlayerStart obj = CreateObject!<PlayerStart>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBool("indoor") case .Ok(bool val))
                obj.Indoor = val;

            obj.MpTeam = ReadTeamProperty(rfgObj, "mp_team");

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
                RemoveNullTerminator!(val);
				obj.MissionInfo.Set(val);
                obj.Name.Set(val);
			}

            //Use mp team as name instead of mission_info if no mission is set
            if (obj.Name == "none")
            {
                if (obj.MpTeam == .None)
                {
                    obj.Name.Set("player start");
                }
                else
                {
                    obj.Name.Set(scope $"{obj.MpTeam.ToString(.. scope .())} player start");
                }
            }    

            return obj;
        }

        //trigger_region : object
        private static Result<ZoneObject, StringView> TriggerRegionReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            TriggerRegion obj = CreateObject!<TriggerRegion>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("trigger_shape") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "sphere":
                        obj.TriggerShape = .Sphere;
                    case "box":
                        obj.TriggerShape = .Box;
                    default:
                        obj.TriggerShape = .Box;
                }
            }

            if (rfgObj.GetBBox("bb") case .Ok(BoundingBox val))
                obj.LocalBBox = val;

            if (rfgObj.GetF32("outer_radius") case .Ok(f32 val))
                obj.OuterRadius = val;

            if (rfgObj.GetBool("enabled") case .Ok(bool val))
                obj.Enabled = val;

            if (rfgObj.GetString("region_type") case .Ok(StringView val))
		    {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "kill human":
                        obj.RegionType = .KillHuman;
                    case "default":
                        obj.RegionType = .Default;
                    default:
                        obj.RegionType = .Default;
                }
			}
            else
            {
                obj.RegionType = .Default;
            }

            if (rfgObj.GetString("region_kill_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "mine":
                        obj.KillType = .Mine;
                    case "cliff":
                        obj.KillType = .Cliff;
                    default:
                        obj.KillType = .Cliff;
                }
                obj.Name.Set(val);
            }
            else
            {
                obj.KillType = .Cliff;
            }

            if (rfgObj.GetString("trigger_flags") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                if (val.Contains("not_in_activity"))
                    obj.TriggerFlags |= .NotInActivity;
                if (val.Contains("not_in_mission"))
                    obj.TriggerFlags |= .NotInMission;
            }
            else
            {
                obj.TriggerFlags = .None;
            }

            return obj;
        }

        //object_mover : object
        private static Result<ZoneObject, StringView> ObjectMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectMover obj = CreateObject!<ObjectMover>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("building_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                if (val.Contains("Dynamic"))
                    obj.BuildingType |= .Dynamic;
                if (val.Contains("Force_Field"))
                    obj.BuildingType |= .ForceField;
                if (val.Contains("Bridge"))
                    obj.BuildingType |= .Bridge;
                if (val.Contains("Raid"))
                    obj.BuildingType |= .Raid;
                if (val.Contains("House"))
                    obj.BuildingType |= .House;
                if (val.Contains("Player_Base"))
                    obj.BuildingType |= .PlayerBase;
                if (val.Contains("Communications"))
                    obj.BuildingType |= .Communications;
            }
            else
            {
                obj.BuildingType = .None;
            }

            if (rfgObj.GetU32("dest_checksum") case .Ok(u32 val))
                obj.DestructionChecksum = val;

            if (rfgObj.GetString("gameplay_props") case .Ok(StringView val))
	        {
                RemoveNullTerminator!(val);
				obj.GameplayProps.Set(val);
			}
            else
            {
				obj.GameplayProps.Set("Default");
			}

            //if (rfgObj.GetU32("flags") case .Ok(u32 val)) //Disabled. See ObjectMover.InternalFlags comments for reason
            //    obj.InternalFlags = val;

            if (rfgObj.GetString("chunk_flags") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                obj.ChunkFlags = .None;
                if (val.Contains("child_gives_control"))
                    obj.ChunkFlags |= .ChildGivesControl;
                if (val.Contains("building"))
                    obj.ChunkFlags |= .Building;
                if (val.Contains("dynamic_link"))
                    obj.ChunkFlags |= .DynamicLink;
                if (val.Contains("world_anchor"))
                    obj.ChunkFlags |= .WorldAnchor;
                if (val.Contains("no_cover"))
                    obj.ChunkFlags |= .NoCover;
                if (val.Contains("propaganda"))
                    obj.ChunkFlags |= .Propaganda;
                if (val.Contains("kiosk"))
                    obj.ChunkFlags |= .Kiosk;
                if (val.Contains("touch_terrain"))
                    obj.ChunkFlags |= .TouchTerrain;
                if (val.Contains("supply_crate"))
                    obj.ChunkFlags |= .SupplyCrate;
                if (val.Contains("mining"))
                    obj.ChunkFlags |= .Mining;
                if (val.Contains("one_of_many"))
                    obj.ChunkFlags |= .OneOfMany;
                if (val.Contains("plume_on_death"))
                    obj.ChunkFlags |= .PlumeOnDeath;
                if (val.Contains("invulnerable"))
                    obj.ChunkFlags |= .Invulnerable;
                if (val.Contains("inherit_damage_pct"))
                    obj.ChunkFlags |= .InheritDamagePercentage;
                if (val.Contains("regrow_on_stream"))
                    obj.ChunkFlags |= .RegrowOnStream;
                if (val.Contains("casts_drop_shadow"))
                    obj.ChunkFlags |= .CastsDropShadow;
                if (val.Contains("disable_collapse_effect"))
                    obj.ChunkFlags |= .DisableCollapseEffect;
                if (val.Contains("force_dynamic"))
                    obj.ChunkFlags |= .ForceDynamic;
                if (val.Contains("show_on_map"))
                    obj.ChunkFlags |= .ShowOnMap;
                if (val.Contains("regenerate"))
                    obj.ChunkFlags |= .Regenerate;
                if (val.Contains("casts_shadow"))
                    obj.ChunkFlags |= .CastsShadow;
            }
            else
            {
                obj.ChunkFlags = .None;
            }

            if (rfgObj.GetBool("dynamic_object") case .Ok(bool val))
	            obj.Dynamic = val;

            if (rfgObj.GetU32("chunk_uid") case .Ok(u32 val))
                obj.ChunkUID = val;

            if (rfgObj.GetString("props") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.Props.Set(val);
			}

            if (rfgObj.GetString("chunk_name") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.ChunkName.Set(val);
                obj.Name.Set(val);
			}

            if (rfgObj.GetU32("uid") case .Ok(u32 val))
	            obj.DestroyableUID = val;

            if (rfgObj.GetU32("shape_uid") case .Ok(u32 val))
	            obj.ShapeUID = val;

            obj.Team = ReadTeamProperty(rfgObj, "team");

            if (rfgObj.GetF32("control") case .Ok(f32 val))
                obj.Control = val;

            return obj;
        }

        //general_mover : object_mover
        private static Result<ZoneObject, StringView> GeneralMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            GeneralMover obj = CreateObject!<GeneralMover>(preExisting, changes);
            if (ObjectMoverReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetU32("gm_flags") case .Ok(u32 val))
                obj.GmFlags = val;

            if (rfgObj.GetU32("original_object") case .Ok(u32 val))
                obj.OriginalObject = val;

            if (rfgObj.GetU32("ctype") case .Ok(u32 val))
                obj.CollisionType = val;

            if (rfgObj.GetU32("idx") case .Ok(u32 val))
	            obj.Idx = val;

            if (rfgObj.GetU32("mtype") case .Ok(u32 val))
	            obj.MoveType = val;

            if (rfgObj.GetU32("destruct_uid") case .Ok(u32 val))
	            obj.DestructionUID = val;

            if (rfgObj.GetI32("hitpoints") case .Ok(i32 val))
	            obj.Hitpoints = val;

            if (rfgObj.GetI32("dislodge_hitpoints") case .Ok(i32 val))
	            obj.DislodgeHitpoints = val;

            return obj;
        }

        //rfg_mover : object_mover
        private static Result<ZoneObject, StringView> RfgMoverReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            RfgMover obj = CreateObject!<RfgMover>(preExisting, changes);
            if (ObjectMoverReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetU32("mtype") case .Ok(u32 val))
	            obj.MoveType = (RfgMover.MoveTypeEnum)val;

            //if (rfgObj.GetF32("damage_percent") case .Ok(f32 val))
            //    obj.DamagePercent = val;

            //Note: Didn't implement readers for several disabled properties. See RfgMover definition

            if (rfgObj.GetBuffer("world_anchors") case .Ok(Span<u8> bytes))
            {
                switch (ProjectDB.CreateBuffer(bytes, scope $"world_anchors_{obj.Handle}"))
                {
                    case .Ok(ProjectBuffer buffer):
                        obj.WorldAnchors = buffer;
                    case .Err:
                        Logger.Error("Failed to create buffer to store world_anchors for rfg object {}, {}", obj.Handle, obj.Num);
                        return .Err("Failed to create world_anchors buffer");
                }
            }

            if (rfgObj.GetBuffer("dynamic_links") case .Ok(Span<u8> bytes))
            {
                switch (ProjectDB.CreateBuffer(bytes, scope $"dynamic_links_{obj.Handle}"))
                {
                    case .Ok(ProjectBuffer buffer):
                        obj.DynamicLinks = buffer;
                    case .Err:
                        Logger.Error("Failed to create buffer to store dynamic_links for rfg object {}, {}", obj.Handle, obj.Num);
                        return .Err("Failed to create dynamic_links buffer");
                }
            }

            return obj;
        }

        //shape_cutter : object
        private static Result<ZoneObject, StringView> ShapeCutterReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ShapeCutter obj = CreateObject!<ShapeCutter>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetI16("flags") case .Ok(let val))
                obj.ShapeCutterFlags = val;

            if (rfgObj.GetF32("outer_radius") case .Ok(let val))
                obj.OuterRadius = val;

            if (rfgObj.GetU32("source") case .Ok(let val))
                obj.Source = val;

            if (rfgObj.GetU32("owner") case .Ok(let val))
                obj.Owner = val;

            if (rfgObj.GetU8("exp_info") case .Ok(let val))
                obj.ExplosionId = val;

            return obj;
        }

        //object_effect : object
        private static Result<ZoneObject, StringView> ObjectEffectReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjectEffect obj = CreateObject!<ObjectEffect>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("effect_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.EffectType.Set(val);
			}

            return obj;
        }

        //item : object
        private static Result<ZoneObject, StringView> ItemReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Item obj = CreateObject!<Item>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("item_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
				obj.ItemType.Set(val);
                obj.Name.Set(val);
            }

            return obj;
        }

        //weapon : item
        private static Result<ZoneObject, StringView> WeaponReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Weapon obj = CreateObject!<Weapon>(preExisting, changes);
            if (ItemReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("item_type") case .Err)
            {
                if (rfgObj.GetString("weapon_type") case .Ok(StringView val)) //Only loaded if the object doesn't have item_type
                {
                    RemoveNullTerminator!(val);
                    obj.WeaponType.Set(val);
                    obj.Name.Set(val);
                }
            }

            return obj;
        }

        //ladder : object
        private static Result<ZoneObject, StringView> LadderReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            Ladder obj = CreateObject!<Ladder>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetI32("ladder_rungs") case .Ok(i32 val))
                obj.LadderRungs = val;

            if (rfgObj.GetBool("ladder_enabled") case .Ok(bool val))
                obj.Enabled = val;

            return obj;
        }

        //obj_light : object
        private static Result<ZoneObject, StringView> ObjLightReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjLight obj = CreateObject!<ObjLight>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetF32("atten_start") case .Ok(f32 val))
                obj.AttenuationStart = val;

            if (rfgObj.GetF32("atten_end") case .Ok(f32 val))
	            obj.AttenuationEnd = val;
            else if (rfgObj.GetF32("atten_range") case .Ok(f32 attenRange))
                obj.AttenuationEnd = obj.AttenuationStart + obj.AttenuationRange;

            if (rfgObj.GetString("light_flags") case .Ok(StringView val))
			{
                RemoveNullTerminator!(val);
                obj.LightFlags = .None;
                if (val.Contains("use_clipping"))
                    obj.LightFlags |= .UseClipping;
                if (val.Contains("daytime"))
                    obj.LightFlags |= .DayTime;
                if (val.Contains("nighttime"))
                    obj.LightFlags |= .NightTime;
            }

            if (rfgObj.GetString("type_enum") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
				{
                    case "spot":
                        obj.LightType = .Spot;
                    case "omni":
                        obj.LightType = .Omni;
                    default:
                        obj.LightType = .Omni;
                }	
            }
            else
                obj.LightType = .Omni;

            if (rfgObj.GetVec3("clr_orig") case .Ok(let val))
                obj.Color = val;

            if (rfgObj.GetF32("hotspot_size") case .Ok(f32 val))
                obj.HotspotSize = val;

            if (rfgObj.GetF32("hotspot_falloff_size") case .Ok(f32 val))
	            obj.HotspotFalloffSize = val;

            if (rfgObj.GetVec3("min_clip") case .Ok(let val))
	            obj.MinClip = val;

            if (rfgObj.GetVec3("max_clip") case .Ok(let val))
	            obj.MaxClip = val;

            /*if (rfgObj.GetI32("clip_mesh") case .Ok(i32 val))
                obj.ClipMesh = val;*/

            return obj;
        }

        //multi_object_marker : object
        private static Result<ZoneObject, StringView> MultiObjectMarkerReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            MultiMarker obj = CreateObject!<MultiMarker>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetBBox("bb") case .Ok(let val))
                obj.LocalBBox = val;

            if (rfgObj.GetString("marker_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "Siege target":
                        obj.MarkerType = .SiegeTarget;
                    case "Backpack rack":
                        obj.MarkerType = .BackpackRack;
                    case "Spawn node":
                        obj.MarkerType = .SpawnNode;
                    case "Flag capture zone":
                        obj.MarkerType = .FlagCaptureZone;
                    case "King of the Hill target":
                        obj.MarkerType = .KingOfTheHillTarget;
                    case "Spectator camera":
                        obj.MarkerType = .SpectatorCamera;
                    default:
                        obj.MarkerType = .None;
                }
                obj.Name.Set(val);
            }

            obj.MpTeam = ReadTeamProperty(rfgObj, "mp_team");

            if (rfgObj.GetI32("priority") case .Ok(i32 val))
                obj.Priority = val;
            
            if (rfgObj.GetString("backpack_type") case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "Commando":
                        obj.BackpackType = .Commando;
                    case "Fleetfoot":
                        obj.BackpackType = .Fleetfoot;
                    case "Heal":
                        obj.BackpackType = .Heal;
                    case "Jetpack":
                        obj.BackpackType = .Jetpack;
                    case "Kaboom":
                        obj.BackpackType = .Kaboom;
                    case "Rhino":
                        obj.BackpackType = .Rhino;
                    case "Shockwave":
                        obj.BackpackType = .Shockwave;
                    case "Stealth":
                        obj.BackpackType = .Stealth;
                    case "Thrust":
                        obj.BackpackType = .Thrust;
                    case "Tremor":
                        obj.BackpackType = .Tremor;
                    case "Vision":
                        obj.BackpackType = .Vision;
                    default:
                        obj.BackpackType = .Jetpack;
                }
            }

            if (rfgObj.GetI32("num_backpacks") case .Ok(i32 val))
                obj.NumBackpacks = val;

            if (rfgObj.GetBool("random_backpacks") case .Ok(bool val))
                obj.RandomBackpacks = val;

            if (rfgObj.GetI32("group") case .Ok(i32 val))
                obj.Group = val;

            return obj;
        }

        //multi_object_flag : item
        private static Result<ZoneObject, StringView> MultiObjectFlagReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            MultiFlag obj = CreateObject!<MultiFlag>(preExisting, changes);
            if (ItemReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            obj.MpTeam = ReadTeamProperty(rfgObj, "mp_team");

            return obj;
        }

        //navpoint : object
        private static Result<ZoneObject, StringView> NavPointReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            NavPoint obj = CreateObject!<NavPoint>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read nav node specific properties. Not needed until SP map editing is added.

            return obj;
        }

        //cover_node : object
        private static Result<ZoneObject, StringView> CoverNodeReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            CoverNode obj = CreateObject!<CoverNode>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read cover node specific properties. Not needed until SP map editing is added.

            return obj;
        }

        //constraint : object
        private static Result<ZoneObject, StringView> ConstraintReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            RfgConstraint obj = CreateObject!<RfgConstraint>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read constraint specific properties. Not needed until SP map editing is added.

            return obj;
        }

        //object_action_node : object
        private static Result<ZoneObject, StringView> ActionNodeReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ActionNode obj = CreateObject!<ActionNode>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read action node specific properties. Not needed until SP map editing is added.

            return obj;
        }

        private static Team ReadTeamProperty(RfgZoneObject* rfgObj, StringView name)
        {
            if (rfgObj.GetString(name) case .Ok(StringView val))
            {
                RemoveNullTerminator!(val);
                switch (val)
                {
                    case "Neutral":
                        return .Neutral;
                    case "Guerilla":
                        return .Guerilla;
                    case "EDF":
                        return .EDF;
                    case "Civilian":
                        return .Civilian;
                    case "Marauder":
                        return .Marauder;
                    case "None":
                        return .None;
                    default:
                        return .None;
                }
            }
            else
            {
                return .None;
            }
        }

        private static mixin RemoveNullTerminator(StringView str)
        {
            if (str.EndsWith('\0'))
                str.RemoveFromEnd(1);
        }
	}
}