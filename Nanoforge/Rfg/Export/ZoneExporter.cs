using System;
using System.Collections.Generic;
using System.IO;
using Nanoforge.Misc;
using RFGM.Formats.Math;
using Serilog;

namespace Nanoforge.Rfg.Export;

public class ZoneExporter
{
    private Dictionary<Type, ZoneObjectWriter> _objectWriters = new();
    private ZoneFileWriter _writer = new();

    delegate void ZoneObjectWriter(ZoneObject objBase);

    //Handle based object relations used by RFG. Only used by Nanoforge during export since it converts the handles to ZoneObject references on import.
    public class RfgObjectRelations
    {
        public uint Parent;
        public uint Child;
        public uint Sibling;
    }

    public ZoneExporter()
    {
        _objectWriters[typeof(ZoneObject)] = ObjectWriter;
        _objectWriters[typeof(ObjZone)] = ObjZoneWriter;
        _objectWriters[typeof(ObjectBoundingBox)] = ObjBoundingBoxWriter;
        _objectWriters[typeof(ObjectDummy)] = ObjectDummyWriter;
        _objectWriters[typeof(PlayerStart)] = PlayerStartWriter;
        _objectWriters[typeof(TriggerRegion)] = TriggerRegionWriter;
        _objectWriters[typeof(ObjectMover)] = ObjectMoverWriter;
        _objectWriters[typeof(GeneralMover)] = GeneralMoverWriter;
        _objectWriters[typeof(RfgMover)] = RfgMoverWriter;
        _objectWriters[typeof(ShapeCutter)] = ShapeCutterWriter;
        _objectWriters[typeof(ObjectEffect)] = ObjectEffectWriter;
        _objectWriters[typeof(Item)] = ItemWriter;
        _objectWriters[typeof(Weapon)] = WeaponWriter;
        _objectWriters[typeof(Ladder)] = LadderWriter;
        _objectWriters[typeof(ObjLight)] = ObjLightWriter;
        _objectWriters[typeof(MultiMarker)] = MultiObjectMarkerWriter;
        _objectWriters[typeof(MultiFlag)] = MultiObjectFlagWriter;
        _objectWriters[typeof(NavPoint)] = NavPointWriter;
        _objectWriters[typeof(CoverNode)] = CoverNodeWriter;
        _objectWriters[typeof(RfgConstraint)] = ConstraintWriter;
        _objectWriters[typeof(ActionNode)] = ActionNodeWriter;
    }

    public bool Export(Zone zone, string outputPath, bool persistent)
    {
        try
        {
            //Collect objects we're writing to this file
            List<ZoneObject> objects = new();
            foreach (ZoneObject obj in zone.Objects)
            {
                if (persistent && !obj.Persistent)
                    continue;

                objects.Add(obj);
            }

            //Check if object limit is exceeded. This limit is based on the max object count of the relation data section.
            //The necessity of that section is still in question, so it may be possible to exceed this. Just playing it safe for now. No zone in the SP map exceeds 3000 objects
            //TODO: See if this limit can be exceeded. Likely depends on how necessary the relation data hash table is
            const int maxObjects = 7280;
            if (objects.Count > maxObjects)
            {
                string error = $"Failed to export {zone.Name}. Zone has {objects.Count} objects. This exceeds the maximum per zone object limit of {maxObjects}";
                Log.Error(error);
                throw new Exception(error);
            }

            //NOTE: This collects relations on all objects in the zone. While some objects get put in the persistent zone file its still all one zone
            //Update RFG relative handles. For convenience NF uses object references instead of RFGs u32 handles. Need to generate them again for the game to use though.
            //Collect parents and children
            Dictionary<ZoneObject, RfgObjectRelations> relations = new();
            foreach (ZoneObject obj in zone.Objects)
            {
                RfgObjectRelations relatives = new()
                {
                    Parent = obj.Parent?.Handle ?? ZoneObject.NullHandle,
                    Child = (obj.Children.Count > 0) ? obj.Children[0].Handle : ZoneObject.NullHandle,
                    Sibling = ZoneObject.NullHandle
                };
                relations[obj] = relatives;
            }

            //Update sibling values. Basically a linked list of u32 handles. Easier to do as a second loop since we know all objects are in the relations dictionary
            foreach (ZoneObject obj in zone.Objects)
            {
                if (obj.Children.Count > 1)
                {
                    for (int i = 0; i < obj.Children.Count - 1; i++)
                    {
                        ZoneObject current = obj.Children[i];
                        ZoneObject next = obj.Children[i + 1];
                        relations[current].Sibling = next.Handle;
                    }
                }
            }

            using FileStream stream = new FileStream(outputPath, FileMode.Create, FileAccess.Write);
            _writer = new();
            _writer.BeginFile(stream, zone.District, zone.DistrictFlags, zone.DistrictHash);

            foreach (ZoneObject obj in objects)
            {
                RfgObjectRelations relatives = relations[obj];
                _writer.BeginObject(obj.Classname, obj.Handle, obj.Num, (ushort)obj.Flags, obj.BBox.Min, obj.BBox.Max, relatives.Parent, relatives.Sibling, relatives.Child);

                //Run typed object writer delegate
                if (!_objectWriters.ContainsKey(obj.GetType()))
                {
                    throw new Exception($"Zone object writer not defined for type {obj.GetType().FullName}");
                }

                _objectWriters[obj.GetType()].Invoke(obj);

                _writer.EndObject();
            }

            _writer.EndFile();
            return true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to export {zone}", zone);
            return false;
        }
    }

