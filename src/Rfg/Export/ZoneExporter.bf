using Common;
using System;
using System.Threading;
using System.Collections;
using Nanoforge.Misc;
using System.IO;
using RfgTools.Hashing;
using RfgTools.Formats.Zones;
using Common.Math;
using Nanoforge.App;
using Common.IO;

namespace Nanoforge.Rfg.Export
{
	public class ZoneExporter
	{
        private append Dictionary<Type, ZoneObjectWriterFunc> _objectWriters = .();
        private append Monitor _lock;
        private ZoneFileWriter _writer = null;

        /*private append FileStream stream;
        private int numPropertiesWritten = 0;*/

        //Returns the number of properties written
        typealias ZoneObjectWriterFunc = function Result<void>(ZoneExporter this, ZoneObject objBase);

        //Handle based object relations used by RFG. Only used by Nanoforge during export since it converts the handles to ZoneObject references on import.
        public struct RfgObjectRelations
        {
            public u32 Parent;
            public u32 Child;
            public u32 Sibling;
        }

        public this()
        {
            _objectWriters[typeof(ZoneObject)] = => ObjectWriter;
            _objectWriters[typeof(ObjZone)] = => ObjZoneWriter;
            _objectWriters[typeof(ObjectBoundingBox)] = => ObjBoundingBoxWriter;
            _objectWriters[typeof(ObjectDummy)] = => ObjectDummyWriter;
            _objectWriters[typeof(PlayerStart)] = => PlayerStartWriter;
            _objectWriters[typeof(TriggerRegion)] = => TriggerRegionWriter;
            _objectWriters[typeof(ObjectMover)] = => ObjectMoverWriter;
            _objectWriters[typeof(GeneralMover)] = => GeneralMoverWriter;
            _objectWriters[typeof(RfgMover)] = => RfgMoverWriter;
            _objectWriters[typeof(ShapeCutter)] = => ShapeCutterWriter;
            _objectWriters[typeof(ObjectEffect)] = => ObjectEffectWriter;
            _objectWriters[typeof(Item)] = => ItemWriter;
            _objectWriters[typeof(Weapon)] = => WeaponWriter;
            _objectWriters[typeof(Ladder)] = => LadderWriter;
            _objectWriters[typeof(ObjLight)] = => ObjLightWriter;
            _objectWriters[typeof(MultiMarker)] = => MultiObjectMarkerWriter;
            _objectWriters[typeof(MultiFlag)] = => MultiObjectFlagWriter;
            _objectWriters[typeof(NavPoint)] = => NavPointWriter;
            _objectWriters[typeof(CoverNode)] = => CoverNodeWriter;
            _objectWriters[typeof(RfgConstraint)] = => ConstraintWriter;
            _objectWriters[typeof(ActionNode)] = => ActionNodeWriter;
        }

