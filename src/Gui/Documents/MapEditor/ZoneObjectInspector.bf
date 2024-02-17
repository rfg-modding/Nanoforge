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
    public static void Init();
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

    private static bool _editingName = false;
    private static ZoneObject _objectGettingNameEdited = null; //Used to cancel name edit if selected object changes
    private static append String _nameEditBuffer;

    public static void Init()
    {
        _flagsCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ZoneObject obj)
    {
        //Cancel name edit if user selects a different one
        if (_objectGettingNameEdited != null && _objectGettingNameEdited != obj)
        {
            _objectGettingNameEdited = null;
            _editingName = false;
            _nameEditBuffer.Set("");
        }

        //Name display and editing
        Fonts.FontM.Push();
        if (_editingName)
        {
            mixin ApplyNameEdit()
            {
                obj.Name.Set(_nameEditBuffer);
                _objectGettingNameEdited = null;
                _editingName = false;
                editor.UnsavedChanges = true;
                _nameEditBuffer.Set("");
            }

            if (ImGui.InputText("##ObjectNameEdit", _nameEditBuffer, .EnterReturnsTrue))
            {
                ApplyNameEdit!();
            }
            ImGui.SameLine();
            if (ImGui.Button("Cancel"))
            {
                _editingName = false;
                _objectGettingNameEdited = null;
                _nameEditBuffer.Set("");
            }
            ImGui.SameLine();
            if (ImGui.Button("Save"))
            {
                ApplyNameEdit!();
            }
        }
        else
        {
            f32 nameTextAdjustment = 2.5f; //Little hack to align the name text and rename button
            ImGui.SetCursorPosY(ImGui.GetCursorPosY() + nameTextAdjustment);
            ImGui.Text(obj.Name);
            ImGui.SameLine();
            ImGui.SetCursorPosY(ImGui.GetCursorPosY() - nameTextAdjustment);
            if (ImGui.Button("Rename"))
            {
                _editingName = true;
                _nameEditBuffer.Set(obj.Name);
                _objectGettingNameEdited = obj;
            }
        }
        Fonts.FontM.Pop();

        ImGui.SetCursorPosY(ImGui.GetCursorPosY() - 7.0f);
        using (ImGui.DisposableStyleColor(ImGui.Col.Text, ImGui.SecondaryTextColor))
        {
            ImGui.Text(scope $"{obj.Classname} - {obj.Handle}, {obj.Num}");
        }    
        ImGui.Separator();

        if (ImGui.InputOptionalString("Description", obj.Description))
        {
            editor.UnsavedChanges = true;
        }

        if (ImGui.InputOptionalString("RFG Display name", obj.RfgDisplayName))
        {
            editor.UnsavedChanges = true;
        }

        DrawRelativeEditor(app, editor, zone, obj);
        _flagsCombo.Draw(editor, ref obj.Flags);

        Vec3 initialPos = obj.Position;
        if (ImGui.InputVec3("Position", ref obj.Position))
        {
            editor.UnsavedChanges = true;
            Vec3 delta = obj.Position - initialPos;
            obj.BBox += delta;

            //Update mesh position
            if (obj.RenderObject != null)
            {
                obj.RenderObject.Position = obj.Position;
            }

            if (editor.AutoMoveChildren)
            {
                MoveObjectChildrenRecursive(obj, delta);
            }    
        }

        if (ImGui.InputBoundingBox("Bounding Box", ref obj.BBox))
        {
            editor.UnsavedChanges = true;
        }
        //Disabled in favor of pitch/yaw/roll sliders. Would be willing to re-enable as a opt-in feature so people can intentionally screw up the matrix and make cursed maps
        /*if (ImGui.InputOptionalMat3("Orientation", ref obj.Orient))
        {
            editor.UnsavedChanges = true;
            if (obj.RenderObject != null && obj.Orient.Enabled)
            {
                obj.RenderObject.Rotation = obj.Orient.Value;
            }
        }*/
        ImGui.Checkbox("##AnglesEnabled", &obj.Orient.Enabled);
        ImGui.SameLine();
        using (ImGui.DisabledBlock(!obj.Orient.Enabled))
        {
            if (ImGui.TreeNodeEx("Orientation", .FramePadding))
            {
                f32 initialPitch = obj.Pitch;
                f32 initialYaw = obj.Yaw;
                f32 initialRoll = obj.Roll;

                f32 angleMinDeg = -360.0f;
                f32 angleMaxDeg = 360.0f;
                bool orientChanged = false;
                if (ImGui.SliderFloat("Pitch", &obj.Pitch, angleMinDeg, angleMaxDeg))
                {
                    orientChanged = true;
                }
                if (ImGui.SliderFloat("Yaw", &obj.Yaw, angleMinDeg, angleMaxDeg))
                {
                    orientChanged = true;
                }
                if (ImGui.SliderFloat("Roll", &obj.Roll, angleMinDeg, angleMaxDeg))
                {
                    orientChanged = true;
                }

                //Make matrices from the angle deltas and multiply Orient by them to rotate it
                Mat3 pitchDelta = Mat3.RotationAxis(.(0.0f, 0.0f, 1.0f), Math.ToRadians(obj.Pitch - initialPitch));
                Mat3 yawDelta = Mat3.RotationAxis(.(0.0f, 1.0f, 0.0f), Math.ToRadians(obj.Yaw - initialYaw));
                Mat3 rollDelta = Mat3.RotationAxis(.(1.0f, 0.0f, 0.0f), Math.ToRadians(obj.Roll - initialRoll));
                Mat3 rotationDelta = rollDelta * yawDelta * pitchDelta;
                obj.Orient.Value *= rotationDelta;

                if (orientChanged && obj.RenderObject != null)
                {
                    obj.RenderObject.Rotation = obj.Orient.Value;
                }

                ImGui.TreePop();
            }
        }

        ImGui.Separator();
    }

    private static void DrawRelativeEditor(App app, MapEditorDocument editor, Zone zone, ZoneObject obj)
    {
        ZoneObject currentParent = obj.Parent;
        StringView currentParentName = (currentParent != null) ? currentParent.Name..EnsureNullTerminator() : "Not set";
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
                if (IsObjectDescendant(obj, newParent))
                    continue; //Can't make an objects descendant into its parent

                String newParentName = scope String()..Append(newParent.Name);
                if (newParentName.IsEmpty)
                {
                    newParentName.Set(scope $"{newParent.Classname}");
                }
                newParentName.Append(scope $", {newParent.Handle}, {newParent.Num}");
                if (!(_parentSelectorSearch.IsEmpty || _parentSelectorSearch.IsWhiteSpace) && !newParentName.Contains(_parentSelectorSearch, ignoreCase: true))
                    continue;

                bool selected = (newParent == currentParent);
                if (ImGui.Selectable(newParentName, selected))
                {
                    editor.UnsavedChanges = true;
                    SetObjectParent(obj, newParent);
                }
			}

            ImGui.EndCombo();
        }

        ImGui.SameLine();
        if (ImGui.Button("Reset"))
        {
            editor.UnsavedChanges = true;
            obj.Parent = null;
            SetObjectParent(obj, null);
        }
    }

    //Recursively iterates child tree of rootObject and returns true if target is found.
    private static bool IsObjectDescendant(ZoneObject rootObject, ZoneObject target)
    {
        for (ZoneObject child in rootObject.Children)
        {
            if (child == target)
            {
                return true;
            }
            if (IsObjectDescendant(child, target))
            {
                return true;
            }
        }

        return false;
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
        if (IsObjectDescendant(obj, newParent))
        {
            Logger.Error("Failed to update relation data! Tried to make an objects descendant into its parent.");
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

    public static void Init()
    {
        _ambientSpawnCombo.Init();
        _spawnResourceCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjZone obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _ambientSpawnCombo.Draw(editor, obj.AmbientSpawn);
        _spawnResourceCombo.Draw(editor, obj.SpawnResource);
        if (ImGui.InputText("Terrain file name", obj.TerrainFileName))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("string");

        if (ImGui.InputOptionalFloat("Wind min speed", ref obj.WindMinSpeed))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputOptionalFloat("Wind max speed", ref obj.WindMaxSpeed))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");
    }
}