    //object - base class inherited by all other zone object classes
    private void ObjectWriter(ZoneObject objBase)
    {
        if (objBase.Orient.Enabled)
        {
            _writer.WritePositionOrientProperty(objBase.Position, objBase.Orient.Value.ToMatrix3x3());
        }
        else
        {
            _writer.WriteVec3Property("just_pos", objBase.Position);
        }

        if (objBase.RfgDisplayName.Enabled)
        {
            _writer.WriteStringProperty("display_name", objBase.RfgDisplayName);
        }
    }

    //obj_zone : object
    private void ObjZoneWriter(ZoneObject objBase)
    {
        ObjZone obj = (ObjZone)objBase;
        ObjectWriter(obj);

        if (obj.AmbientSpawn.Enabled)
        {
            _writer.WriteStringProperty("ambient_spawn", obj.AmbientSpawn.Value);
        }

        if (obj.SpawnResource.Enabled)
        {
            _writer.WriteStringProperty("spawn_resource", obj.SpawnResource.Value);
        }

        _writer.WriteStringProperty("terrain_file_name", obj.TerrainFileName);

        if (obj.WindMinSpeed.Enabled)
        {
            _writer.WriteFloatProperty("wind_min_speed", obj.WindMinSpeed.Value);
        }

        if (obj.WindMaxSpeed.Enabled)
        {
            _writer.WriteFloatProperty("wind_max_speed", obj.WindMaxSpeed.Value);
        }
    }

    //object_bounding_box : object
    private void ObjBoundingBoxWriter(ZoneObject objBase)
    {
        ObjectBoundingBox obj = (ObjectBoundingBox)objBase;
        ObjectWriter(obj);

        _writer.WriteBoundingBoxProperty("bb", obj.LocalBBox);

        if (obj.BBType.Enabled)
        {
            _writer.WriteEnum<ObjectBoundingBox.BoundingBoxType>("bounding_box_type", obj.BBType.Value);
        }
    }

    //object_dummy : object
    private void ObjectDummyWriter(ZoneObject objBase)
    {
        ObjectDummy obj = (ObjectDummy)objBase;
        ObjectWriter(obj);

        if (!DataHelper.ToRfgName<ObjectDummy.DummyTypeEnum>(obj.DummyType, out string rfgName))
        {
            throw new Exception($"Failed to convert dummy type '{obj.DummyType}' to RfgName");
        }
        _writer.WriteStringProperty("dummy_type", rfgName);
    }

    //player_start : object
    private void PlayerStartWriter(ZoneObject objBase)
    {
        PlayerStart obj = (PlayerStart)objBase;
        ObjectWriter(obj);

        _writer.WriteBoolProperty("indoor", obj.Indoor);

        if (obj.MpTeam.Enabled)
        {
            _writer.WriteEnum<RfgTeam>("mp_team", obj.MpTeam.Value);
        }

        _writer.WriteBoolProperty("initial_spawn", obj.InitialSpawn);

        _writer.WriteBoolProperty("respawn", obj.Respawn);

        _writer.WriteBoolProperty("checkpoint_respawn", obj.CheckpointRespawn);

        _writer.WriteBoolProperty("activity_respawn", obj.ActivityRespawn);

        if (obj.MissionInfo.Enabled)
        {
            _writer.WriteStringProperty("mission_info", obj.MissionInfo.Value);
        }
    }

