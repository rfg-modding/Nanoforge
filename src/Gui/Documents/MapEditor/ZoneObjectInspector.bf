using Nanoforge.Rfg;
using Nanoforge.App;
using ImGui;
using System;
using System.Collections;
using Nanoforge.FileSystem;
using Common;
using Nanoforge.Misc;
using Xml_Beef;
using System.IO;
using System.Reflection;
using Common.Math;
using static Nanoforge.Rfg.MultiMarker;
namespace Nanoforge.Gui.Documents.MapEditor;

//Inspector for a specific zone object type. Implemented as a separate class to keep a clean separation between data and UI logic.
public interface IZoneObjectInspector<T> where T : ZoneObject
{
    public static void Draw(App app, Gui gui, T obj);
}

[AttributeUsage(.Class, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .All, AlwaysIncludeUser = .IncludeAllMethods | .AssumeInstantiated)]
public struct RegisterInspectorAttribute : Attribute
{
    public readonly Type ZoneObjectType;

    public this(Type inspectorType)
    {
        ZoneObjectType = inspectorType;
    }
}

public static class BaseZoneObjectInspector : IZoneObjectInspector<ZoneObject>
{
    private static append BitflagComboBox<ZoneObject.Flags> _flagsCombo = .("Flags",
	    nameOverrides: new .(
		    (.SpawnInAnarchy, "Spawn in Anarchy"),
		    (.SpawnInTeamAnarchy, "Spawn in Team Anarchy"),
            (.SpawnInCTF, "Spawn in CTF"),
            (.SpawnInSiege, "Spawn in Siege"),
            (.SpawnInDamageControl, "Spawn in Damage Control"),
            (.SpawnInBagman, "Spawn in Bagman"),
            (.SpawnInTeamBagman, "Spawn in Team Bagman"),
            (.SpawnInDemolition, "Spawn in Demolition")
	    ));

    public static void Draw(App app, Gui gui, ZoneObject obj)
    {
        Fonts.FontL.Push();
        ImGui.Text(obj.Classname);
        Fonts.FontL.Pop();
        ImGui.Text(scope $"{obj.Handle}, {obj.Num}");
        ImGui.Separator();

        //TODO: Add description and display name and load/save it in EditorData.xml or equivalent

        if (ImGui.TreeNodeEx("Bounding Box", .FramePadding))
        {
            //TODO: Add extension to Imgui for NF vector types
            ImGui.InputFloat3("Min", ref *(float[3]*)&obj.BBox.Min);
            ImGui.InputFloat3("Max", ref *(float[3]*)&obj.BBox.Max);
            ImGui.TreePop();
        }

        //TODO: Implement relative (parent/child) editor

        if (ImGui.TreeNodeEx("Orientation", .FramePadding))
        {
            //TODO: Add Mat3 editor extension
            ImGui.TooltipOnPrevious("Mat3");
            ImGui.InputFloat3("Right", ref *(float[3]*)&obj.Orient.Vectors[0]);
            ImGui.TooltipOnPrevious("Vec3");
            ImGui.InputFloat3("Up", ref *(float[3]*)&obj.Orient.Vectors[1]);
            ImGui.TooltipOnPrevious("Vec3");
            ImGui.InputFloat3("Forward", ref *(float[3]*)&obj.Orient.Vectors[2]);
            ImGui.TooltipOnPrevious("Vec3");
            ImGui.TreePop();
        }
        else
        {
            ImGui.TooltipOnPrevious("Mat3");
        }

        _flagsCombo.Draw(ref obj.Flags);

        Vec3 initialPos = obj.Position;
        if (ImGui.InputFloat3("Position", ref *(float[3]*)&obj.Position))
        {
            Vec3 delta = obj.Position - initialPos;
            obj.BBox += delta;

            //TODO: Update render chunk position

            bool autoMoveChildren = true;
            if (autoMoveChildren)
            {
                MoveObjectChildrenRecursive(obj, delta);
                //TODO: Auto move children if setting is enabled
                //TODO:    Pass MapEditorDocument in for ease of access
                //TODO:    Implement CVar. Maybe use bon this time for wider type options
            }    
        }

        ImGui.Separator();
    }

