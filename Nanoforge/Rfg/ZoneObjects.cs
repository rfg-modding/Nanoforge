using System;
using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using RFGM.Formats.Math;
using Nanoforge.Misc;
using Nanoforge.Render.Resources;

namespace Nanoforge.Rfg;

//Most objects seen in game are zone objects. With the exceptions of terrain, rocks, and grass.
public class ZoneObject : EditorObject
{
    //Used for the RFG representation of handles in import & export
    public const uint NullHandle = uint.MaxValue;
    public string Classname = "";
    public uint Handle;
    public uint Num;
    public ZoneObjectFlags Flags;
    public BoundingBox BBox = BoundingBox.Zero;
    public ZoneObject? Parent = null;
    public List<ZoneObject> Children = new();

    public bool Persistent
    {
        get => Flags.HasFlag(ZoneObjectFlags.Persistent);
        set
        {
            if (value)
            {
                Flags |= ZoneObjectFlags.Persistent;
            }
            else
            {
                Flags &= (~ZoneObjectFlags.Persistent);
            }
        }
    }

    public Vector3 Position = new Vector3();

    //Note: This is really a 3x3 matrix that we load from the rfgzone_pc/layer_pc file. Just using Matrix4x4 since .NET doesn't have a 3x3 matrix built in.
    public Toggleable<Matrix4x4> Orient = new(Matrix4x4.Identity);
    
    //We track pitch/yaw/roll to generate rotation delta matrices whenever they change.
    //This avoids issues with gimbal lock and indeterminate angles that we'd get if we tried to extract the euler angles from the matrix every time we edited it.
    public float Pitch;
    public float Yaw;
    public float Roll;

    public Toggleable<string> RfgDisplayName = new("");
    public Toggleable<string> Description = new("");

    //Reference to renderer data. Only set for objects which have visible mesh in the editor
    public RenderObject? RenderObject = null;

    [Flags]
    public enum ZoneObjectFlags : ushort
    {
        None = 0,
        Persistent = 1 << 0,
        Flag1 = 1 << 1,
        Flag2 = 1 << 2,
        Flag3 = 1 << 3,
        Flag4 = 1 << 4,
        Flag5 = 1 << 5,
        Flag6 = 1 << 6,
        Flag7 = 1 << 7,
        SpawnInAnarchy = 1 << 8,
        SpawnInTeamAnarchy = 1 << 9,
        SpawnInCtf = 1 << 10,
        SpawnInSiege = 1 << 11,
        SpawnInDamageControl = 1 << 12,
        SpawnInBagman = 1 << 13,
        SpawnInTeamBagman = 1 << 14,
        SpawnInDemolition = 1 << 15
    }

    public override ZoneObject Clone()
    {
        var clone = (ZoneObject)base.Clone();
        clone.Classname = new string(this.Classname);
        clone.Orient = new Toggleable<Matrix4x4>(this.Orient.Value);
        clone.RfgDisplayName = new Toggleable<string>(new string(this.RfgDisplayName.Value));
        clone.Description = new Toggleable<string>(new string(this.Description.Value));

        //Note: RFG objects can't share children.
        clone.Children.Clear();

        //Note: It's the responsibility of the caller to clone the RenderObject since they have handles to the Scene
        clone.RenderObject = null;

        return clone;
    }

    public void AddChild(ZoneObject child)
    {
        if (!Children.Contains(child))
        {
            Children.Add(child);
        }
        child.Parent = this;
    }
}

public class ObjZone : ZoneObject
{
    public Toggleable<string> AmbientSpawn = new("");
    public Toggleable<string> SpawnResource = new("");
    public string TerrainFileName = "";
    public Toggleable<float> WindMinSpeed = new(0.0f);
    public Toggleable<float> WindMaxSpeed = new(0.0f);

    public override ObjZone Clone()
    {
        var clone = (ObjZone)base.Clone();
        clone.AmbientSpawn = new Toggleable<string>(new string(this.AmbientSpawn.Value));
        clone.SpawnResource = new Toggleable<string>(new string(this.SpawnResource.Value));
        clone.TerrainFileName = new string(this.TerrainFileName);
        clone.WindMinSpeed = new Toggleable<float>(this.WindMinSpeed.Value);
        clone.WindMaxSpeed = new Toggleable<float>(this.WindMaxSpeed.Value);
        return clone;
    }
}