    //trigger_region : object
    private void TriggerRegionWriter(ZoneObject objBase)
    {
        TriggerRegion obj = (TriggerRegion)objBase;
        ObjectWriter(obj);

        if (obj.TriggerShape.Enabled)
        {
            _writer.WriteEnum<TriggerRegion.Shape>("trigger_shape", obj.TriggerShape.Value);
        }

        _writer.WriteBoundingBoxProperty("bb", obj.LocalBBox);

        _writer.WriteFloatProperty("outer_radius", obj.OuterRadius);

        _writer.WriteBoolProperty("enabled", obj.Enabled);

        if (obj.RegionType.Enabled)
        {
            _writer.WriteEnum<TriggerRegion.RegionTypeEnum>("region_type", obj.RegionType.Value);
        }

        if (obj.KillType.Enabled)
        {
            _writer.WriteEnum<TriggerRegion.KillTypeEnum>("region_kill_type", obj.KillType.Value);
        }

        if (obj.TriggerFlags.Enabled)
        {
            _writer.WriteFlagsString<TriggerRegion.TriggerRegionFlags>("trigger_flags", obj.TriggerFlags.Value);
        }
    }

    //object_mover : object
    private void ObjectMoverWriter(ZoneObject objBase)
    {
        ObjectMover obj = (ObjectMover)objBase;
        ObjectWriter(obj);

        _writer.WriteFlagsString<ObjectMover.BuildingTypeFlagsEnum>("building_type", obj.BuildingType);

        _writer.WriteU32Property("dest_checksum", obj.DestructionChecksum);

        if (obj.GameplayProps.Enabled)
        {
            _writer.WriteStringProperty("gameplay_props", obj.GameplayProps.Value);
        }

        //_writer.WriteU32Property("chunk_flags", obj.InternalFlags);

        if (obj.ChunkFlags.Enabled)
        {
            _writer.WriteFlagsString<ObjectMover.ChunkFlagsEnum>("chunk_flags", obj.ChunkFlags.Value);
        }

        if (obj.Dynamic.Enabled)
        {
            _writer.WriteBoolProperty("dynamic_object", obj.Dynamic.Value);
        }

        if (obj.ChunkUID.Enabled)
        {
            _writer.WriteU32Property("chunk_uid", obj.ChunkUID.Value);
        }

        if (obj.Props.Enabled)
        {
            _writer.WriteStringProperty("props", obj.Props.Value);
        }

        if (obj.ChunkName.Enabled)
        {
            _writer.WriteStringProperty("chunk_name", obj.ChunkName.Value);
        }

        _writer.WriteU32Property("uid", obj.DestroyableUID);

        if (obj.ShapeUID.Enabled)
        {
            _writer.WriteU32Property("shape_uid", obj.ShapeUID.Value);
        }

        if (obj.Team.Enabled)
        {
            _writer.WriteEnum<RfgTeam>("team", obj.Team.Value);
        }

        if (obj.Control.Enabled)
        {
            _writer.WriteFloatProperty("control", obj.Control.Value);
        }
    }

    //general_mover : object_mover
    private void GeneralMoverWriter(ZoneObject objBase)
    {
        GeneralMover obj = (GeneralMover)objBase;
        ObjectMoverWriter(obj);

        _writer.WriteU32Property("gm_flags", obj.GmFlags);

        if (obj.OriginalObject.Enabled)
        {
            _writer.WriteU32Property("original_object", obj.OriginalObject.Value);
        }

        if (obj.CollisionType.Enabled)
        {
            _writer.WriteU32Property("ctype", obj.CollisionType.Value);
        }

        if (obj.Idx.Enabled)
        {
            _writer.WriteU32Property("idx", obj.Idx.Value);
        }

        if (obj.MoveType.Enabled)
        {
            _writer.WriteU32Property("mtype", obj.MoveType.Value);
        }

        if (obj.DestructionUID.Enabled)
        {
            _writer.WriteU32Property("destruct_uid", obj.DestructionUID.Value);
        }

        if (obj.Hitpoints.Enabled)
        {
            _writer.WriteI32Property("hitpoints", obj.Hitpoints.Value);
        }

        if (obj.DislodgeHitpoints.Enabled)
        {
            _writer.WriteI32Property("dislodge_hitpoints", obj.DislodgeHitpoints.Value);
        }
    }