    private static void MoveObjectChildrenRecursive(ZoneObject obj, Vec3 delta)
    {
        for (ZoneObject child in obj.Children)
        {
            MoveObjectChildrenRecursive(child, delta);
            child.Position += delta;
            child.BBox += delta;
        }    
    }
}

[RegisterInspector(typeof(ObjZone))]
public static class ObjZoneInspector : IZoneObjectInspector<ObjZone>
{
    private static append XtblComboBox _ambientSpawnCombo = .("Ambient spawn", "ambient_spawn_info.xtbl", "table;ambient_spawn_info;Name");

    public static void Draw(App app, Gui gui, ObjZone obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _ambientSpawnCombo.Draw(obj.AmbientSpawn);
        ImGui.InputText("Spawn resource", obj.SpawnResource);
        ImGui.InputText("Terrain file name", obj.TerrainFileName);
        ImGui.InputFloat("Wind min speed", &obj.WindMinSpeed);
        ImGui.InputFloat("Wind max speed", &obj.WindMaxSpeed);
    }
}

[RegisterInspector(typeof(ObjectBoundingBox))]
public static class ObjectBoundingBoxInspector : IZoneObjectInspector<ObjectBoundingBox>
{
    private static append EnumComboBox<ObjectBoundingBox.BoundingBoxType> _bboxTypeCombo = .("Local BBox Type");

    public static void Draw(App app, Gui gui, ObjectBoundingBox obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        if (ImGui.TreeNodeEx("Local Bounding Box", .FramePadding))
        {
            //TODO: Add extension to Imgui for NF vector types & BBox
            ImGui.InputFloat3("Min", ref *(float[3]*)&obj.LocalBBox.Min);
            ImGui.InputFloat3("Max", ref *(float[3]*)&obj.LocalBBox.Max);
            ImGui.TreePop();
        }
        _bboxTypeCombo.Draw(ref obj.BBType);
    }
}

[RegisterInspector(typeof(ObjectDummy))]
public static class ObjectDummyInspector : IZoneObjectInspector<ObjectDummy>
{
    private static append EnumComboBox<ObjectDummy.DummyType> _dummyTypeCombo = .("Dummy type");

    public static void Draw(App app, Gui gui, ObjectDummy obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _dummyTypeCombo.Draw(ref obj.DummyType);
    }
}

[RegisterInspector(typeof(PlayerStart))]
public static class PlayerStartInspector : IZoneObjectInspector<PlayerStart>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");
    private static append XtblComboBox _missionInfoCombo = .("Mission Info", "missions.xtbl", "table;mission;Name", defaultOption: "none");

    public static void Draw(App app, Gui gui, PlayerStart obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        ImGui.Checkbox("Indoor", &obj.Indoor);
        _teamCombo.Draw(ref obj.MpTeam);
        _missionInfoCombo.Draw(obj.MissionInfo);
        ImGui.Checkbox("Initial spawn", &obj.InitialSpawn);
        ImGui.Checkbox("Respawn", &obj.Respawn);
        ImGui.Checkbox("Checkpoint Respawn", &obj.CheckpointRespawn);
        ImGui.Checkbox("Activity Respawn", &obj.ActivityRespawn);
    }
}

[RegisterInspector(typeof(TriggerRegion))]
public static class TriggerRegionInspector : IZoneObjectInspector<TriggerRegion>
{
    private static append EnumComboBox<TriggerRegion.Shape> _triggerShapeCombo = .("Trigger Shape");
    private static append EnumComboBox<TriggerRegion.RegionTypeEnum> _regionTypeCombo = .("Region Type");
    private static append EnumComboBox<TriggerRegion.KillTypeEnum> _killTypeCombo = .("Kill Type");
    private static append BitflagComboBox<TriggerRegion.TriggerRegionFlags> _triggerFlagsCombo = .("Trigger Flags");