public class ObjectBoundingBox : ZoneObject
{
    public BoundingBox LocalBBox = BoundingBox.Zero;

    // ReSharper disable once InconsistentNaming
    public Toggleable<BoundingBoxType> BBType = new(BoundingBoxType.None);

    public enum BoundingBoxType
    {
        [RfgName("None")] None,

        [RfgName("GPS Target")] GpsTarget
    }

    public override ObjectBoundingBox Clone()
    {
        var clone = (ObjectBoundingBox)base.Clone();
        clone.BBType = new Toggleable<BoundingBoxType>(this.BBType.Value);
        return clone;
    }
}

public class ObjectDummy : ZoneObject
{
    public DummyTypeEnum DummyType = DummyTypeEnum.None;
    
    public enum DummyTypeEnum
    {
        [RfgName("None")]
        None,

        // ReSharper disable once StringLiteralTypo
        [RfgName("Tech Reponse Pos")] //That typo is in the game. Do not fix it or the game won't load this field from zone files correctly anymore
        TechResponsePos,

        [RfgName("VRail Spawn")]
        VRailSpawn,

        [RfgName("Demo Master")]
        DemoMaster,

        [RfgName("Cutscene")]
        Cutscene,

        [RfgName("Air Bomb")]
        AirBomb,

        [RfgName("Rally")]
        Rally,

        [RfgName("Barricade")]
        Barricade,

        [RfgName("Reinforced_Fence")]
        ReinforcedFence,

        [RfgName("Smoke Plume")]
        SmokePlume,

        [RfgName("Demolition")]
        Demolition
    }

    public override ObjectDummy Clone()
    {
        var clone = (ObjectDummy)base.Clone();
        return clone;
    }
}

public class PlayerStart : ZoneObject
{
    public bool Indoor;
    public Toggleable<RfgTeam> MpTeam = new(RfgTeam.None);
    public bool InitialSpawn;
    public bool Respawn;
    public bool CheckpointRespawn;
    public bool ActivityRespawn;
    public Toggleable<string> MissionInfo = new("");

    public override PlayerStart Clone()
    {
        var clone = (PlayerStart)base.Clone();
        clone.MpTeam = new Toggleable<RfgTeam>(this.MpTeam.Value);
        clone.MissionInfo = new Toggleable<string>(new (this.MissionInfo.Value));
        return clone;
    }
}

public class TriggerRegion : ZoneObject
{
    public Toggleable<Shape> TriggerShape = new(Shape.Box);
    public BoundingBox LocalBBox = BoundingBox.Zero;
    public float OuterRadius;
    public bool Enabled;
    public Toggleable<RegionTypeEnum> RegionType = new(RegionTypeEnum.Default);
    public Toggleable<KillTypeEnum> KillType = new(KillTypeEnum.Cliff);
    public Toggleable<TriggerRegionFlags> TriggerFlags = new(TriggerRegionFlags.None);

    public enum Shape
    {
        [RfgName("box")]
        Box,

        [RfgName("sphere")]
        Sphere
    }

    public enum RegionTypeEnum
    {
        [RfgName("default")]
        Default,

        [RfgName("kill human")]
        KillHuman
    }

    public enum KillTypeEnum
    {
        [RfgName("cliff")]
        Cliff,

        [RfgName("mine")]
        Mine
    }

    [Flags]
    public enum TriggerRegionFlags
    {
        None = 0,

        [RfgName("not_in_activity")]
        NotInActivity = 1,

        [RfgName("not_in_mission")]
        NotInMission = 2
    }

    public override TriggerRegion Clone()
    {
        var clone = (TriggerRegion)base.Clone();
        clone.TriggerShape = new(this.TriggerShape.Value);
        clone.RegionType = new(this.RegionType.Value);
        clone.KillType = new(this.KillType.Value);
        clone.TriggerFlags = new(this.TriggerFlags.Value);
        return clone;
    }
}