    //rfg_mover : object_mover
    private void RfgMoverWriter(ZoneObject objBase)
    {
        RfgMover obj = (RfgMover)objBase;
        ObjectMoverWriter(obj);

        _writer.WriteU32Property("mtype", (uint)obj.MoveType); //One of the rare few enums we write into an integer instead of a string

        if (obj.DamagePercent.Enabled)
        {
            _writer.WriteFloatProperty("damage_percent", obj.DamagePercent.Value);
        }

        if (obj.GameDestroyedPercentage.Enabled)
        {
            _writer.WriteFloatProperty("game_destroyed_pct", obj.GameDestroyedPercentage.Value);
        }

        if (obj.WorldAnchors.Enabled && obj.WorldAnchors.Value != null)
        {
            _writer.WriteBuffer("world_anchors", obj.WorldAnchors.Value);
        }

        if (obj.DynamicLinks.Enabled && obj.DynamicLinks.Value != null)
        {
            _writer.WriteBuffer("dynamic_links", obj.DynamicLinks.Value);
        }
    }

    //shape_cutter : object
    private void ShapeCutterWriter(ZoneObject objBase)
    {
        ShapeCutter obj = (ShapeCutter)objBase;
        ObjectWriter(obj);

        if (obj.ShapeCutterFlags.Enabled)
        {
            _writer.WriteI16Property("flags", obj.ShapeCutterFlags.Value);
        }

        if (obj.OuterRadius.Enabled)
        {
            _writer.WriteFloatProperty("outer_radius", obj.OuterRadius.Value);
        }

        if (obj.Source.Enabled)
        {
            _writer.WriteU32Property("source", obj.Source.Value);
        }

        if (obj.Owner.Enabled)
        {
            _writer.WriteU32Property("owner", obj.Owner.Value);
        }

        if (obj.ExplosionId.Enabled)
        {
            _writer.WriteU8Property("exp_info", obj.ExplosionId.Value);
        }
    }
    
    //object_effect : object
    private void ObjectEffectWriter(ZoneObject objBase)
    {
        ObjectEffect obj = (ObjectEffect)objBase;
        ObjectWriter(obj);

        if (obj.EffectType.Enabled)
        {
            _writer.WriteStringProperty("effect_type", obj.EffectType.Value);
        }

        if (obj.SoundAlr.Enabled)
        {
            _writer.WriteStringProperty("sound_alr", obj.SoundAlr.Value);
        }

        if (obj.Sound.Enabled)
        {
            _writer.WriteStringProperty("sound", obj.Sound.Value);
        }

        if (obj.Visual.Enabled)
        {
            _writer.WriteStringProperty("visual", obj.Visual.Value);
        }

        if (obj.Looping.Enabled)
        {
            _writer.WriteBoolProperty("looping", obj.Looping.Value);
        }
    }

    //item : object
    private void ItemWriter(ZoneObject objBase)
    {
        Item obj = (Item)objBase;
        ObjectWriter(obj);

        if (obj.ItemType.Enabled)
        {
            _writer.WriteStringProperty("item_type", obj.ItemType.Value);
        }

        //Nanoforge loads both "respawn" and "respawns" into this field, but only exports "respawns". The game will load either into the same field internally.
        if (obj.Respawns.Enabled)
        {
            _writer.WriteBoolProperty("respawns", obj.Respawns.Value);
        }

        if (obj.Preplaced.Enabled)
        {
            _writer.WriteBoolProperty("preplaced", obj.Preplaced.Value);
        }

    }

    //weapon : item
    private void WeaponWriter(ZoneObject objBase)
    {
        Weapon obj = (Weapon)objBase;
        ItemWriter(obj);

        _writer.WriteStringProperty("weapon_type", obj.WeaponType);
    }

    //ladder : object
    private void LadderWriter(ZoneObject objBase)
    {
        Ladder obj = (Ladder)objBase;
        ObjectWriter(obj);

        _writer.WriteI32Property("ladder_rungs", obj.LadderRungs);

        if (obj.LadderEnabled.Enabled)
        {
            _writer.WriteBoolProperty("ladder_enabled", obj.LadderEnabled.Value);
        }
    }

