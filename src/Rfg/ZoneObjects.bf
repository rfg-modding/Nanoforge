using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;
using Nanoforge.Misc;

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
        public Mat3 Orient = .Identity;

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
        //public OptionalField<f32> WindMaxSpeed = .Empty;
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
        public bool Dynamic;
        [EditorProperty]
        public u32 ChunkUID;
        [EditorProperty]
        public OptionalObject<String> Props = new .(new .()) ~delete _; //Name of entry in level_objects.xtbl
        [EditorProperty]
        //TODO: Populate with chunk assets in the .asm_pc for the current map
        public OptionalObject<String> ChunkName = new .(new .()) ~delete _;
        [EditorProperty]
        public u32 DestroyableUID;
        [EditorProperty]
        public u32 ShapeUID;
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
        public u32 OriginalObject;
        [EditorProperty]
        //TODO: This is only used by the game with specific mover flags. Define the logic for that and show/hide in the UI depending on that. Same applies to Idx and MoveType
        public u32 CollisionType; //TODO: Determine if this is really a bitflag defining collision layers. Some example collision types in the RfgTools zonex doc for general_mover
        [EditorProperty]
        public OptionalValue<u32> Idx = .(0);
        [EditorProperty]
        public OptionalValue<u32> MoveType = .(0); //Todo: See if this is the same as RfgMover.MoveTypeEnum
        [EditorProperty]
        public u32 DestructionUID;
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

        //Disabled these for now in favor of setting same values using gameplay_props.xtbl value inherited from ObjectMover
        //[EditorProperty]
        //public i32 NumSalvagePieces;
        //[EditorProperty]
        //public i32 GameDestroyedPct;
        //[EditorProperty]
        //public i32 StreamOutPlayTime;

        //TODO: Enable once buffer support is added to ProjectDB
        //[EditorProperty]
        //public u8[] WorldAnchors;
        //[EditorProperty]
        //public u8[] DynamicLinks;

        public ProjectBuffer WorldAnchors;
        public ProjectBuffer DynamicLinks;

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
        [EditorProperty]
        public i16 ShapeCutterFlags;
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
        public String EffectType = new .() ~delete _; //Name of an entry in effects.xtbl
    }

    [ReflectAll]
    public class Item : ZoneObject
    {
        [EditorProperty]
        public String ItemType = new .() ~delete _; //Name of an entry in items_3d.xtbl for plain items
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
        public bool Enabled;
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
        public MultiBackpackType BackpackType;
        [EditorProperty]
        public i32 NumBackpacks;
        [EditorProperty]
        public bool RandomBackpacks;
        [EditorProperty]
        public i32 Group;

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
        public Team MpTeam;
    }

    [ReflectAll]
    public class NavPoint : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
        public int Dummy; //Dummy variable to fix Bon not detecting reflection info on this. TODO: Fix once map export is implemented. More important features to focus on for now.
    }

    [ReflectAll]
    public class CoverNode : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
        public int Dummy;
    }

    [ReflectAll]
    public class RfgConstraint : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the mp_wasteland map can be loaded. Full support for this class will be added along with SP map editing.
        public int Dummy;
    }

    [ReflectAll]
    public class ActionNode : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
        public int Dummy;
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

    [ReflectAll]
    public enum MultiBackpackType
    {
        [RfgName("Commando")]
        Commando,

        [RfgName("Fleetfoot")]
        Fleetfoot,

        [RfgName("Heal")]
        Heal,

        [RfgName("Jetpack")]
        Jetpack,

        [RfgName("Kaboom")]
        Kaboom,

        [RfgName("Rhino")]
        Rhino,

        [RfgName("Shockwave")]
        Shockwave,

        [RfgName("Stealth")]
        Stealth,

        [RfgName("Thrust")]
        Thrust,

        [RfgName("Tremor")]
        Tremor,

        [RfgName("Vision")]
        Vision
    }
}