[RegisterInspector(typeof(ObjectBoundingBox))]
public static class ObjectBoundingBoxInspector : IZoneObjectInspector<ObjectBoundingBox>
{
    private static append EnumComboBox<ObjectBoundingBox.BoundingBoxType> _bboxTypeCombo = .("Local BBox Type");

    public static void Init()
    {
        _bboxTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectBoundingBox obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputBoundingBox("Local Bounding Box", ref obj.LocalBBox))
        {
            editor.UnsavedChanges = true;
        }
        _bboxTypeCombo.Draw(editor, ref obj.BBType);
    }
}

[RegisterInspector(typeof(ObjectDummy))]
public static class ObjectDummyInspector : IZoneObjectInspector<ObjectDummy>
{
    private static append EnumComboBox<ObjectDummy.DummyType> _dummyTypeCombo = .("Dummy type");

    public static void Init()
    {
        _dummyTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectDummy obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _dummyTypeCombo.Draw(editor, ref obj.DummyType);
    }
}

[RegisterInspector(typeof(PlayerStart))]
public static class PlayerStartInspector : IZoneObjectInspector<PlayerStart>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");
    private static append XtblComboBox _missionInfoCombo = .("Mission Info", "missions.xtbl", "table;mission;Name", defaultOption: "none");

    public static void Init()
    {
        _teamCombo.Init();
        _missionInfoCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, PlayerStart obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _teamCombo.Draw(editor, ref obj.MpTeam);
        _missionInfoCombo.Draw(editor, obj.MissionInfo);
        if (ImGui.Checkbox("Indoor", &obj.Indoor))
        {
            editor.UnsavedChanges = true;
        }
        if (ImGui.Checkbox("Initial spawn", &obj.InitialSpawn))
        {
            editor.UnsavedChanges = true;
        }
        if (ImGui.Checkbox("Respawn", &obj.Respawn))
        {
            editor.UnsavedChanges = true;
        }
        if (ImGui.Checkbox("Checkpoint Respawn", &obj.CheckpointRespawn))
        {
            editor.UnsavedChanges = true;
        }
        if (ImGui.Checkbox("Activity Respawn", &obj.ActivityRespawn))
        {
            editor.UnsavedChanges = true;
        }
    }
}