    //obj_light : object
    private void ObjLightWriter(ZoneObject objBase)
    {
        ObjLight obj = (ObjLight)objBase;
        ObjectWriter(obj);

        if (obj.AttenuationStart.Enabled)
        {
            _writer.WriteFloatProperty("atten_start", obj.AttenuationStart.Value);
        }

        if (obj.AttenuationEnd.Enabled)
        {
            _writer.WriteFloatProperty("atten_end", obj.AttenuationEnd.Value);
        }

        if (obj.AttenuationRange.Enabled)
        {
            _writer.WriteFloatProperty("atten_range", obj.AttenuationRange.Value);
        }

        if (obj.LightFlags.Enabled)
        {
            _writer.WriteFlagsString<ObjLight.ObjLightFlags>("light_flags", obj.LightFlags.Value);
        }

        if (obj.LightType.Enabled)
        {
            _writer.WriteEnum<ObjLight.LightTypeEnum>("type_enum", obj.LightType.Value);
        }

        _writer.WriteVec3Property("clr_orig", obj.Color);

        if (obj.HotspotSize.Enabled)
        {
            _writer.WriteFloatProperty("hotspot_size", obj.HotspotSize.Value);
        }

        if (obj.HotspotFalloffSize.Enabled)
        {
            _writer.WriteFloatProperty("hotspot_falloff_size", obj.HotspotFalloffSize.Value);
        }

        _writer.WriteVec3Property("min_clip", obj.MinClip);

        _writer.WriteVec3Property("max_clip", obj.MaxClip);
    }

    //multi_object_marker : object
    private void MultiObjectMarkerWriter(ZoneObject objBase)
    {
        MultiMarker obj = (MultiMarker)objBase;
        ObjectWriter(obj);

        _writer.WriteBoundingBoxProperty("bb", obj.LocalBBox);

        _writer.WriteEnum<MultiMarker.MultiMarkerType>("marker_type", obj.MarkerType);

        _writer.WriteEnum<RfgTeam>("mp_team", obj.MpTeam);

        _writer.WriteI32Property("priority", obj.Priority);

        if (obj.BackpackType.Enabled)
        {
            _writer.WriteStringProperty("backpack_type", obj.BackpackType.Value);
        }

        if (obj.NumBackpacks.Enabled)
        {
            _writer.WriteI32Property("num_backpacks", obj.NumBackpacks.Value);
        }

        if (obj.RandomBackpacks.Enabled)
        {
            _writer.WriteBoolProperty("random_backpacks", obj.RandomBackpacks.Value);
        }

        if (obj.Group.Enabled)
        {
            _writer.WriteI32Property("group", obj.Group.Value);
        }
    }

    //multi_object_flag : item
    private void MultiObjectFlagWriter(ZoneObject objBase)
    {
        MultiFlag obj = (MultiFlag)objBase;
        ItemWriter(obj);

        _writer.WriteEnum<RfgTeam>("mp_team", obj.MpTeam);
    }

    //navpoint : object
    private void NavPointWriter(ZoneObject objBase)
    {
        NavPoint obj = (NavPoint)objBase;
        ObjectWriter(obj);

        if (obj.NavpointData.Enabled && obj.NavpointData.Value != null)
        {
            _writer.WriteBuffer("navpoint_data", obj.NavpointData.Value);
        }
        else
        {
            if (obj.NavpointType.Enabled)
            {
                //Either nav_type or navpoint_type could be used here
                _writer.WriteU8Property("nav_type", (byte)obj.NavpointType.Value);
            }

            if (obj.OuterRadius.Enabled)
            {
                _writer.WriteFloatProperty("outer_radius", obj.OuterRadius.Value);
            }

            if (obj.SpeedLimit.Enabled)
            {
                _writer.WriteFloatProperty("speed_limit", obj.SpeedLimit.Value);
            }

            if (obj.NavpointType.Enabled && obj.NavpointType.Value != NavPoint.NavpointTypeEnum.Patrol)
            {
                if (obj.DontFollowRoad.Enabled)
                {
                    _writer.WriteBoolProperty("dont_follow_roads", obj.DontFollowRoad.Value);
                }

                if (obj.IgnoreLanes.Enabled)
                {
                    _writer.WriteBoolProperty("ignore_lanes", obj.IgnoreLanes.Value);
                }
            }
        }

        if (obj.Links.Enabled && obj.Links.Value != null)
        {
            _writer.WriteBuffer("obj_links", obj.Links.Value);
        }
        else if (obj.NavLinks.Enabled && obj.NavLinks.Value != null)
        {
            _writer.WriteBuffer("navlinks", obj.NavLinks.Value);
        }

        if (obj.NavpointType.Enabled && obj.NavpointType.Value == NavPoint.NavpointTypeEnum.RoadCheckpoint)
        {
            if (obj.NavOrient.Enabled)
            {
                _writer.WriteMat3Property("nav_orient", obj.NavOrient.Value.ToMatrix3x3());
            }
        }
    }