public class ObjectMover : ZoneObject
{
    public BuildingTypeFlagsEnum BuildingType;
    public uint DestructionChecksum;
    public Toggleable<string> GameplayProps = new(""); // Name of an entry in gameplay_properties.xtbl
    // public uint InternalFlags; // Note: Disabled for the moment since it appears to be for runtime use only. Will add once it's tested more thoroughly.
    public Toggleable<ChunkFlagsEnum> ChunkFlags = new(ChunkFlagsEnum.None);
    public Toggleable<bool> Dynamic = new(false);
    public Toggleable<uint> ChunkUID = new(0);
    public Toggleable<string> Props = new(""); // Name of entry in level_objects.xtbl
    // TODO: Populate with chunk assets in the .asm_pc for the current map
    public Toggleable<string> ChunkName = new("");
    public uint DestroyableUID;
    public Toggleable<uint> ShapeUID = new(0);
    public Toggleable<RfgTeam> Team = new(RfgTeam.None);
    public Toggleable<float> Control = new(0.0f);

    // Data on the chunk mesh, textures, and subpieces. This data isn't in the rfgzone_pc files. It's loaded from other files during map import.
    public Chunk? ChunkData = null;

    [Flags]
    public enum BuildingTypeFlagsEnum
    {
        None = 0,

        [RfgName("Dynamic")]
        Dynamic = 1 << 0,

        [RfgName("Force_Field")]
        ForceField = 1 << 1,

        [RfgName("Bridge")]
        Bridge = 1 << 2,

        [RfgName("Raid")]
        Raid = 1 << 3,

        [RfgName("House")]
        House = 1 << 4,

        [RfgName("Player_base")]
        PlayerBase = 1 << 5,

        [RfgName("Communications")]
        Communications = 1 << 6
    }

    [Flags]
    public enum ChunkFlagsEnum
    {
        None = 0,

        [RfgName("child_gives_control")]
        ChildGivesControl = 1 << 0,

        [RfgName("building")]
        Building = 1 << 1,

        [RfgName("dynamic_link")]
        DynamicLink = 1 << 2,

        [RfgName("world_anchor")]
        WorldAnchor = 1 << 3,

        [RfgName("no_cover")]
        NoCover = 1 << 4,

        [RfgName("propaganda")]
        Propaganda = 1 << 5,

        [RfgName("kiosk")]
        Kiosk = 1 << 6,

        [RfgName("touch_terrain")]
        TouchTerrain = 1 << 7,

        [RfgName("supply_crate")]
        SupplyCrate = 1 << 8,

        [RfgName("mining")]
        Mining = 1 << 9,

        [RfgName("one_of_many")]
        OneOfMany = 1 << 10,

        [RfgName("plume_on_death")]
        PlumeOnDeath = 1 << 11,

        [RfgName("invulnerable")]
        Invulnerable = 1 << 12,

        [RfgName("inherit_damage_pct")]
        InheritDamagePercentage = 1 << 13,

        [RfgName("regrow_on_stream")]
        RegrowOnStream = 1 << 14,

        [RfgName("casts_drop_shadow")]
        CastsDropShadow = 1 << 15,

        [RfgName("disable_collapse_effect")]
        DisableCollapseEffect = 1 << 16,

        [RfgName("force_dynamic")]
        ForceDynamic = 1 << 17,

        [RfgName("show_on_map")]
        ShowOnMap = 1 << 18,

        [RfgName("regenerate")]
        Regenerate = 1 << 19,

        [RfgName("casts_shadow")]
        CastsShadow = 1 << 20
    }

    public override ObjectMover Clone()
    {
        var clone = (ObjectMover)base.Clone();
        clone.GameplayProps = new(new string(this.GameplayProps.Value));
        clone.ChunkFlags = new(this.ChunkFlags.Value);
        clone.Dynamic = new(this.Dynamic.Value);
        clone.ChunkUID = new(this.ChunkUID.Value);
        clone.Props = new(new string(this.Props.Value));
        clone.ChunkName = new(new string(this.ChunkName.Value));
        clone.ShapeUID = new(this.ShapeUID.Value);
        clone.Team = new(this.Team.Value);
        clone.Control = new(this.Control.Value);
        //Note: Has the same ChunkData instance since it just holds metadata like the buffers to load the meshes and textures from. Doesn't hold any instance specific data.
        return clone;
    }
}

