using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;
using Nanoforge.Misc;
using Nanoforge.Render.Resources;
using Bon;

namespace Nanoforge.Rfg
{
    //Most objects seen in game are zone objects. With the exceptions of terrain, rocks, and grass.
    [ReflectAll]
    public class ZoneObject : EditorObject
    {
        //Used for the RFG representation of handles in import & export
        public const u32 NullHandle = u32.MaxValue;

        [EditorProperty]
        public String Classname = new .() ~delete _;
        [EditorProperty]
        public u32 Handle;
        [EditorProperty]
        public u32 Num;
        [EditorProperty]
        public Flags Flags;
        [EditorProperty]
        public BoundingBox BBox;
        [EditorProperty]
        public ZoneObject Parent = null;
        [EditorProperty]
        public List<ZoneObject> Children = new .() ~delete _;

        public bool Persistent
        {
            get
            {
                return (Flags & .Persistent) != 0;
            }
            set
            {
                if (value)
                {
                    Flags |= .Persistent;
                }
                else
                {
                    Flags &= (~.Persistent);
                }
            }
        }

        [EditorProperty]
        public Vec3 Position = .Zero;
        [EditorProperty]
        public OptionalValue<Mat3> Orient = .(.Identity);
        [EditorProperty]
        public OptionalObject<String> RfgDisplayName = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalObject<String> Description = new .(new String()) ~delete _;

        //Reference to renderer data. Only set for objects which have visible mesh in the editor
        [BonIgnore]
        public RenderObject RenderObject = null;

        [Reflect(.All)]
        public enum Flags : u16
        {
            None                     = 0,
            Persistent               = 1 << 0,
            Flag1                    = 1 << 1,
            Flag2                    = 1 << 2,
            Flag3                    = 1 << 3,
            Flag4                    = 1 << 4,
            Flag5                    = 1 << 5,
            Flag6                    = 1 << 6,
            Flag7                    = 1 << 7,
            SpawnInAnarchy           = 1 << 8,
            SpawnInTeamAnarchy       = 1 << 9,
            SpawnInCTF               = 1 << 10,
            SpawnInSiege             = 1 << 11,
            SpawnInDamageControl     = 1 << 12,
            SpawnInBagman            = 1 << 13,
            SpawnInTeamBagman        = 1 << 14,
            SpawnInDemolition        = 1 << 15
        }
    }

    [ReflectAll]
    public class ObjZone : ZoneObject
    {
        [EditorProperty]
        public OptionalObject<String> AmbientSpawn = new .(new .()) ~delete _;
        [EditorProperty]
        public OptionalObject<String> SpawnResource = new .(new .()) ~delete _;
        [EditorProperty]
        public String TerrainFileName = new .() ~delete _;
        [EditorProperty]
        public OptionalValue<f32> WindMinSpeed = .(0.0f);
        [EditorProperty]
        public OptionalValue<f32> WindMaxSpeed = .(0.0f);
    }

    [ReflectAll]
    public class ObjectBoundingBox : ZoneObject
    {
        [EditorProperty]
        public BoundingBox LocalBBox;
        [EditorProperty]
        public OptionalValue<BoundingBoxType> BBType = .(.None);

        [Reflect(.All)]
        public enum BoundingBoxType
        {
            [RfgName("None")]
            None,

            [RfgName("GPS Target")]
            GpsTarget
        }
    }

    [ReflectAll]
    public class ObjectDummy : ZoneObject
    {
        [EditorProperty]
        public DummyType DummyType;

        [Reflect(.All)]
        public enum DummyType
        {
            [RfgName("None")]
            None,

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
    }

    [ReflectAll]
    public class PlayerStart : ZoneObject
    {
        [EditorProperty]
        public bool Indoor;
        [EditorProperty]
        public OptionalValue<Team> MpTeam = .(.None);
        [EditorProperty]
        public bool InitialSpawn;
        [EditorProperty]
        public bool Respawn;
        [EditorProperty]
        public bool CheckpointRespawn;
        [EditorProperty]
        public bool ActivityRespawn;
        [EditorProperty]
        public OptionalObject<String> MissionInfo = new .(new .()) ~delete _;
    }