[RegisterInspector(typeof(TriggerRegion))]
public static class TriggerRegionInspector : IZoneObjectInspector<TriggerRegion>
{
    private static append EnumComboBox<TriggerRegion.Shape> _triggerShapeCombo = .("Trigger Shape");
    private static append EnumComboBox<TriggerRegion.RegionTypeEnum> _regionTypeCombo = .("Region Type");
    private static append EnumComboBox<TriggerRegion.KillTypeEnum> _killTypeCombo = .("Kill Type");
    private static append BitflagComboBox<TriggerRegion.TriggerRegionFlags> _triggerFlagsCombo = .("Trigger Flags");

    public static void Init()
    {
        _triggerShapeCombo.Init();
        _regionTypeCombo.Init();
        _killTypeCombo.Init();
        _triggerFlagsCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, TriggerRegion obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);

        _triggerShapeCombo.Draw(editor, ref obj.TriggerShape);
        using (ImGui.DisabledBlock(!obj.TriggerShape.Enabled))
        {
            if (obj.TriggerShape.Value == .Box)
            {
                if (ImGui.InputBoundingBox("Trigger Bounding Box", ref obj.LocalBBox))
                {
                    editor.UnsavedChanges = true;
                }
            }
            else if (obj.TriggerShape.Value == .Sphere)
            {
                if (ImGui.InputFloat("Outer radius", &obj.OuterRadius))
                {
                    editor.UnsavedChanges = true;
                }
                ImGui.TooltipOnPrevious("float");
            }
        }