public class GeneralMover : ObjectMover
{
    public uint GmFlags;
    public Toggleable<uint> OriginalObject = new(0);
    //TODO: This is only used by the game with specific mover flags. Define the logic for that and show/hide in the UI depending on that. Same applies to Idx and MoveType
    public Toggleable<uint> CollisionType = new(0); //TODO: Determine if this is really a bitflag defining collision layers. Some example collision types in the RfgTools zonex doc for general_mover
    public Toggleable<uint> Idx = new(0);
    public Toggleable<uint> MoveType = new(0); //Todo: See if this is the same as RfgMover.MoveTypeEnum
    public Toggleable<uint> DestructionUID = new(0);
    public Toggleable<int> Hitpoints = new(0);
    public Toggleable<int> DislodgeHitpoints = new(0);

    public override ObjectMover Clone()
    {
        var clone = (GeneralMover)base.Clone();
        clone.OriginalObject = new(this.OriginalObject.Value);
        clone.CollisionType = new(this.CollisionType.Value);
        clone.Idx = new(this.Idx.Value);
        clone.MoveType = new(this.MoveType.Value);
        clone.DestructionUID = new(this.DestructionUID.Value);
        clone.Hitpoints = new(this.Hitpoints.Value);
        clone.DislodgeHitpoints = new(this.DislodgeHitpoints.Value);
        return clone;
    }
}

public class RfgMover : ObjectMover
{
    public MoveTypeEnum MoveType;
    //public RfgMoverFlags Flags; //TODO: Implement. Currently disabled since it appears to be intended for runtime use only
    public Toggleable<float> DamagePercent = new(0.0f);
    public Toggleable<float> GameDestroyedPercentage = new(0.0f);

    //Disabled these for now in favor of setting same values using gameplay_props.xtbl value inherited from ObjectMover
    //public int NumSalvagePieces;
    //public int StreamOutPlayTime;

    public Toggleable<byte[]?> WorldAnchors = new(null);
    public Toggleable<byte[]?> DynamicLinks = new(null);

    public enum MoveTypeEnum
    {
        Fixed = 0,
        Normal = 1,
        Lite = 2,
        UltraLite = 3,
        WorldOnly = 4,
        NoCollision = 5
    }

    public override RfgMover Clone()
    {
        var clone = (RfgMover)base.Clone();
        clone.DamagePercent = new(this.DamagePercent.Value);
        clone.GameDestroyedPercentage = new(this.GameDestroyedPercentage.Value);
        clone.WorldAnchors = DataHelper.DeepCopyToggleableBytes(this.WorldAnchors);
        clone.DynamicLinks = DataHelper.DeepCopyToggleableBytes(this.DynamicLinks);
        return clone;
    }
}

public class ShapeCutter : ZoneObject
{
    //TODO: Figure out what the valid values are for this and make it an enum
    public Toggleable<short> ShapeCutterFlags = new(0);
    public Toggleable<float> OuterRadius = new(0.0f);
    public Toggleable<uint> Source = new(uint.MaxValue);
    public Toggleable<uint> Owner = new(uint.MaxValue);
    public Toggleable<byte> ExplosionId = new(byte.MaxValue);

    public override ShapeCutter Clone()
    {
        var clone = (ShapeCutter)base.Clone();
        clone.ShapeCutterFlags = new(this.ShapeCutterFlags.Value);
        clone.OuterRadius = new(this.OuterRadius.Value);
        clone.Source = new(this.Source.Value);
        clone.Owner = new(this.Owner.Value);
        clone.ExplosionId = new(this.ExplosionId.Value);
        return clone;
    }
}

public class ObjectEffect : ZoneObject
{
    public Toggleable<string> EffectType = new("");
    public Toggleable<string> SoundAlr = new("");
    public Toggleable<string> Sound = new("");
    public Toggleable<string> Visual = new("");
    public Toggleable<bool> Looping = new(false);

    public override ObjectEffect Clone()
    {
        var clone = (ObjectEffect)base.Clone();
        clone.EffectType = new(new string(this.EffectType.Value));
        clone.SoundAlr = new(new string(this.SoundAlr.Value));
        clone.Sound = new(new string(this.Sound.Value));
        clone.Visual = new(new string(this.Visual.Value));
        clone.Looping = new(this.Looping.Value);
        return clone;
    }
}

public class Item : ZoneObject
{
    public Toggleable<string> ItemType = new("");
    public Toggleable<bool> Respawns = new(false);
    public Toggleable<bool> Preplaced = new(false);