    //cover_node : object
    private void CoverNodeWriter(ZoneObject objBase)
    {
        CoverNode obj = (CoverNode)objBase;
        ObjectWriter(obj);
            
        if (obj.CovernodeData.Enabled && obj.CovernodeData.Value != null)
        {
            _writer.WriteBuffer("covernode_data", obj.CovernodeData.Value);
        }
        else if (obj.OldCovernodeData.Enabled && obj.OldCovernodeData.Value != null)
        {
            _writer.WriteBuffer("cnp", obj.OldCovernodeData.Value);
        }

        if (obj.DefensiveAngleLeft.Enabled)
        {
            _writer.WriteFloatProperty("def_angle_left", obj.DefensiveAngleLeft.Value);
        }

        if (obj.DefensiveAngleRight.Enabled)
        {
            _writer.WriteFloatProperty("def_angle_right", obj.DefensiveAngleRight.Value);
        }

        if (obj.OffensiveAngleLeft.Enabled)
        {
            _writer.WriteFloatProperty("off_angle_left", obj.OffensiveAngleLeft.Value);
        }

        if (obj.AngleLeft.Enabled)
        {
            _writer.WriteFloatProperty("angle_left", obj.AngleLeft.Value);
        }

        if (obj.OffensiveAngleRight.Enabled)
        {
            _writer.WriteFloatProperty("off_angle_right", obj.OffensiveAngleRight.Value);
        }

        if (obj.AngleRight.Enabled)
        {
            _writer.WriteFloatProperty("angle_right", obj.AngleRight.Value);
        }

        if (obj.CoverNodeFlags.Enabled)
        {
            _writer.WriteU16Property("binary_flags", (ushort)obj.CoverNodeFlags.Value);
        }

        if (obj.Stance.Enabled)
        {
            _writer.WriteStringProperty("stance", obj.Stance.Value);
        }

        if (obj.FiringFlags.Enabled)
        {
            _writer.WriteStringProperty("firing_flags", obj.FiringFlags.Value);
        }

    }

    //constraint : object
    private void ConstraintWriter(ZoneObject objBase)
    {
        RfgConstraint obj = (RfgConstraint)objBase;
        ObjectWriter(obj);

        if (obj.Template.Enabled && obj.Template.Value != null)
        {
            _writer.WriteConstraintTemplate("template", obj.Template.Value);
        }

        if (obj.HostHandle.Enabled)
        {
            _writer.WriteU32Property("host_handle", obj.HostHandle.Value);
        }

        if (obj.ChildHandle.Enabled)
        {
            _writer.WriteU32Property("child_handle", obj.ChildHandle.Value);
        }

        if (obj.HostIndex.Enabled)
        {
            _writer.WriteU32Property("host_index", obj.HostIndex.Value);
        }

        if (obj.ChildIndex.Enabled)
        {
            _writer.WriteU32Property("child_index", obj.ChildIndex.Value);
        }

        if (obj.HostHavokAltIndex.Enabled)
        {
            _writer.WriteU32Property("host_hk_alt_index", obj.HostHavokAltIndex.Value);
        }

        if (obj.ChildHavokAltIndex.Enabled)
        {
            _writer.WriteU32Property("child_hk_alt_index", obj.ChildHavokAltIndex.Value);
        }
        
    }

    //object_action_node : object
    private void ActionNodeWriter(ZoneObject objBase)
    {
        ActionNode obj = (ActionNode)objBase;
        ObjectWriter(obj);

        if (obj.ActionNodeType.Enabled)
        {
            _writer.WriteStringProperty("animation_type", obj.ActionNodeType.Value);
        }

        if (obj.HighPriority.Enabled)
        {
            _writer.WriteBoolProperty("high_priority", obj.HighPriority.Value);
        }

        if (obj.InfiniteDuration.Enabled)
        {
            _writer.WriteBoolProperty("infinite_duration", obj.InfiniteDuration.Value);
        }

        if (obj.DisabledBy.Enabled)
        {
            _writer.WriteI32Property("disabled", obj.DisabledBy.Value);
        }

        if (obj.RunTo.Enabled)
        {
            _writer.WriteBoolProperty("run_to", obj.RunTo.Value);
        }

        if (obj.OuterRadius.Enabled)
        {
            _writer.WriteFloatProperty("outer_radius", obj.OuterRadius.Value);
        }

        if (obj.ObjLinks.Enabled && obj.ObjLinks.Value != null)
        {
            _writer.WriteBuffer("obj_links", obj.ObjLinks.Value);
        }
        else if (obj.Links.Enabled && obj.Links.Value != null)
        {
            _writer.WriteBuffer("links", obj.Links.Value);
        }

    }
}