    [ReflectAll]
    public class TriggerRegion : ZoneObject
    {
        [EditorProperty]
        public OptionalValue<Shape> TriggerShape = .(.Box);
        [EditorProperty]
        public BoundingBox LocalBBox;
        [EditorProperty]
        public f32 OuterRadius;
        [EditorProperty]
        public bool Enabled;
        [EditorProperty]
        public OptionalValue<RegionTypeEnum> RegionType = .(.Default);
        [EditorProperty]
        public OptionalValue<KillTypeEnum> KillType = .(.Cliff);
        [EditorProperty]
        public OptionalValue<TriggerRegionFlags> TriggerFlags = .(.None);

        [Reflect(.All)]
        public enum Shape
        {
            [RfgName("box")]
            Box,

            [RfgName("sphere")]
            Sphere
        }
        [Reflect(.All)]
        public enum RegionTypeEnum
        {
            [RfgName("default")]
            Default,

            [RfgName("kill human")]
            KillHuman
        }
        [Reflect(.All)]
        public enum KillTypeEnum
        {
            [RfgName("cliff")]
            Cliff,

            [RfgName("mine")]
            Mine
        }
        [Reflect(.All)]
        public enum TriggerRegionFlags
        {
            None = 0,

            [RfgName("not_in_activity")]
            NotInActivity = 1,

            [RfgName("not_in_mission")]
            NotInMission = 2
        }
    }

    [ReflectAll]
    public class ObjectMover : ZoneObject
    {
        [EditorProperty]
        public BuildingTypeFlagsEnum BuildingType;
        [EditorProperty]
        public u32 DestructionChecksum;
        [EditorProperty]
        public OptionalObject<String> GameplayProps = new .(new .()) ~delete _; //Name of an entry in gameplay_properties.xtbl
        //[EditorProperty]
        //public u32 InternalFlags; //Note: Disabled for the moment since it appears to be for runtime use only. Will add once its tested more thoroughly.
        [EditorProperty]
        public OptionalValue<ChunkFlagsEnum> ChunkFlags = .(.None);
        [EditorProperty]
        public OptionalValue<bool> Dynamic = .(false);
        [EditorProperty]
        public OptionalValue<u32> ChunkUID = .(0);
        [EditorProperty]
        public OptionalObject<String> Props = new .(new .()) ~delete _; //Name of entry in level_objects.xtbl
        [EditorProperty]
        //TODO: Populate with chunk assets in the .asm_pc for the current map
        public OptionalObject<String> ChunkName = new .(new .()) ~delete _;
        [EditorProperty]
        public u32 DestroyableUID;
        [EditorProperty]
        public OptionalValue<u32> ShapeUID = .(0);
        [EditorProperty]
        public OptionalValue<Team> Team = .(.None);
        [EditorProperty]
        public OptionalValue<f32> Control = .(0.0f);

        //Data on the chunk mesh, textures, and subpieces. This data isn't in the rfgzone_pc files. It's loaded from other files during map import.
        public Chunk ChunkData = null;

        [Reflect(.All)]
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
        [Reflect(.All)]
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
    }

    [ReflectAll]
    public class GeneralMover : ObjectMover
    {
        [EditorProperty]
        public u32 GmFlags;
        [EditorProperty]
        public OptionalValue<u32> OriginalObject = .(0);
        [EditorProperty]
        //TODO: This is only used by the game with specific mover flags. Define the logic for that and show/hide in the UI depending on that. Same applies to Idx and MoveType
        public OptionalValue<u32> CollisionType = .(0); //TODO: Determine if this is really a bitflag defining collision layers. Some example collision types in the RfgTools zonex doc for general_mover
        [EditorProperty]
        public OptionalValue<u32> Idx = .(0);
        [EditorProperty]
        public OptionalValue<u32> MoveType = .(0); //Todo: See if this is the same as RfgMover.MoveTypeEnum
        [EditorProperty]
        public OptionalValue<u32> DestructionUID = .(0);
        [EditorProperty]
        public OptionalValue<i32> Hitpoints = .(0);
        [EditorProperty]
        public OptionalValue<i32> DislodgeHitpoints = .(0);
    }

    [ReflectAll]
    public class RfgMover : ObjectMover
    {
        [EditorProperty]
        public MoveTypeEnum MoveType;
        //[EditorProperty]
        //public RfgMoverFlags Flags; //TODO: Implement. Currently disabled since it appears to be intended for runtime use only
        [EditorProperty]
        public OptionalValue<f32> DamagePercent = .(0.0f);
        [EditorProperty]
        public OptionalValue<f32> GameDestroyedPercentage = .(0.0f);