    public static void Draw(App app, Gui gui, TriggerRegion obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _triggerShapeCombo.Draw(ref obj.TriggerShape);
        //TODO: Add extension to Imgui for NF vector types & BBox
        if (obj.TriggerShape == .Box)
        {
            if (ImGui.TreeNodeEx("Trigger Bounding Box", .FramePadding))
            {
                //TODO: Add extension to Imgui for NF vector types & BBox
                ImGui.InputFloat3("Min", ref *(float[3]*)&obj.LocalBBox.Min);
                ImGui.InputFloat3("Max", ref *(float[3]*)&obj.LocalBBox.Max);
                ImGui.TreePop();
            }
        }
        else if (obj.TriggerShape == .Sphere)
        {
            ImGui.InputFloat("Outer radius", &obj.OuterRadius);
        }

        ImGui.Checkbox("Enabled", &obj.Enabled);
        _regionTypeCombo.Draw(ref obj.RegionType);
        if (obj.RegionType == .KillHuman)
        {
            _killTypeCombo.Draw(ref obj.KillType);
        }
        _triggerFlagsCombo.Draw(ref obj.TriggerFlags);
    }
}

[RegisterInspector(typeof(ObjectMover))]
public static class ObjectMoverInspector : IZoneObjectInspector<ObjectMover>
{
    private static append BitflagComboBox<ObjectMover.BuildingTypeEnum> _buildingTypeCombo = .("Building Type");
    private static append XtblComboBox _gameplayPropsCombo = .("Gameplay Properties", "gameplay_properties.xtbl", "table;gameplay;name");
    private static append BitflagComboBox<ObjectMover.ChunkFlagsEnum> _chunkFlagsCombo = .("Chunk Flags");
    private static append XtblComboBox _propertiesCombo = .("Properties", "level_objects.xtbl", "table;level_object;name");
    private static append EnumComboBox<Team> _teamComboBox = .("Team");

    public static void Draw(App app, Gui gui, ObjectMover obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _buildingTypeCombo.Draw(ref obj.BuildingType);
        _chunkFlagsCombo.Draw(ref obj.ChunkFlags);
        _gameplayPropsCombo.Draw(obj.GameplayProps);
        _propertiesCombo.Draw(obj.Props);
        _teamComboBox.Draw(ref obj.Team);
        ImGui.InputScalar("Destruction Checksum", .U32, &obj.DestructionChecksum);
        ImGui.InputScalar("Chunk UID", .U32, &obj.ChunkUID);
        ImGui.InputText("Chunk Name", obj.ChunkName);
        ImGui.InputScalar("Destroyable UID", .U32, &obj.DestroyableUID);
        ImGui.InputScalar("Shape UID", .U32, &obj.ShapeUID);
        ImGui.InputFloat("Control", &obj.Control);
        ImGui.Checkbox("Dynamic", &obj.Dynamic);
        ImGui.Separator();
    }
}

[RegisterInspector(typeof(GeneralMover))]
public static class GeneralMoverInspector : IZoneObjectInspector<GeneralMover>
{
    public static void Draw(App app, Gui gui, GeneralMover obj)
    {
        ObjectMoverInspector.Draw(app, gui, obj);
        ImGui.InputScalar("GM Flags", .U32, &obj.GmFlags);
        ImGui.InputScalar("Original Object", .U32, &obj.OriginalObject);
        ImGui.InputScalar("Collision Type", .U32, &obj.CollisionType);
        ImGui.InputScalar("Idx", .U32, &obj.Idx);
        ImGui.InputScalar("Move Type", .U32, &obj.MoveType);
        ImGui.InputScalar("Destruction UID", .U32, &obj.DestructionUID);
        ImGui.InputScalar("Hitpoints", .S32, &obj.Hitpoints);
        ImGui.InputScalar("Dislodge Hitpoints", .S32, &obj.DislodgeHitpoints);
    }
}