    public override Item Clone()
    {
        var clone = (Item)base.Clone();
        clone.ItemType = new(new string(this.ItemType.Value));
        clone.Respawns = new(this.Respawns.Value);
        clone.Preplaced = new(this.Preplaced.Value);
        return clone;
    }
}

public class Weapon : Item
{
    public string WeaponType = ""; //Name of an entry in weapons.xtbl

    public override Weapon Clone()
    {
        var clone = (Weapon)base.Clone();
        clone.WeaponType = new string(this.WeaponType);
        return clone;
    }
}

public class Ladder : ZoneObject
{
    public int LadderRungs;
    public Toggleable<bool> LadderEnabled = new(false);

    public override Ladder Clone()
    {
        var clone = (Ladder)base.Clone();
        clone.LadderEnabled = new(this.LadderEnabled.Value);
        return clone;
    }
}

public class ObjLight : ZoneObject
{
    public Toggleable<float> AttenuationStart = new(0.0f);
    public Toggleable<float> AttenuationEnd = new(0.0f);
    public Toggleable<float> AttenuationRange = new(0.0f);
    public Toggleable<ObjLightFlags> LightFlags = new(ObjLightFlags.None);
    public Toggleable<LightTypeEnum> LightType = new(LightTypeEnum.Omni); //TODO: Change this to use light_type property so we can use all light types instead of just spot and omni
    public Vector3 Color;
    public Toggleable<float> HotspotSize = new(1.0f);
    public Toggleable<float> HotspotFalloffSize = new(1.0f);
    public Vector3 MinClip;
    public Vector3 MaxClip;
    //This needs verification before being re-enabled. Looks like it should actually be a string with a max of 68 characters.
    /*public int ClipMesh;*/

    [Flags]
    public enum ObjLightFlags
    {
        None = 0,
        
        [RfgName("use_clipping")]
        UseClipping = 1 << 0,
        
        [RfgName("daytime")]
        DayTime = 1 << 1,
        
        [RfgName("nighttime")]
        NightTime = 1 << 2
    }

    public enum LightTypeEnum
    {
        [RfgName("spot")]
        Spot,
        
        [RfgName("omni")]
        Omni
    }

    public override ObjLight Clone()
    {
        var clone = (ObjLight)base.Clone();
        clone.AttenuationStart = new(this.AttenuationStart.Value);
        clone.AttenuationEnd = new(this.AttenuationEnd.Value);
        clone.AttenuationRange = new(this.AttenuationRange.Value);
        clone.LightFlags = new(this.LightFlags.Value);
        clone.LightType = new(this.LightType.Value);
        clone.HotspotSize = new(this.HotspotSize.Value);
        clone.HotspotFalloffSize = new(this.HotspotFalloffSize.Value);
        return clone;
    }
}

public class MultiMarker : ZoneObject
{
    public BoundingBox LocalBBox = BoundingBox.Zero;
    public MultiMarkerType MarkerType;
    public RfgTeam MpTeam;
    public int Priority;

    //These 4 are only loaded by the game when MarkerType == BackpackRack
    public Toggleable<string> BackpackType = new("Jetpack");
    public Toggleable<int> NumBackpacks = new(1);
    public Toggleable<bool> RandomBackpacks = new(false);
    public Toggleable<int> Group = new(1);

    public enum MultiMarkerType
    {
        None,

        [RfgName("Siege target")]
        SiegeTarget,

        [RfgName("Backpack rack")]
        BackpackRack,

        [RfgName("Spawn node")]
        SpawnNode,

        [RfgName("Flag capture zone")]
        FlagCaptureZone,

        [RfgName("King of the Hill target")]
        KingOfTheHillTarget,

        [RfgName("Spectator camera")]
        SpectatorCamera
    }

    public override MultiMarker Clone()
    {
        var clone = (MultiMarker)base.Clone();
        clone.BackpackType = new(this.BackpackType.Value);
        clone.NumBackpacks = new(this.NumBackpacks.Value);
        clone.RandomBackpacks = new(this.RandomBackpacks.Value);
        clone.Group = new(this.Group.Value);
        return clone;
    }
}

public class MultiFlag : Item
{
    public RfgTeam MpTeam;

    public override MultiFlag Clone()
    {
        var clone = (MultiFlag)base.Clone();
        return clone;
    }
}

