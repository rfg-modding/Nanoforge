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
    public static void Draw(App app, MapEditorDocument editor, Zone zone, T obj);
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

    private static append String _parentSelectorSearch;

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ZoneObject obj)
    {
        Fonts.FontL.Push();
        ImGui.Text(obj.Classname);
        Fonts.FontL.Pop();
        ImGui.Text(scope $"{obj.Handle}, {obj.Num}");
        ImGui.Separator();

        //TODO: Add description and display name and load/save it in EditorData.xml or equivalent

        ImGui.InputBoundingBox("Bounding Box", ref obj.BBox);
        ImGui.InputMat3("Orientation", ref obj.Orient);
        DrawRelativeEditor(app, editor, zone, obj);
        _flagsCombo.Draw(ref obj.Flags);

        Vec3 initialPos = obj.Position;
        if (ImGui.InputVec3("Position", ref obj.Position))
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

    private static void DrawRelativeEditor(App app, MapEditorDocument editor, Zone zone, ZoneObject obj)
    {
        ZoneObject currentParent = obj.Parent;
        StringView currentParentName = (currentParent != null) ? currentParent.Name : "Not set";
        if (ImGui.BeginCombo("Parent", currentParentName.Ptr))
        {
            //Search bar
            ImGui.InputText("Search", _parentSelectorSearch);
            ImGui.SameLine();
            if (ImGui.Button(Icons.ICON_FA_WINDOW_CLOSE))
            {
                _parentSelectorSearch.Set("");
            }

            for (ZoneObject newParent in zone.Objects)
			{
                if (newParent == obj)
                    continue;
                if (newParent == null)
                    continue;

                String newParentName = scope String()..Append(newParent.Name);
                if (newParentName.IsEmpty)
                {
                    newParentName.Set(scope $"{newParent.Classname}, {newParent.Handle}, {newParent.Num}");
                }
                if (!(_parentSelectorSearch.IsEmpty || _parentSelectorSearch.IsWhiteSpace) && !newParentName.Contains(_parentSelectorSearch, ignoreCase: true))
                    continue;

                bool selected = (newParent == currentParent);
                if (ImGui.Selectable(newParentName, selected))
                {
                    SetObjectParent(obj, newParent);
                }
			}

            ImGui.EndCombo();
        }

        ImGui.SameLine();
        if (ImGui.Button("Reset"))
        {
            obj.Parent = null;
            SetObjectParent(obj, null);
        }
    }

    //TODO: Move this into ZoneObject and make sure parent can only be set through this or an equivalent property. May need to make tweaks for it to work with bon
    private static void SetObjectParent(ZoneObject obj, ZoneObject newParent)
    {
        ZoneObject oldParent = obj.Parent;
        if (oldParent == newParent)
            return; //Nothing will change

        //Don't allow objects to be their own parent
        if (obj == newParent)
        {
            Logger.Error("Failed to update relation data! Tried to make an object its own parent.");
            return;
        }

        //Remove object from old parents children list
        if (oldParent != null)
        {
            oldParent.Children.Remove(obj);
        }

        //Set parent and add to children list
        obj.Parent = newParent;
        if (newParent != null)
        {
            newParent.Children.Add(obj);
        }
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
    private static append XtblComboBox _spawnResourceCombo = .("Spawn resource", "spawn_resource.xtbl", "Table;spawn_resource_entry;Name");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjZone obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _ambientSpawnCombo.Draw(obj.AmbientSpawn);
        _spawnResourceCombo.Draw(obj.SpawnResource);
        ImGui.InputText("Terrain file name", obj.TerrainFileName);
        ImGui.TooltipOnPrevious("string");
        ImGui.InputFloat("Wind min speed", &obj.WindMinSpeed);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputFloat("Wind max speed", &obj.WindMaxSpeed);
        ImGui.TooltipOnPrevious("float");
    }
}

[RegisterInspector(typeof(ObjectBoundingBox))]
public static class ObjectBoundingBoxInspector : IZoneObjectInspector<ObjectBoundingBox>
{
    private static append EnumComboBox<ObjectBoundingBox.BoundingBoxType> _bboxTypeCombo = .("Local BBox Type");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectBoundingBox obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.InputBoundingBox("Local Bounding Box", ref obj.LocalBBox);
        _bboxTypeCombo.Draw(ref obj.BBType);
    }
}

[RegisterInspector(typeof(ObjectDummy))]
public static class ObjectDummyInspector : IZoneObjectInspector<ObjectDummy>
{
    private static append EnumComboBox<ObjectDummy.DummyType> _dummyTypeCombo = .("Dummy type");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectDummy obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _dummyTypeCombo.Draw(ref obj.DummyType);
    }
}