[RegisterInspector(typeof(RfgMover))]
public static class RfgMoverInspector : IZoneObjectInspector<RfgMover>
{
    private static append EnumComboBox<RfgMover.MoveTypeEnum> _moveTypeCombo = .("Move Type");

    public static void Draw(App app, Gui gui, RfgMover obj)
    {
        ObjectMoverInspector.Draw(app, gui, obj);
        _moveTypeCombo.Draw(ref obj.MoveType);
        ImGui.InputFloat("Damage Percent", &obj.DamagePercent);
    }
}

[RegisterInspector(typeof(ShapeCutter))]
public static class ShapeCutterInspector : IZoneObjectInspector<ShapeCutter>
{
    public static void Draw(App app, Gui gui, ShapeCutter obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        ImGui.InputScalar("Shape Cutter Flags", .S16, &obj.ShapeCutterFlags);
        ImGui.InputFloat("Outer Radius", &obj.OuterRadius);
        ImGui.InputScalar("Source", .U32, &obj.Source);
        ImGui.InputScalar("Owner", .U32, &obj.Owner);
        ImGui.InputScalar("Explosion ID", .U8, &obj.ExplosionId);
    }
}

[RegisterInspector(typeof(ObjectEffect))]
public static class ObjectEffectInspector : IZoneObjectInspector<ObjectEffect>
{
    private static append XtblComboBox _effectTypeCombo = .("Effect Type", "effects.xtbl", "table;effect;name");

    public static void Draw(App app, Gui gui, ObjectEffect obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _effectTypeCombo.Draw(obj.EffectType);
    }
}

[RegisterInspector(typeof(Item))]
public static class ItemInspector : IZoneObjectInspector<Item>
{
    private static append XtblComboBox _itemTypeCombo = .("Item Type", "items_3d.xtbl", "table;item;name");
    public static bool DrawItemCombo = true;

    public static void Draw(App app, Gui gui, Item obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        if (DrawItemCombo)
        {
            _itemTypeCombo.Draw(obj.ItemType);
            ImGui.Separator();
        }
    }
}

[RegisterInspector(typeof(Weapon))]
public static class WeaponInspector : IZoneObjectInspector<Weapon>
{
    private static append XtblComboBox _weaponTypeCombo = .("Weapon Type", "weapons.xtbl", "table;Weapon;Name");

    public static void Draw(App app, Gui gui, Weapon obj)
    {
        ItemInspector.DrawItemCombo = false;
        ItemInspector.Draw(app, gui, obj);
        ItemInspector.DrawItemCombo = true;
        _weaponTypeCombo.Draw(obj.WeaponType);
    }
}

[RegisterInspector(typeof(Ladder))]
public static class LadderInspector : IZoneObjectInspector<Ladder>
{
    public static void Draw(App app, Gui gui, Ladder obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        ImGui.InputScalar("Ladder Rungs", .S32, &obj.LadderRungs);
        ImGui.Checkbox("Enabled", &obj.Enabled);
    }
}

[RegisterInspector(typeof(ObjLight))]
public static class ObjLightInspector : IZoneObjectInspector<ObjLight>
{
    private static append BitflagComboBox<ObjLight.ObjLightFlags> _objLightFlagsCombo = .("Light Flags");
    private static append EnumComboBox<ObjLight.LightTypeEnum> _objLightTypeCombo = .("Light Type");