        //ZoneExporter.Export(zone, scope $"", persistent: false) case .Err)
        public Result<void> Export(Zone zone, StringView outputPath, bool persistent)
        {
            ScopedLock!(_lock); //Make sure only one thread exports at a time. This code isn't designed for multithreading

            //Collect objects we're writing to this file
            List<ZoneObject> objects = scope .();
            for (ZoneObject object in zone.Objects)
            {
                if (persistent && !object.Persistent)
                    continue;

                objects.Add(object);
            }

            //Check if object limit is exceeded. This limit is based on the max object count of the relation data section.
            //The necessity of that section is still in question, so it may be possible to exceed this. Just playing it safe for now. No zone in the SP map exceeds 3000 objects
            //TODO: See if this limit can be exceeded. Likely depends on how necessary the relation data hash table is
            const int maxObjects = 7280;
            if (objects.Count > maxObjects)
            {
                Logger.Error("Failed to export {0}. Zone has {1} objects. This exceeds the maximum per zone object limit of {2}", zone.Name, objects.Count, maxObjects);
                return .Err;
            }

            //NOTE: This collects relations on all objects in the zone. While some objects get put in the persistent zone file its still all one zone
            //Update RFG relative handles. For convenience NF uses object references instead of RFGs u32 handles. Need to generate them again for the game to use though.
            //Collect parents and children
            Dictionary<ZoneObject, RfgObjectRelations> relations = scope .();
            for (ZoneObject obj in zone.Objects)
            {
                RfgObjectRelations relatives = .();
                relatives.Parent = (obj.Parent != null) ? obj.Parent.Handle : ZoneObject.NullHandle;
                relatives.Child = (obj.Children.Count > 0) ? obj.Children[0].Handle : ZoneObject.NullHandle;
                relatives.Sibling = ZoneObject.NullHandle;
                relations[obj] = relatives;
            }

            //Update sibling values. Basically a linked list of u32 handles. Easier to do as a second loop since we know all objects are in the relations dictionary
            for (ZoneObject obj in zone.Objects)
            {
                if (obj.Children.Count > 1)
            	{
                    for (int i in 0 ..< obj.Children.Count - 1)
                    {
                        ZoneObject current = obj.Children[i];
                        ZoneObject next = obj.Children[i + 1];
                        relations[current].Sibling = next.Handle;
                    }
            	}
            }

            //Note: I was having performance issues with FileStream. It's excessively flushing when ZoneFileWriter.EndObject() is called and seeks backwards to the object header.
            //      I don't see an obvious fix. So for the moment I'm just writing the zone file in memory. I needed quick writing for my zone validation tests. When using FileStream it took 15+ minutes to export the zones.
            MemoryStream stream = scope .(5000000); //The biggest vanilla zone file is 840KB. So we're unlikely to exceed this.
            /*FileStream stream = scope .();
            if (stream.Open(outputPath, mode: .Create, access: .Write, share: .None, bufferSize: 1000000) case .Err(FileOpenError err))
            {
                Logger.Error("Failed to open '{0}'. Error: {1}", outputPath, err);
                return .Err;
            }*/

            _writer = scope ZoneFileWriter();
            if (_writer.BeginFile(stream, zone.District, zone.DistrictFlags, zone.DistrictHash) case .Err(let err))
            {
                Logger.Error("Zone exporter failed to begin zone file. Error: {0}", err);
                return .Err;
            }

            for (ZoneObject object in objects)
            {
                var relatives = relations[object];
                if (_writer.BeginObject(object.Classname, object.Handle, object.Num, (u16)object.Flags, object.BBox.Min, object.BBox.Max, relatives.Parent, relatives.Sibling, relatives.Child) case .Err(let err))
                {
                    Logger.Error("Zone exporter failed to begin object. Error: {0}", err);
                    return .Err;
                }

                if (_objectWriters.ContainsKey(object.GetType()))
                {
                    if (_objectWriters[object.GetType()](this, object) case .Err)
                    {
                        Logger.Error(scope $"Zone object writer for class '{object.Classname}' failed.");
                        return .Err("Zone object write error.");
                    }
                }
                else
                {
                    Logger.Error(scope $"Zone exporter encountered an unsupported zone object class '{object.Classname}'. Please implement and register a writer for that class and try again.");
                    return .Err("Zone exporter encountered an unsupported zone object class.");
                }

                if (_writer.EndObject() case .Err(let err))
                {
                    Logger.Error("Zone exporter failed to end object. Error: {0}", err);
                    return .Err;
                }
            }

            if (_writer.EndFile() case .Err(let err))
            {
                Logger.Error("Zone exporter failed to end zone file. Error: {0}", err);
                return .Err;
            }

            var fileData = Span<u8>(stream.Memory.Ptr, _writer.DataLength);
            if (File.WriteAll(outputPath, fileData) case .Err)
            {
                Logger.Error("Failed write zone file to disk at '{}'", outputPath);
                return .Err;
            }

            _writer = null;
            return .Ok;
        }

        //object - base class inherited by all other zone object classes
        private Result<void> ObjectWriter(ZoneObject objBase)
        {
            if (objBase.Orient.Enabled)
            {
                Try!(_writer.WritePositionOrientationProperty(objBase.Position, objBase.Orient.Value));
            }
            else
            {
                Try!(_writer.WriteVec3Property("just_pos", objBase.Position));
            }

            if (objBase.RfgDisplayName.Enabled)
            {
                Try!(_writer.WriteStringProperty("display_name", objBase.RfgDisplayName.Value));
            }

            return .Ok;
        }