        _regionTypeCombo.Draw(editor, ref obj.RegionType);
        using (ImGui.DisabledBlock(!obj.RegionType.Enabled))
        {
            if (obj.RegionType.Value == .KillHuman)
            {
                _killTypeCombo.Draw(editor, ref obj.KillType);
            }
        }

        _triggerFlagsCombo.Draw(editor, ref obj.TriggerFlags);
        if (ImGui.Checkbox("Enabled", &obj.Enabled))
        {
            editor.UnsavedChanges = true;
        }
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

    public static void Init()
    {
        _buildingTypeCombo.Init();
        _gameplayPropsCombo.Init();
        _chunkFlagsCombo.Init();
        _propertiesCombo.Init();
        _teamComboBox.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectMover obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _buildingTypeCombo.Draw(editor, ref obj.BuildingType);
        _chunkFlagsCombo.Draw(editor, ref obj.ChunkFlags);
        _gameplayPropsCombo.Draw(editor, obj.GameplayProps);
        _propertiesCombo.Draw(editor, obj.Props);
        _teamComboBox.Draw(editor, ref obj.Team);
        if (ImGui.InputScalar("Destruction Checksum", .U32, &obj.DestructionChecksum))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Chunk UID", .U32, &obj.ChunkUID))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputOptionalString("Chunk Name", obj.ChunkName))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("string");

        if (ImGui.InputScalar("Destroyable UID", .U32, &obj.DestroyableUID))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Shape UID", .U32, &obj.ShapeUID))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputOptionalFloat("Control", ref obj.Control))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        using (ImGui.DisabledBlock(!obj.Dynamic.Enabled))
        {
            if (ImGui.Checkbox("Dynamic", &obj.Dynamic.Value))
            {
                editor.UnsavedChanges = true;
            }
        }

        ImGui.Separator();
    }
}

[RegisterInspector(typeof(GeneralMover))]
public static class GeneralMoverInspector : IZoneObjectInspector<GeneralMover>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, GeneralMover obj)
    {
        ObjectMoverInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputScalar("GM Flags", .U32, &obj.GmFlags))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Original Object", .U32, &obj.OriginalObject))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Collision Type", .U32, &obj.CollisionType))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Idx", .U32, &obj.Idx))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Move Type", .U32, &obj.MoveType))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Destruction UID", .U32, &obj.DestructionUID))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Hitpoints", .S32, &obj.Hitpoints))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int32");

        if (ImGui.InputScalar("Dislodge Hitpoints", .S32, &obj.DislodgeHitpoints))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int32");
    }
}

[RegisterInspector(typeof(RfgMover))]
public static class RfgMoverInspector : IZoneObjectInspector<RfgMover>
{
    private static append EnumComboBox<RfgMover.MoveTypeEnum> _moveTypeCombo = .("Move Type");

    public static void Init()
    {
        _moveTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, RfgMover obj)
    {
        ObjectMoverInspector.Draw(app, editor, zone, obj);
        _moveTypeCombo.Draw(editor, ref obj.MoveType);
        if (ImGui.InputOptionalFloat("Damage Percent", ref obj.DamagePercent))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputOptionalFloat("Game destroyed percentage", ref obj.GameDestroyedPercentage))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");
    }
}

[RegisterInspector(typeof(ShapeCutter))]
public static class ShapeCutterInspector : IZoneObjectInspector<ShapeCutter>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ShapeCutter obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputScalar("Shape Cutter Flags", .S16, &obj.ShapeCutterFlags))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int16");

        if (ImGui.InputOptionalFloat("Outer Radius", ref obj.OuterRadius))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputScalar("Source", .U32, &obj.Source))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Owner", .U32, &obj.Owner))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint32");

        if (ImGui.InputScalar("Explosion ID", .U8, &obj.ExplosionId))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("uint8");
    }
}

[RegisterInspector(typeof(ObjectEffect))]
public static class ObjectEffectInspector : IZoneObjectInspector<ObjectEffect>
{
    private static append XtblComboBox _effectTypeCombo = .("Effect Type", "effects.xtbl", "table;effect;name");