public class NavPoint : ZoneObject
{
    public Toggleable<byte[]?> NavpointData = new(null);
    public Toggleable<NavpointTypeEnum> NavpointType = new(NavpointTypeEnum.Ground);
    public Toggleable<float> OuterRadius = new(0.5f);
    public Toggleable<float> SpeedLimit = new(10.0f);
    public Toggleable<bool> DontFollowRoad = new(false);
    public Toggleable<bool> IgnoreLanes = new(false);
    public Toggleable<byte[]?> Links = new(null);
    public Toggleable<byte[]?> NavLinks = new(null);
    //NOTE: This is really a mat33 in the zone files. .NET doesn't have a 3x3 matrix built in so 4x4 is used.
    public Toggleable<Matrix4x4> NavOrient = new(Matrix4x4.Identity);

    public enum NavpointTypeEnum : byte
    {
        [RfgName("ground")]
        Ground = 0,

        [RfgName("patrol")]
        Patrol = 1,

        [RfgName("pedestrian")]
        Pedestrian = 2,

        [RfgName("floating")]
        Floating = 3,

        [RfgName("supply dropoff")]
        SupplyDropoff = 4,

        [RfgName("road")]
        Road = 5,

        [RfgName("road jump")]
        RoadJump = 6,

        [RfgName("road jump dest")]
        RoadJumpDestination = 7,

        [RfgName("road jump start")]
        RoadJumpStart = 8,

        [RfgName("road checkpoint")]
        RoadCheckpoint = 9,

        [RfgName("path")]
        Path = 10,

        [RfgName("transition")]
        Transition = 11,

        [RfgName("ladder transition")]
        LadderTransition = 12,

        [RfgName("ladder")]
        Ladder = 13,

        [RfgName("stairs")]
        Stairs = 14,

        [RfgName("transition jump")]
        TransitionJump = 15,

        [RfgName("transition leap over")]
        TransitionLeapOver = 16,

        [RfgName("unknown17")]
        Unknown17 = 17,

        [RfgName("unknown18")]
        Unknown18 = 18,

        [RfgName("unknown19")]
        Unknown19 = 19,

        [RfgName("unknown20")]
        Unknown20 = 20,

        [RfgName("unknown21")]
        Unknown21 = 21,

        [RfgName("road bridge")]
        RoadBridge = 22,

        [RfgName("path door")]
        PathDoor = 23,

        [RfgName("transition door")]
        TransitionDoor = 24
    }

    public override NavPoint Clone()
    {
        var clone = (NavPoint)base.Clone();
        clone.NavpointData = DataHelper.DeepCopyToggleableBytes(this.NavpointData);
        clone.NavpointType = new(this.NavpointType.Value);
        clone.OuterRadius = new(this.OuterRadius.Value);
        clone.SpeedLimit = new(this.SpeedLimit.Value);
        clone.DontFollowRoad = new(this.DontFollowRoad.Value);
        clone.IgnoreLanes = new(this.IgnoreLanes.Value);
        clone.Links = DataHelper.DeepCopyToggleableBytes(this.Links);
        clone.NavLinks = DataHelper.DeepCopyToggleableBytes(this.NavLinks);
        clone.NavOrient = new(this.NavOrient.Value);
        return clone;
    }
}

public class CoverNode : ZoneObject
{
    public Toggleable<byte[]?> CovernodeData = new(null);
    public Toggleable<byte[]?> OldCovernodeData = new(null);

    public Toggleable<float> DefensiveAngleLeft = new(90.0f);
    public Toggleable<float> DefensiveAngleRight = new(90.0f);
    public Toggleable<float> OffensiveAngleLeft = new(90.0f);
    public Toggleable<float> OffensiveAngleRight = new(90.0f);
    public Toggleable<float> AngleLeft = new(90.0f);
    public Toggleable<float> AngleRight = new(90.0f);
    public Toggleable<CoverNodeFlagsEnum> CoverNodeFlags = new(CoverNodeFlagsEnum.None);
    public Toggleable<string> Stance = new("");
    public Toggleable<string> FiringFlags = new("");

    [Flags]
    public enum CoverNodeFlagsEnum : ushort
    {
        None = 0,

        DisabledReason = 1 << 0,

        Dynamic = 1 << 1,

        Zone = 1 << 2,