[RegisterInspector(typeof(PlayerStart))]
public static class PlayerStartInspector : IZoneObjectInspector<PlayerStart>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");
    private static append XtblComboBox _missionInfoCombo = .("Mission Info", "missions.xtbl", "table;mission;Name", defaultOption: "none");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, PlayerStart obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _teamCombo.Draw(ref obj.MpTeam);
        _missionInfoCombo.Draw(obj.MissionInfo);
        ImGui.Checkbox("Indoor", &obj.Indoor);
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

    public static void Draw(App app, MapEditorDocument editor, Zone zone, TriggerRegion obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _triggerShapeCombo.Draw(ref obj.TriggerShape);
        if (obj.TriggerShape == .Box)
        {
            ImGui.InputBoundingBox("Trigger Bounding Box", ref obj.LocalBBox);
        }
        else if (obj.TriggerShape == .Sphere)
        {
            ImGui.InputFloat("Outer radius", &obj.OuterRadius);
            ImGui.TooltipOnPrevious("float");
        }

        _regionTypeCombo.Draw(ref obj.RegionType);
        if (obj.RegionType == .KillHuman)
        {
            _killTypeCombo.Draw(ref obj.KillType);
        }
        _triggerFlagsCombo.Draw(ref obj.TriggerFlags);
        ImGui.Checkbox("Enabled", &obj.Enabled);
    }
}

[RegisterInspector(typeof(ObjectMover))]
public static class ObjectMoverInspector : IZoneObjectInspector<ObjectMover>
{
    private static append BitflagComboBox<ObjectMover.BuildingTypeFlagsEnum> _buildingTypeCombo = .("Building Type");
    private static append XtblComboBox _gameplayPropsCombo = .("Gameplay Properties", "gameplay_properties.xtbl", "table;gameplay;name");
    private static append BitflagComboBox<ObjectMover.ChunkFlagsEnum> _chunkFlagsCombo = .("Chunk Flags");
    private static append XtblComboBox _propertiesCombo = .("Properties", "level_objects.xtbl", "table;level_object;name");
    private static append EnumComboBox<Team> _teamComboBox = .("Team");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectMover obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _buildingTypeCombo.Draw(ref obj.BuildingType);
        _chunkFlagsCombo.Draw(ref obj.ChunkFlags);
        _gameplayPropsCombo.Draw(obj.GameplayProps);
        _propertiesCombo.Draw(obj.Props);
        _teamComboBox.Draw(ref obj.Team);
        ImGui.InputScalar("Destruction Checksum", .U32, &obj.DestructionChecksum);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Chunk UID", .U32, &obj.ChunkUID);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputText("Chunk Name", obj.ChunkName);
        ImGui.TooltipOnPrevious("string");
        ImGui.InputScalar("Destroyable UID", .U32, &obj.DestroyableUID);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Shape UID", .U32, &obj.ShapeUID);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputFloat("Control", &obj.Control);
        ImGui.TooltipOnPrevious("float");
        ImGui.Checkbox("Dynamic", &obj.Dynamic);
        ImGui.Separator();
    }
}

[RegisterInspector(typeof(GeneralMover))]
public static class GeneralMoverInspector : IZoneObjectInspector<GeneralMover>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, GeneralMover obj)
    {
        ObjectMoverInspector.Draw(app, editor, zone, obj);
        ImGui.InputScalar("GM Flags", .U32, &obj.GmFlags);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Original Object", .U32, &obj.OriginalObject);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Collision Type", .U32, &obj.CollisionType);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Idx", .U32, &obj.Idx);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Move Type", .U32, &obj.MoveType);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Destruction UID", .U32, &obj.DestructionUID);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Hitpoints", .S32, &obj.Hitpoints);
        ImGui.TooltipOnPrevious("int32");
        ImGui.InputScalar("Dislodge Hitpoints", .S32, &obj.DislodgeHitpoints);
        ImGui.TooltipOnPrevious("int32");
    }
}

[RegisterInspector(typeof(RfgMover))]
public static class RfgMoverInspector : IZoneObjectInspector<RfgMover>
{
    private static append EnumComboBox<RfgMover.MoveTypeEnum> _moveTypeCombo = .("Move Type");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, RfgMover obj)
    {
        ObjectMoverInspector.Draw(app, editor, zone, obj);
        _moveTypeCombo.Draw(ref obj.MoveType);
        ImGui.InputFloat("Damage Percent", &obj.DamagePercent);
        ImGui.TooltipOnPrevious("float");
    }
}

[RegisterInspector(typeof(ShapeCutter))]
public static class ShapeCutterInspector : IZoneObjectInspector<ShapeCutter>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, ShapeCutter obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.InputScalar("Shape Cutter Flags", .S16, &obj.ShapeCutterFlags);
        ImGui.TooltipOnPrevious("int16");
        ImGui.InputFloat("Outer Radius", &obj.OuterRadius);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputScalar("Source", .U32, &obj.Source);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Owner", .U32, &obj.Owner);
        ImGui.TooltipOnPrevious("uint32");
        ImGui.InputScalar("Explosion ID", .U8, &obj.ExplosionId);
        ImGui.TooltipOnPrevious("uint8");
    }
}