        //obj_zone : object
        private Result<void> ObjZoneWriter(ZoneObject objBase)
        {
            ObjZone object = (ObjZone)objBase;
            Try!(ObjectWriter(object));

            if (object.AmbientSpawn.Enabled)
            {
                Try!(_writer.WriteStringProperty("ambient_spawn", object.AmbientSpawn.Value));
            }

            if (object.SpawnResource.Enabled)
            {
                Try!(_writer.WriteStringProperty("spawn_resource", object.SpawnResource.Value));
            }

            Try!(_writer.WriteStringProperty("terrain_file_name", object.TerrainFileName));

            if (object.WindMinSpeed.Enabled)
            {
                Try!(_writer.WriteFloatProperty("wind_min_speed", object.WindMinSpeed.Value));
            }

            if (object.WindMaxSpeed.Enabled)
            {
                Try!(_writer.WriteFloatProperty("wind_max_speed", object.WindMaxSpeed.Value));
            }

            return .Ok;
        }

        //object_bounding_box : object
        private Result<void> ObjBoundingBoxWriter(ZoneObject objBase)
        {
            ObjectBoundingBox object = (ObjectBoundingBox)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteBoundingBoxProperty("bb", object.LocalBBox));

            if (object.BBType.Enabled)
            {
                Try!(_writer.WriteEnum<ObjectBoundingBox.BoundingBoxType>("bounding_box_type", object.BBType.Value));
            }

            return .Ok;
        }

        //object_dummy : object
        private Result<void> ObjectDummyWriter(ZoneObject objBase)
        {
            ObjectDummy object = (ObjectDummy)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteStringProperty("dummy_type", Enum.ToRfgName<ObjectDummy.DummyType>(object.DummyType, .. scope .())));