    public static void Draw(App app, Gui gui, ObjLight obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        ImGui.InputFloat("Attenuation Start", &obj.AttenuationStart);
        ImGui.InputFloat("Attenuation End", &obj.AttenuationEnd);
        ImGui.InputFloat("Attenuation Range", &obj.AttenuationRange);
        _objLightFlagsCombo.Draw(ref obj.LightFlags);
        _objLightTypeCombo.Draw(ref obj.LightType);
        ImGui.InputFloat3("Color", ref *(float[3]*)&obj.Color);
        ImGui.InputFloat("Hotspot Size", &obj.HotspotSize);
        ImGui.InputFloat("Hotspot Falloff Size", &obj.HotspotFalloffSize);
        ImGui.InputFloat3("Min Clip", ref *(float[3]*)&obj.MinClip);
        ImGui.InputFloat3("Max Clip", ref *(float[3]*)&obj.MaxClip);
        ImGui.InputScalar("Clip Mesh", .S32, &obj.ClipMesh);
    }
}

[RegisterInspector(typeof(MultiMarker))]
public static class MultiMarkerInspector : IZoneObjectInspector<MultiMarker>
{
    private static append EnumComboBox<MultiMarkerType> _markerTypeCombo = .("Marker Type",
	    nameOverrides: new .(
            (.None, "None"),
		    (.SiegeTarget, "Siege Target"),
		    (.BackpackRack, "Backpack Rack"),
            (.SpawnNode, "Spawn Node"),
            (.FlagCaptureZone, "Flag Capture Zone"),
            (.KingOfTheHillTarget, "King of the Hill Target"),
            (.SpectatorCamera, "Spectator Camera")
	    ));
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");
    private static append EnumComboBox<MultiBackpackType> _backpackTypeCombo = .("Backpack Type");

    public static void Draw(App app, Gui gui, MultiMarker obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        if (ImGui.TreeNodeEx("Local Bounding Box", .FramePadding))
        {
            //TODO: Add extension to Imgui for NF vector types
            ImGui.InputFloat3("Min", ref *(float[3]*)&obj.LocalBBox.Min);
            ImGui.InputFloat3("Max", ref *(float[3]*)&obj.LocalBBox.Max);
            ImGui.TreePop();
        }
        _markerTypeCombo.Draw(ref obj.MarkerType);
        _teamCombo.Draw(ref obj.MpTeam);
        _backpackTypeCombo.Draw(ref obj.BackpackType);
        ImGui.InputScalar("Priority", .S32, &obj.Priority);
        ImGui.InputScalar("Num Backpacks", .S32, &obj.NumBackpacks);
        ImGui.InputScalar("Group", .S32, &obj.Group);
        ImGui.Checkbox("Random Backpacks", &obj.RandomBackpacks);
    }
}

[RegisterInspector(typeof(MultiFlag))]
public static class MultiFlagInspector : IZoneObjectInspector<MultiFlag>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");

    public static void Draw(App app, Gui gui, MultiFlag obj)
    {
        ItemInspector.Draw(app, gui, obj);
        _teamCombo.Draw(ref obj.MpTeam);
    }
}

//These objects aren't actually supposed to be in MP maps, but since there's a few in the Nordic Special map and mp_wasteland they need placeholder inspectors
[RegisterInspector(typeof(NavPoint))]
public static class NavPointInspector : IZoneObjectInspector<NavPoint>
{
    public static void Draw(App app, Gui gui, NavPoint obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        //TODO: Implement. Not needed yet for MP so not yet implemented.
    }
}

[RegisterInspector(typeof(CoverNode))]
public static class CoverNodeInspector : IZoneObjectInspector<CoverNode>
{
    public static void Draw(App app, Gui gui, CoverNode obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(RfgConstraint))]
public static class RfgConstraintInspector : IZoneObjectInspector<RfgConstraint>
{
    public static void Draw(App app, Gui gui, RfgConstraint obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(ActionNode))]
public static class ActionNodeInspector : IZoneObjectInspector<ActionNode>
{
    public static void Draw(App app, Gui gui, ActionNode obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        //TODO: Implement
    }
}