        //Disabled these for now in favor of setting same values using gameplay_props.xtbl value inherited from ObjectMover
        //[EditorProperty]
        //public i32 NumSalvagePieces;
        //[EditorProperty]
        //public i32 StreamOutPlayTime;

        [EditorProperty]
        public OptionalObject<u8[]> WorldAnchors = new .(null) ~delete _;
        [EditorProperty]
        public OptionalObject<u8[]> DynamicLinks = new .(null) ~delete _;

        [Reflect(.All)]
        public enum MoveTypeEnum
        {
            Fixed = 0,
            Normal = 1,
            Lite = 2,
            UltraLite = 3,
            WorldOnly = 4,
            NoCollision = 5
        }
    }

    [ReflectAll]
    public class ShapeCutter : ZoneObject
    {
        //TODO: Figure out what the valid values are for this and make it an enum
        [EditorProperty]
        public OptionalValue<i16> ShapeCutterFlags = .(0);
        [EditorProperty]
        public OptionalValue<f32> OuterRadius = .(0.0f);
        [EditorProperty]
        public OptionalValue<u32> Source = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> Owner = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u8> ExplosionId = .(u8.MaxValue);
    }

    [ReflectAll]
    public class ObjectEffect : ZoneObject
    {
        [EditorProperty]
        public OptionalObject<String> EffectType = new .(new String()) ~delete _; //Name of an entry in effects.xtbl
        [EditorProperty]
        public OptionalObject<String> SoundAlr = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalObject<String> Sound = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalObject<String> Visual = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalValue<bool> Looping = .(false);
    }

    [ReflectAll]
    public class Item : ZoneObject
    {
        [EditorProperty]
        public OptionalObject<String> ItemType = new .(new String()) ~delete _; //Name of an entry in items_3d.xtbl for plain items
        [EditorProperty]
        public OptionalValue<bool> Respawns = .(false);
        [EditorProperty]
        public OptionalValue<bool> Preplaced = .(false);
    }

    [ReflectAll]
    public class Weapon : Item
    {
        [EditorProperty]
        public String WeaponType = new .() ~delete _; //Name of an entry in weapons.xtbl
    }

    [ReflectAll]
    public class Ladder : ZoneObject
    {
        [EditorProperty]
        public i32 LadderRungs;
        [EditorProperty]
        public OptionalValue<bool> LadderEnabled = .(false);
    }

    [ReflectAll]
    public class ObjLight : ZoneObject
    {
        [EditorProperty]
        public OptionalValue<f32> AttenuationStart = .(0.0f);
        [EditorProperty]
        public OptionalValue<f32> AttenuationEnd = .(0.0f);
        [EditorProperty]
        public OptionalValue<f32> AttenuationRange = .(0.0f);
        [EditorProperty]
        public OptionalValue<ObjLightFlags> LightFlags = .(.None);
        [EditorProperty]
        public OptionalValue<LightTypeEnum> LightType = .(.Omni); //TODO: Change this to use light_type property so we can use all light types instead of just spot and omni
        [EditorProperty]
        public Vec3 Color;
        [EditorProperty]
        public OptionalValue<f32> HotspotSize = .(1.0f);
        [EditorProperty]
        public OptionalValue<f32> HotspotFalloffSize = .(1.0f);
        [EditorProperty]
        public Vec3 MinClip;
        [EditorProperty]
        public Vec3 MaxClip;
        //This needs verification before being re-enabled. Looks like it should actually be a string with a max of 68 characters.
        /*[EditorProperty]
        public i32 ClipMesh;*/

        [Reflect(.All)]
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
        [Reflect(.All)]
        public enum LightTypeEnum
        {
            [RfgName("spot")]
            Spot,

            [RfgName("omni")]
            Omni
        }
    }

    [ReflectAll]
    public class MultiMarker : ZoneObject
    {
        [EditorProperty]
        public BoundingBox LocalBBox;
        [EditorProperty]
        public MultiMarkerType MarkerType;
        [EditorProperty]
        public Team MpTeam;
        [EditorProperty]
        public i32 Priority;

        //These 4 are only loaded by the game when MarkerType == BackpackRack
        [EditorProperty]
        public OptionalObject<String> BackpackType = new .(new String()..Append("Jetpack")) ~delete _;
        [EditorProperty]
        public OptionalValue<i32> NumBackpacks = .(1);
        [EditorProperty]
        public OptionalValue<bool> RandomBackpacks = .(false);
        [EditorProperty]
        public OptionalValue<i32> Group = .(1);