            return .Ok;
        }

        //player_start : object
        private Result<void> PlayerStartWriter(ZoneObject objBase)
        {
            PlayerStart object = (PlayerStart)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteBoolProperty("indoor", object.Indoor));

            if (object.MpTeam.Enabled)
            {
                Try!(_writer.WriteEnum<Team>("mp_team", object.MpTeam.Value));
            }

            Try!(_writer.WriteBoolProperty("initial_spawn", object.InitialSpawn));

            Try!(_writer.WriteBoolProperty("respawn", object.Respawn));

            Try!(_writer.WriteBoolProperty("checkpoint_respawn", object.CheckpointRespawn));

            Try!(_writer.WriteBoolProperty("activity_respawn", object.ActivityRespawn));

            if (object.MissionInfo.Enabled)
            {
                Try!(_writer.WriteStringProperty("mission_info", object.MissionInfo.Value));
            }

            return .Ok;
        }

        //trigger_region : object
        private Result<void> TriggerRegionWriter(ZoneObject objBase)
        {
            TriggerRegion object = (TriggerRegion)objBase;
            Try!(ObjectWriter(object));

            if (object.TriggerShape.Enabled)
            {
                Try!(_writer.WriteEnum<TriggerRegion.Shape>("trigger_shape", object.TriggerShape.Value));
            }

            Try!(_writer.WriteBoundingBoxProperty("bb", object.LocalBBox));

            Try!(_writer.WriteFloatProperty("outer_radius", object.OuterRadius));

            Try!(_writer.WriteBoolProperty("enabled", object.Enabled));

            if (object.RegionType.Enabled)
            {
                Try!(_writer.WriteEnum<TriggerRegion.RegionTypeEnum>("region_type", object.RegionType.Value));
            }

            if (object.KillType.Enabled)
            {
                Try!(_writer.WriteEnum<TriggerRegion.KillTypeEnum>("region_kill_type", object.KillType.Value));
            }

            if (object.TriggerFlags.Enabled)
            {
                Try!(_writer.WriteFlagsString<TriggerRegion.TriggerRegionFlags>("trigger_flags", object.TriggerFlags.Value));
            }

            return .Ok;
        }

        //object_mover : object
        private Result<void> ObjectMoverWriter(ZoneObject objBase)
        {
            ObjectMover object = (ObjectMover)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteFlagsString<ObjectMover.BuildingTypeFlagsEnum>("building_type", object.BuildingType));

            Try!(_writer.WriteU32Property("dest_checksum", object.DestructionChecksum));

            if (object.GameplayProps.Enabled)
            {
                Try!(_writer.WriteStringProperty("gameplay_props", object.GameplayProps.Value));
            }

            //Try!(_writer.WriteU32Property("chunk_flags", object.InternalFlags));

            if (object.ChunkFlags.Enabled)
            {
                Try!(_writer.WriteFlagsString<ObjectMover.ChunkFlagsEnum>("chunk_flags", object.ChunkFlags.Value));
            }

            if (object.Dynamic.Enabled)
            {
                Try!(_writer.WriteBoolProperty("dynamic_object", object.Dynamic.Value));
            }

            if (object.ChunkUID.Enabled)
            {
                Try!(_writer.WriteU32Property("chunk_uid", object.ChunkUID.Value));
            }

            if (object.Props.Enabled)
            {
                Try!(_writer.WriteStringProperty("props", object.Props.Value));
            }

            if (object.ChunkName.Enabled)
            {
                Try!(_writer.WriteStringProperty("chunk_name", object.ChunkName.Value));
            }

            Try!(_writer.WriteU32Property("uid", object.DestroyableUID));

            if (object.ShapeUID.Enabled)
            {
                Try!(_writer.WriteU32Property("shape_uid", object.ShapeUID.Value));
            }

            if (object.Team.Enabled)
            {
                Try!(_writer.WriteEnum<Team>("team", object.Team.Value));
            }

            if (object.Control.Enabled)
            {
                Try!(_writer.WriteFloatProperty("control", object.Control.Value));
            }

            return .Ok;
        }

        //general_mover : object_mover
        private Result<void> GeneralMoverWriter(ZoneObject objBase)
        {
            GeneralMover object = (GeneralMover)objBase;
            Try!(ObjectMoverWriter(object));

            Try!(_writer.WriteU32Property("gm_flags", object.GmFlags));

            if (object.OriginalObject.Enabled)
            {
                Try!(_writer.WriteU32Property("original_object", object.OriginalObject.Value));
            }

            if (object.CollisionType.Enabled)
            {
                Try!(_writer.WriteU32Property("ctype", object.CollisionType.Value));
            }

            if (object.Idx.Enabled)
            {
                Try!(_writer.WriteU32Property("idx", object.Idx.Value));
            }

            if (object.MoveType.Enabled)
            {
                Try!(_writer.WriteU32Property("mtype", object.MoveType.Value));
            }

            if (object.DestructionUID.Enabled)
            {
                Try!(_writer.WriteU32Property("destruct_uid", object.DestructionUID.Value));
            }

            if (object.Hitpoints.Enabled)
            {
                Try!(_writer.WriteI32Property("hitpoints", object.Hitpoints.Value));
            }

            if (object.DislodgeHitpoints.Enabled)
            {
                Try!(_writer.WriteI32Property("dislodge_hitpoints", object.DislodgeHitpoints.Value));
            }

            return .Ok;
        }

        //rfg_mover : object_mover
        private Result<void> RfgMoverWriter(ZoneObject objBase)
        {
            RfgMover object = (RfgMover)objBase;
            Try!(ObjectMoverWriter(object));

            Try!(_writer.WriteU32Property("mtype", (u32)object.MoveType)); //One of the rare few enums we write into an integer instead of a string

            if (object.DamagePercent.Enabled)
            {
                Try!(_writer.WriteFloatProperty("damage_percent", object.DamagePercent.Value));
            }

            if (object.GameDestroyedPercentage.Enabled)
            {
                Try!(_writer.WriteFloatProperty("game_destroyed_pct", object.GameDestroyedPercentage.Value));
            }

            if (object.WorldAnchors != null)
            {
                Try!(_writer.WriteBuffer("world_anchors", object.WorldAnchors));
            }

            if (object.DynamicLinks != null)
            {
                Try!(_writer.WriteBuffer("dynamic_links", object.DynamicLinks));
            }

            return .Ok;
        }

        //shape_cutter : object
        private Result<void> ShapeCutterWriter(ZoneObject objBase)
        {
            ShapeCutter object = (ShapeCutter)objBase;
            ObjectWriter(objBase);

            if (object.ShapeCutterFlags.Enabled)
            {
                Try!(_writer.WriteI16Property("flags", object.ShapeCutterFlags.Value));
            }

            if (object.OuterRadius.Enabled)
            {
                Try!(_writer.WriteFloatProperty("outer_radius", object.OuterRadius.Value));
            }

            if (object.Source.Enabled)
            {
                Try!(_writer.WriteU32Property("source", object.Source.Value));
            }

            if (object.Owner.Enabled)
            {
                Try!(_writer.WriteU32Property("owner", object.Owner.Value));
            }

            if (object.ExplosionId.Enabled)
            {
                Try!(_writer.WriteU8Property("exp_info", object.ExplosionId.Value));
            }

            return .Ok;
        }

        //object_effect : object
        private Result<void> ObjectEffectWriter(ZoneObject objBase)
        {
            ObjectEffect object = (ObjectEffect)objBase;
            Try!(ObjectWriter(object));

            if (object.EffectType.Enabled)
            {
                Try!(_writer.WriteStringProperty("effect_type", object.EffectType.Value));
            }

            if (object.SoundAlr.Enabled)
            {
                Try!(_writer.WriteStringProperty("sound_alr", object.SoundAlr.Value));
            }

            if (object.Sound.Enabled)
            {
                Try!(_writer.WriteStringProperty("sound", object.Sound.Value));
            }

            if (object.Visual.Enabled)
            {
                Try!(_writer.WriteStringProperty("visual", object.Visual.Value));
            }

            if (object.Looping.Enabled)
            {
                Try!(_writer.WriteBoolProperty("looping", object.Looping.Value));
            }

            return .Ok;
        }

        //item : object
        private Result<void> ItemWriter(ZoneObject objBase)
        {
            Item object = (Item)objBase;
            Try!(ObjectWriter(object));

            if (object.ItemType.Enabled)
            {
                Try!(_writer.WriteStringProperty("item_type", object.ItemType.Value));
            }

            //Nanoforge loads both "respawn" and "respawns" into this field, but only exports "respawns". The game will load either into the same field internally.
            if (object.Respawns.Enabled)
            {
                Try!(_writer.WriteBoolProperty("respawns", object.Respawns.Value));
            }

            if (object.Preplaced.Enabled)
            {
                Try!(_writer.WriteBoolProperty("preplaced", object.Preplaced.Value));
            }

            return .Ok;
        }

        //weapon : item
        private Result<void> WeaponWriter(ZoneObject objBase)
        {
            Weapon object = (Weapon)objBase;
            Try!(ItemWriter(object));

            Try!(_writer.WriteStringProperty("weapon_type", object.WeaponType));

            return .Ok;
        }

        //ladder : object
        private Result<void> LadderWriter(ZoneObject objBase)
        {
            Ladder object = (Ladder)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteI32Property("ladder_rungs", object.LadderRungs));

            if (object.LadderEnabled.Enabled)
            {
                Try!(_writer.WriteBoolProperty("ladder_enabled", object.LadderEnabled.Value));
            }

            return .Ok;
        }

        //obj_light : object
        private Result<void> ObjLightWriter(ZoneObject objBase)
        {
            ObjLight object = (ObjLight)objBase;
            Try!(ObjectWriter(object));

            if (object.AttenuationStart.Enabled)
            {
                Try!(_writer.WriteFloatProperty("atten_start", object.AttenuationStart.Value));
            }

            if (object.AttenuationEnd.Enabled)
            {
                Try!(_writer.WriteFloatProperty("atten_end", object.AttenuationEnd.Value));
            }

            if (object.AttenuationRange.Enabled)
            {
                Try!(_writer.WriteFloatProperty("atten_range", object.AttenuationRange.Value));
            }

            if (object.LightFlags.Enabled)
            {
                Try!(_writer.WriteFlagsString<ObjLight.ObjLightFlags>("light_flags", object.LightFlags.Value));
            }

            if (object.LightType.Enabled)
            {
                Try!(_writer.WriteEnum<ObjLight.LightTypeEnum>("type_enum", object.LightType.Value));
            }

            Try!(_writer.WriteVec3Property("clr_orig", object.Color));

            if (object.HotspotSize.Enabled)
            {
                Try!(_writer.WriteFloatProperty("hotspot_size", object.HotspotSize.Value));
            }

            if (object.HotspotFalloffSize.Enabled)
            {
                Try!(_writer.WriteFloatProperty("hotspot_falloff_size", object.HotspotFalloffSize.Value));
            }

            Try!(_writer.WriteVec3Property("min_clip", object.MinClip));

            Try!(_writer.WriteVec3Property("max_clip", object.MaxClip));

            return .Ok;
        }

        //multi_object_marker : object
        private Result<void> MultiObjectMarkerWriter(ZoneObject objBase)
        {
            MultiMarker object = (MultiMarker)objBase;
            Try!(ObjectWriter(object));

            Try!(_writer.WriteBoundingBoxProperty("bb", object.LocalBBox));

            Try!(_writer.WriteEnum<MultiMarker.MultiMarkerType>("marker_type", object.MarkerType));

            Try!(_writer.WriteEnum<Team>("mp_team", object.MpTeam));

            Try!(_writer.WriteI32Property("priority", object.Priority));

            if (object.BackpackType.Enabled)
            {
                Try!(_writer.WriteStringProperty("backpack_type", object.BackpackType.Value));
            }

            if (object.NumBackpacks.Enabled)
            {
                Try!(_writer.WriteI32Property("num_backpacks", object.NumBackpacks.Value));
            }

            if (object.RandomBackpacks.Enabled)
            {
                Try!(_writer.WriteBoolProperty("random_backpacks", object.RandomBackpacks.Value));
            }

            if (object.Group.Enabled)
            {
                Try!(_writer.WriteI32Property("group", object.Group.Value));
            }

            return .Ok;
        }

        //multi_object_flag : item
        private Result<void> MultiObjectFlagWriter(ZoneObject objBase)
        {
            MultiFlag object = (MultiFlag)objBase;
            Try!(ItemWriter(object));

            Try!(_writer.WriteEnum<Team>("mp_team", object.MpTeam));

            return .Ok;
        }

        //navpoint : object
        private Result<void> NavPointWriter(ZoneObject objBase)
        {
            NavPoint object = (NavPoint)objBase;
            Try!(ObjectWriter(object));

            if (object.NavpointData.Enabled && object.NavpointData.Value != null)
            {
                Try!(_writer.WriteBuffer("navpoint_data", object.NavpointData.Value));
            }
            else
            {
                if (object.NavpointType.Enabled)
                {
                    //Either nav_type or navpoint_type could be used here
                    Try!(_writer.WriteU8Property("nav_type", (u8)object.NavpointType.Value));
                }

                if (object.OuterRadius.Enabled)
                {
                    Try!(_writer.WriteFloatProperty("outer_radius", object.OuterRadius.Value));
                }

                if (object.SpeedLimit.Enabled)
                {
                    Try!(_writer.WriteFloatProperty("speed_limit", object.SpeedLimit.Value));
                }

                if (object.NavpointType.Enabled && object.NavpointType.Value != .Patrol)
                {
                    if (object.DontFollowRoad.Enabled)
                    {
                        Try!(_writer.WriteBoolProperty("dont_follow_roads", object.DontFollowRoad.Value));
                    }

                    if (object.IgnoreLanes.Enabled)
                    {
                        Try!(_writer.WriteBoolProperty("ignore_lanes", object.IgnoreLanes.Value));
                    }
                }
            }

            if (object.Links.Enabled && object.Links.Value != null)
            {
                Try!(_writer.WriteBuffer("obj_links", object.Links.Value));
            }
            else if (object.NavLinks.Enabled && object.NavLinks.Value != null)
            {
                Try!(_writer.WriteBuffer("navlinks", object.NavLinks.Value));
            }

            if (object.NavpointType.Enabled && object.NavpointType.Value == .RoadCheckpoint)
            {
                if (object.NavOrient.Enabled)
                {
                    Try!(_writer.WriteMat3Property("nav_orient", object.NavOrient.Value));
                }
            }

            return .Ok;
        }

        //cover_node : object
        private Result<void> CoverNodeWriter(ZoneObject objBase)
        {
            CoverNode object = (CoverNode)objBase;
            Try!(ObjectWriter(object));
            
            if (object.CovernodeData.Enabled && object.CovernodeData.Value != null)
            {
                Try!(_writer.WriteBuffer("covernode_data", object.CovernodeData.Value));
            }
            else if (object.OldCovernodeData.Enabled && object.OldCovernodeData.Value != null)
            {
                Try!(_writer.WriteBuffer("cnp", object.OldCovernodeData.Value));
            }

            if (object.DefensiveAngleLeft.Enabled)
            {
                Try!(_writer.WriteFloatProperty("def_angle_left", object.DefensiveAngleLeft.Value));
            }

            if (object.DefensiveAngleRight.Enabled)
            {
                Try!(_writer.WriteFloatProperty("def_angle_right", object.DefensiveAngleRight.Value));
            }

            if (object.OffensiveAngleLeft.Enabled)
            {
                Try!(_writer.WriteFloatProperty("off_angle_left", object.OffensiveAngleLeft.Value));
            }

            if (object.AngleLeft.Enabled)
            {
                Try!(_writer.WriteFloatProperty("angle_left", object.AngleLeft.Value));
            }

            if (object.OffensiveAngleRight.Enabled)
            {
                Try!(_writer.WriteFloatProperty("off_angle_right", object.OffensiveAngleRight.Value));
            }

            if (object.AngleRight.Enabled)
            {
                Try!(_writer.WriteFloatProperty("angle_right", object.AngleRight.Value));
            }

            if (object.CoverNodeFlags.Enabled)
            {
                Try!(_writer.WriteU16Property("binary_flags", (u16)object.CoverNodeFlags.Value));
            }

            if (object.Stance.Enabled)
            {
                Try!(_writer.WriteStringProperty("stance", object.Stance.Value));
            }

            if (object.FiringFlags.Enabled)
            {
                Try!(_writer.WriteStringProperty("firing_flags", object.FiringFlags.Value));
            }

            return .Ok;
        }

        //constraint : object
        private Result<void> ConstraintWriter(ZoneObject objBase)
        {
            RfgConstraint object = (RfgConstraint)objBase;
            Try!(ObjectWriter(object));

            if (object.Template.Enabled)
            {
                Try!(_writer.WriteConstraintTemplate("template", object.Template.Value));
            }

            if (object.HostHandle.Enabled)
            {
                Try!(_writer.WriteU32Property("host_handle", object.HostHandle.Value));
            }

            if (object.ChildHandle.Enabled)
            {
                Try!(_writer.WriteU32Property("child_handle", object.ChildHandle.Value));
            }

            if (object.HostIndex.Enabled)
            {
                Try!(_writer.WriteU32Property("host_index", object.HostIndex.Value));
            }

            if (object.ChildIndex.Enabled)
            {
                Try!(_writer.WriteU32Property("child_index", object.ChildIndex.Value));
            }

            if (object.HostHavokAltIndex.Enabled)
            {
                Try!(_writer.WriteU32Property("host_hk_alt_index", object.HostHavokAltIndex.Value));
            }

            if (object.ChildHavokAltIndex.Enabled)
            {
                Try!(_writer.WriteU32Property("child_hk_alt_index", object.ChildHavokAltIndex.Value));
            }

            return .Ok;
        }

        //object_action_node : object
        private Result<void> ActionNodeWriter(ZoneObject objBase)
        {
            ActionNode object = (ActionNode)objBase;
            Try!(ObjectWriter(object));

            if (object.ActionNodeType.Enabled)
            {
                Try!(_writer.WriteStringProperty("animation_type", object.ActionNodeType.Value));
            }

            if (object.HighPriority.Enabled)
            {
                Try!(_writer.WriteBoolProperty("high_priority", object.HighPriority.Value));
            }

            if (object.InfiniteDuration.Enabled)
            {
                Try!(_writer.WriteBoolProperty("infinite_duration", object.InfiniteDuration.Value));
            }

            if (object.DisabledBy.Enabled)
            {
                Try!(_writer.WriteI32Property("disabled", object.DisabledBy.Value));
            }

            if (object.RunTo.Enabled)
            {
                Try!(_writer.WriteBoolProperty("run_to", object.RunTo.Value));
            }

            if (object.OuterRadius.Enabled)
            {
                Try!(_writer.WriteFloatProperty("outer_radius", object.OuterRadius.Value));
            }

            if (object.ObjLinks.Enabled && object.ObjLinks.Value != null)
            {
                Try!(_writer.WriteBuffer("obj_links", object.ObjLinks.Value));
            }
            else if (object.Links.Enabled && object.Links.Value != null)
            {
                Try!(_writer.WriteBuffer("links", object.Links.Value));
            }

            return .Ok;
        }
	}
}