[RegisterInspector(typeof(ObjectEffect))]
public static class ObjectEffectInspector : IZoneObjectInspector<ObjectEffect>
{
    private static append XtblComboBox _effectTypeCombo = .("Effect Type", "effects.xtbl", "table;effect;name");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectEffect obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _effectTypeCombo.Draw(obj.EffectType);
    }
}

[RegisterInspector(typeof(Item))]
public static class ItemInspector : IZoneObjectInspector<Item>
{
    private static append XtblComboBox _itemTypeCombo = .("Item Type", "items_3d.xtbl", "table;item;name");
    public static bool DrawItemCombo = true;

    public static void Draw(App app, MapEditorDocument editor, Zone zone, Item obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
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

    public static void Draw(App app, MapEditorDocument editor, Zone zone, Weapon obj)
    {
        ItemInspector.DrawItemCombo = false;
        ItemInspector.Draw(app, editor, zone, obj);
        ItemInspector.DrawItemCombo = true;
        _weaponTypeCombo.Draw(obj.WeaponType);
    }
}

[RegisterInspector(typeof(Ladder))]
public static class LadderInspector : IZoneObjectInspector<Ladder>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, Ladder obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.InputScalar("Ladder Rungs", .S32, &obj.LadderRungs);
        ImGui.TooltipOnPrevious("int32");
        ImGui.Checkbox("Enabled", &obj.Enabled);
    }
}

[RegisterInspector(typeof(ObjLight))]
public static class ObjLightInspector : IZoneObjectInspector<ObjLight>
{
    private static append BitflagComboBox<ObjLight.ObjLightFlags> _objLightFlagsCombo = .("Light Flags");
    private static append EnumComboBox<ObjLight.LightTypeEnum> _objLightTypeCombo = .("Light Type");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjLight obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.InputFloat("Attenuation Start", &obj.AttenuationStart);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputFloat("Attenuation End", &obj.AttenuationEnd);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputFloat("Attenuation Range", &obj.AttenuationRange);
        ImGui.TooltipOnPrevious("float");
        _objLightFlagsCombo.Draw(ref obj.LightFlags);
        _objLightTypeCombo.Draw(ref obj.LightType);
        ImGui.ColorEdit3("Color", ref *(float[3]*)&obj.Color);
        ImGui.TooltipOnPrevious("color");
        ImGui.InputFloat("Hotspot Size", &obj.HotspotSize);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputFloat("Hotspot Falloff Size", &obj.HotspotFalloffSize);
        ImGui.TooltipOnPrevious("float");
        ImGui.InputVec3("Min Clip", ref obj.MinClip);
        ImGui.InputVec3("Max Clip", ref obj.MaxClip);
        //ImGui.InputScalar("Clip Mesh", .S32, &obj.ClipMesh);
        ImGui.TooltipOnPrevious("int32");
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

    public static void Draw(App app, MapEditorDocument editor, Zone zone, MultiMarker obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.InputBoundingBox("Local Bounding Box", ref obj.LocalBBox);
        _markerTypeCombo.Draw(ref obj.MarkerType);
        _teamCombo.Draw(ref obj.MpTeam);
        ImGui.InputScalar("Priority", .S32, &obj.Priority);
        ImGui.TooltipOnPrevious("int32");
        if (obj.MarkerType == .BackpackRack)
        {
            _backpackTypeCombo.Draw(ref obj.BackpackType);
            ImGui.InputScalar("Num Backpacks", .S32, &obj.NumBackpacks);
            ImGui.TooltipOnPrevious("int32");
            ImGui.InputScalar("Group", .S32, &obj.Group);
            ImGui.TooltipOnPrevious("int32");
            ImGui.Checkbox("Random Backpacks", &obj.RandomBackpacks);
        }
    }
}

[RegisterInspector(typeof(MultiFlag))]
public static class MultiFlagInspector : IZoneObjectInspector<MultiFlag>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");

    public static void Draw(App app, MapEditorDocument editor, Zone zone, MultiFlag obj)
    {
        ItemInspector.Draw(app, editor, zone, obj);
        _teamCombo.Draw(ref obj.MpTeam);
    }
}

//These objects aren't actually supposed to be in MP maps, but since there's a few in the Nordic Special map and mp_wasteland they need placeholder inspectors
[RegisterInspector(typeof(NavPoint))]
public static class NavPointInspector : IZoneObjectInspector<NavPoint>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, NavPoint obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        //TODO: Implement. Not needed yet for MP so not yet implemented.
    }
}

[RegisterInspector(typeof(CoverNode))]
public static class CoverNodeInspector : IZoneObjectInspector<CoverNode>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, CoverNode obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(RfgConstraint))]
public static class RfgConstraintInspector : IZoneObjectInspector<RfgConstraint>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, RfgConstraint obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(ActionNode))]
public static class ActionNodeInspector : IZoneObjectInspector<ActionNode>
{
    public static void Draw(App app, MapEditorDocument editor, Zone zone, ActionNode obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        //TODO: Implement
    }
}