    public static void Init()
    {
        _effectTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjectEffect obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        _effectTypeCombo.Draw(editor, obj.EffectType);

        if (ImGui.InputOptionalString("Sound Alr", obj.SoundAlr))
        {
            editor.UnsavedChanges = true;
        }

        if (ImGui.InputOptionalString("Sound", obj.Sound))
        {
            editor.UnsavedChanges = true;
        }

        if (ImGui.InputOptionalString("Visual", obj.Visual))
        {
            editor.UnsavedChanges = true;
        }

        using (ImGui.DisabledBlock(!obj.Looping.Enabled))
        {
            if (ImGui.Checkbox("Looping", &obj.Looping.Value))
            {
                editor.UnsavedChanges = true;
            }
        }
    }
}

[RegisterInspector(typeof(Item))]
public static class ItemInspector : IZoneObjectInspector<Item>
{
    private static append XtblComboBox _itemTypeCombo = .("Item Type", "items_3d.xtbl", "table;item;name");
    public static bool DrawItemCombo = true; //Used to hide the item combo box for classes that inherit item, like weapon

    public static void Init()
    {
        _itemTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, Item obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (DrawItemCombo)
        {
            _itemTypeCombo.Draw(editor, obj.ItemType);
        }

        using (ImGui.DisabledBlock(!obj.Respawns.Enabled))
        {
            if (ImGui.Checkbox("Respawns", &obj.Respawns.Value))
            {
                editor.UnsavedChanges = true;
            }
        }    

        using (ImGui.DisabledBlock(!obj.Preplaced.Enabled))
        {
            if (ImGui.Checkbox("Preplaced", &obj.Preplaced.Value))
            {
                editor.UnsavedChanges = true;
            }
        }    

        ImGui.Separator();
    }
}

[RegisterInspector(typeof(Weapon))]
public static class WeaponInspector : IZoneObjectInspector<Weapon>
{
    private static append XtblComboBox _weaponTypeCombo = .("Weapon Type", "weapons.xtbl", "table;Weapon;Name");

    public static void Init()
    {
        _weaponTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, Weapon obj)
    {
        ItemInspector.DrawItemCombo = false;
        ItemInspector.Draw(app, editor, zone, obj);
        ItemInspector.DrawItemCombo = true;
        _weaponTypeCombo.Draw(editor, obj.WeaponType);
    }
}

[RegisterInspector(typeof(Ladder))]
public static class LadderInspector : IZoneObjectInspector<Ladder>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, Ladder obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputScalar("Ladder Rungs", .S32, &obj.LadderRungs))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int32");

        using (ImGui.DisabledBlock(!obj.LadderEnabled.Enabled))
        {
            if (ImGui.Checkbox("Enabled", &obj.LadderEnabled.Value))
            {
                editor.UnsavedChanges = true;
            }
        }
    }
}

[RegisterInspector(typeof(ObjLight))]
public static class ObjLightInspector : IZoneObjectInspector<ObjLight>
{
    private static append BitflagComboBox<ObjLight.ObjLightFlags> _objLightFlagsCombo = .("Light Flags");
    private static append EnumComboBox<ObjLight.LightTypeEnum> _objLightTypeCombo = .("Light Type");