        Vehicle = 1 << 3,

        Crouch = 1 << 4,

        Unknown5 = 1 << 5,

        Unknown6 = 1 << 6,

        Unknown7 = 1 << 7,

        FirePopup = 1 << 8,

        FireLeft = 1 << 9,

        FireRight = 1 << 10,

        OffNavmesh = 1 << 11,

        OnMover = 1 << 12,

        Unknown13 = 1 << 13,

        Unknown14 = 1 << 14,

        Unknown15 = 1 << 15,
    }

    public override CoverNode Clone()
    {
        var clone = (CoverNode)base.Clone();
        clone.CovernodeData = DataHelper.DeepCopyToggleableBytes(this.CovernodeData);
        clone.OldCovernodeData = DataHelper.DeepCopyToggleableBytes(this.OldCovernodeData);
        clone.DefensiveAngleLeft = new(this.DefensiveAngleLeft.Value);
        clone.DefensiveAngleRight = new(this.DefensiveAngleRight.Value);
        clone.OffensiveAngleLeft = new(this.OffensiveAngleLeft.Value);
        clone.OffensiveAngleRight = new(this.OffensiveAngleRight.Value);
        clone.AngleLeft = new(this.AngleLeft.Value);
        clone.AngleRight = new(this.AngleRight.Value);
        clone.CoverNodeFlags = new(this.CoverNodeFlags.Value);
        clone.Stance = new(new string(this.Stance.Value));
        clone.FiringFlags = new(new string(this.FiringFlags.Value));
        return clone;
    }
}

public class RfgConstraint : ZoneObject
{
    public Toggleable<byte[]?> Template = new(null);
    public Toggleable<uint> HostHandle = new(uint.MaxValue);
    public Toggleable<uint> ChildHandle = new(uint.MaxValue);
    public Toggleable<uint> HostIndex = new(uint.MaxValue);
    public Toggleable<uint> ChildIndex = new(uint.MaxValue);
    public Toggleable<uint> HostHavokAltIndex = new(uint.MaxValue);
    public Toggleable<uint> ChildHavokAltIndex = new(uint.MaxValue);

    public override RfgConstraint Clone()
    {
        var clone = (RfgConstraint)base.Clone();
        clone.Template = DataHelper.DeepCopyToggleableBytes(this.Template);
        clone.HostHandle = new(this.HostHandle.Value);
        clone.ChildHandle = new(this.ChildHandle.Value);
        clone.HostIndex = new(this.HostIndex.Value);
        clone.ChildIndex = new(this.ChildIndex.Value);
        clone.HostHavokAltIndex = new(this.HostHavokAltIndex.Value);
        clone.ChildHavokAltIndex = new(this.ChildHavokAltIndex.Value);
        
        return clone;
    }
}

public class ActionNode : ZoneObject
{
    public Toggleable<string> ActionNodeType = new("");
    public Toggleable<bool> HighPriority = new(false);
    public Toggleable<bool> InfiniteDuration = new(false);
    public Toggleable<int> DisabledBy = new(0);
    public Toggleable<bool> RunTo = new(false);
    public Toggleable<float> OuterRadius = new(1.0f);

    public Toggleable<byte[]?> ObjLinks = new(null);
    public Toggleable<byte[]?> Links = new(null);

    public override ActionNode Clone()
    {
        var clone = (ActionNode)base.Clone();
        clone.ActionNodeType = new(new string(this.ActionNodeType.Value));
        clone.HighPriority = new(this.HighPriority.Value);
        clone.InfiniteDuration = new(this.InfiniteDuration.Value);
        clone.DisabledBy = new(this.DisabledBy.Value);
        clone.RunTo = new(this.RunTo.Value);
        clone.OuterRadius = new(this.OuterRadius.Value);
        clone.ObjLinks = DataHelper.DeepCopyToggleableBytes(this.ObjLinks);
        clone.Links = DataHelper.DeepCopyToggleableBytes(this.Links);
        return clone;
    }
}

public enum RfgTeam
{
    [RfgName("None")]
    None,

    [RfgName("Neutral")]
    Neutral,

    [RfgName("Guerilla")]
    Guerilla,

    [RfgName("EDF")]
    EDF,

    [RfgName("Civilian")]
    Civilian,

    [RfgName("Marauder")]
    Marauder
}