        [Reflect(.All)]
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
    }

    [ReflectAll]
    public class MultiFlag : Item
    {
        [EditorProperty]
        public Team MpTeam;
    }

    [ReflectAll]
    public class NavPoint : ZoneObject
    {
        [EditorProperty]
        public OptionalObject<u8[]> NavpointData = new .(null) ~delete _;
        [EditorProperty]
        public OptionalValue<NavpointType> NavpointType = .(.Ground);
        [EditorProperty]
        public OptionalValue<f32> OuterRadius = .(0.5f);
        [EditorProperty]
        public OptionalValue<f32> SpeedLimit = .(10.0f);
        [EditorProperty]
        public OptionalValue<bool> DontFollowRoad;
        [EditorProperty]
        public OptionalValue<bool> IgnoreLanes;
        [EditorProperty]
        public OptionalObject<u8[]> Links = new .(null) ~delete _;
        [EditorProperty]
        public OptionalObject<u8[]> NavLinks = new .(null) ~delete _;
        [EditorProperty]
        public OptionalValue<Mat3> NavOrient = .(.Identity);

        [Reflect(.All)]
        public enum NavpointType : u8
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
    }

    [ReflectAll]
    public class CoverNode : ZoneObject
    {
        //TODO: Figure out what data these hold and store them in actual fields instead of using buffers
        [EditorProperty]
        public OptionalObject<u8[]> CovernodeData = new .(null) ~delete _;
        [EditorProperty]
        public OptionalObject<u8[]> OldCovernodeData = new .(null) ~delete _;

        [EditorProperty]
        public OptionalValue<f32> DefensiveAngleLeft = .(90.0f);
        [EditorProperty]
        public OptionalValue<f32> DefensiveAngleRight = .(90.0f);
        [EditorProperty]
        public OptionalValue<f32> OffensiveAngleLeft = .(90.0f);
        [EditorProperty]
        public OptionalValue<f32> OffensiveAngleRight = .(90.0f);
        [EditorProperty]
        public OptionalValue<f32> AngleLeft = .(90.0f);
        [EditorProperty]
        public OptionalValue<f32> AngleRight = .(90.0f);
        [EditorProperty]
        public OptionalValue<CoverNodeFlags> CoverNodeFlags = .(.None);
        [EditorProperty]
        public OptionalObject<String> Stance = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalObject<String> FiringFlags = new .(new String()) ~delete _;

        //The game uses bitfields stored in a u16 for this. Using a u16 bitflag enum instead for convenience.
        [Reflect(.All)]
        public enum CoverNodeFlags : u16
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
    }

    [ReflectAll]
    public class RfgConstraint : ZoneObject
    {
        //TODO: Convert this to fields. Storing it in a buffer temporarily since constraints aren't needed for MP map editing. We'll need the actual fields when SP editing is added.
        [EditorProperty]
        public OptionalObject<u8[]> Template = new .(null) ~delete _;
        [EditorProperty]
        public OptionalValue<u32> HostHandle = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> ChildHandle = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> HostIndex = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> ChildIndex = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> HostHavokAltIndex = .(u32.MaxValue);
        [EditorProperty]
        public OptionalValue<u32> ChildHavokAltIndex = .(u32.MaxValue);
    }

    [ReflectAll]
    public class ActionNode : ZoneObject
    {
        //Called `animation_type` in the zone files. Used different name here since it makes more sense. References an entry in action_node_types.xtbl
        [EditorProperty]
        public OptionalObject<String> ActionNodeType = new .(new String()) ~delete _;
        [EditorProperty]
        public OptionalValue<bool> HighPriority = .(false);
        [EditorProperty]
        public OptionalValue<bool> InfiniteDuration = .(false);
        [EditorProperty]
        public OptionalValue<i32> DisabledBy = .(0);
        [EditorProperty]
        public OptionalValue<bool> RunTo = .(false);
        [EditorProperty]
        public OptionalValue<f32> OuterRadius = .(1.0f);

        //TODO: Convert these to their actual data structure. Using buffers for now since we don't need these for MP map editing.
        [EditorProperty]
        public OptionalObject<u8[]> ObjLinks = new .(null) ~delete _;
        [EditorProperty]
        public OptionalObject<u8[]> Links = new .(null) ~delete _;
    }

    [ReflectAll]
    public enum Team
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
}