    public static void Init()
    {
        _objLightFlagsCombo.Init();
        _objLightTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ObjLight obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputOptionalFloat("Attenuation Start", ref obj.AttenuationStart))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputOptionalFloat("Attenuation End", ref obj.AttenuationEnd))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputOptionalFloat("Attenuation Range", ref obj.AttenuationRange))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        _objLightFlagsCombo.Draw(editor, ref obj.LightFlags);
        _objLightTypeCombo.Draw(editor, ref obj.LightType);

        if (ImGui.ColorEdit3("Color", ref *(float[3]*)&obj.Color))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("color");

        if (ImGui.InputOptionalFloat("Hotspot Size", ref obj.HotspotSize))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputOptionalFloat("Hotspot Falloff Size", ref obj.HotspotFalloffSize))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("float");

        if (ImGui.InputVec3("Min Clip", ref obj.MinClip))
        {
            editor.UnsavedChanges = true;
        }

        if (ImGui.InputVec3("Max Clip", ref obj.MaxClip))
        {
            editor.UnsavedChanges = true;
        }

        //ImGui.InputScalar("Clip Mesh", .S32, &obj.ClipMesh);
        //ImGui.TooltipOnPrevious("int32");
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
    private static append XtblComboBox _backpackTypeCombo = .("Backpack type", "mp_backpacks.xtbl", "table;mp_backpacks;Name");

    public static void Init()
    {
        _markerTypeCombo.Init();
        _teamCombo.Init();
        _backpackTypeCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, MultiMarker obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        if (ImGui.InputBoundingBox("Local Bounding Box", ref obj.LocalBBox))
        {
            editor.UnsavedChanges = true;
        }

        _markerTypeCombo.Draw(editor, ref obj.MarkerType);
        _teamCombo.Draw(editor, ref obj.MpTeam);

        if (ImGui.InputScalar("Priority", .S32, &obj.Priority))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int32");

        if (ImGui.InputScalar("Group", .S32, &obj.Group))
        {
            editor.UnsavedChanges = true;
        }
        ImGui.TooltipOnPrevious("int32");

        if (obj.MarkerType == .BackpackRack)
        {
            _backpackTypeCombo.Draw(editor, obj.BackpackType);
            if (ImGui.InputScalar("Num Backpacks", .S32, &obj.NumBackpacks))
            {
                editor.UnsavedChanges = true;
            }
            ImGui.TooltipOnPrevious("int32");

            using (ImGui.DisabledBlock(!obj.RandomBackpacks.Enabled))
            {
                if (ImGui.Checkbox("Random Backpacks", &obj.RandomBackpacks.Value))
                {
                    editor.UnsavedChanges = true;
                }
            }
        }
    }
}

[RegisterInspector(typeof(MultiFlag))]
public static class MultiFlagInspector : IZoneObjectInspector<MultiFlag>
{
    private static append EnumComboBox<Team> _teamCombo = .("MP Team");

    public static void Init()
    {
        _teamCombo.Init();
    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, MultiFlag obj)
    {
        ItemInspector.Draw(app, editor, zone, obj);
        _teamCombo.Draw(editor, ref obj.MpTeam);
    }
}

//These objects aren't actually supposed to be in MP maps, but since there's a few in the Nordic Special map and mp_wasteland they need placeholder inspectors
[RegisterInspector(typeof(NavPoint))]
public static class NavPointInspector : IZoneObjectInspector<NavPoint>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, NavPoint obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.TextColored("Navpoint editing hasn't been implemented yet. It won't be added until SP map editing is added.", .Red);
        //TODO: Implement. Not needed yet for MP so not yet implemented.
    }
}

[RegisterInspector(typeof(CoverNode))]
public static class CoverNodeInspector : IZoneObjectInspector<CoverNode>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, CoverNode obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.TextColored("Cover node editing hasn't been implemented yet. It won't be added until SP map editing is added.", .Red);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(RfgConstraint))]
public static class RfgConstraintInspector : IZoneObjectInspector<RfgConstraint>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, RfgConstraint obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.TextColored("Physics constraint editing hasn't been implemented yet. It won't be added until SP map editing is added.", .Red);
        //TODO: Implement
    }
}

[RegisterInspector(typeof(ActionNode))]
public static class ActionNodeInspector : IZoneObjectInspector<ActionNode>
{
    public static void Init()
    {

    }

    public static void Draw(App app, MapEditorDocument editor, Zone zone, ActionNode obj)
    {
        BaseZoneObjectInspector.Draw(app, editor, zone, obj);
        ImGui.TextColored("Action node editing hasn't been implemented yet. It won't be added until SP map editing is added.", .Red);
        //TODO: Implement
    }
}