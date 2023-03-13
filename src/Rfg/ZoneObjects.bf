using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;

namespace Nanoforge.Rfg
{
    //Most objects seen in game are zone objects. With the exceptions of terrain, rocks, and grass.
    public class ZoneObject : EditorObject
    {
        [EditorProperty]
        public append String Classname;
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
        public append List<ZoneObject> Children;

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

    public class ObjZone : ZoneObject
    {
        [EditorProperty]
        public append String AmbientSpawn;
        [EditorProperty]
        public append String SpawnResource;
        [EditorProperty]
        public append String TerrainFileName;
        [EditorProperty]
        public f32 WindMinSpeed = 50.0f;
        [EditorProperty]
        public f32 WindMaxSpeed = 80.0f;
    }

    public class ObjectBoundingBox : ZoneObject
    {
        [EditorProperty]
        public BoundingBox LocalBBox;
        [EditorProperty]
        public BoundingBoxType BBType;

        public enum BoundingBoxType
        {
            None,
            GpsTarget
        }
    }

    public class ObjectDummy : ZoneObject
    {
        [EditorProperty]
        public DummyType DummyType;

        public enum DummyType
        {
            None,
            TechResponsePos,
            VRailSpawn,
            DemoMaster,
            Cutscene,
            AirBomb,
            Rally,
            Barricade,
            ReinforcedFence,
            SmokePlume,
            Demolition
        }
    }

    public class PlayerStart : ZoneObject
    {
        [EditorProperty]
        public bool Indoor;
        [EditorProperty]
        public Team MpTeam;
        [EditorProperty]
        public bool InitialSpawn;
        [EditorProperty]
        public bool Respawn;
        [EditorProperty]
        public bool CheckpointRespawn;
        [EditorProperty]
        public bool ActivityRespawn;
        [EditorProperty]
        public append String MissionInfo;
    }

    public class TriggerRegion : ZoneObject
    {
        [EditorProperty]
        public Shape TriggerShape;
        [EditorProperty]
        public BoundingBox LocalBBox;
        [EditorProperty]
        public f32 OuterRadius;
        [EditorProperty]
        public bool Enabled;
        [EditorProperty]
        public RegionTypeEnum RegionType;
        [EditorProperty]
        public KillTypeEnum KillType;
        [EditorProperty]
        public TriggerRegionFlags TriggerFlags;

        public enum Shape
        {
            Box,
            Sphere
        }
        public enum RegionTypeEnum
        {
            Default,
            KillHuman
        }
        public enum KillTypeEnum
        {
            Cliff,
            Mine
        }
        public enum TriggerRegionFlags
        {
            None = 0,
            NotInActivity = 1,
            NotInMission = 2
        }
    }

    public class ObjectMover : ZoneObject
    {
        [EditorProperty]
        public BuildingTypeEnum BuildingType;
        [EditorProperty]
        public u32 DestructionChecksum;
        [EditorProperty]
        public append String GameplayProps; //Name of an entry in gameplay_properties.xtbl
        //[EditorProperty]
        //public u32 InternalFlags; //Note: Disabled for the moment since it appears to be for runtime use only. Will add once its tested more thoroughly.
        [EditorProperty]
        public ChunkFlagsEnum ChunkFlags;
        [EditorProperty]
        public bool Dynamic;
        [EditorProperty]
        public u32 ChunkUID;
        [EditorProperty]
        public append String Props; //Name of entry in level_objects.xtbl
        [EditorProperty]
        public append String ChunkName;
        [EditorProperty]
        public u32 DestroyableUID;
        [EditorProperty]
        public u32 ShapeUID;
        [EditorProperty]
        public Team Team;
        [EditorProperty]
        public f32 Control;

        public enum BuildingTypeEnum
        {
            None = 0,
            Dynamic = 1 << 0,
            ForceField = 1 << 1,
            Bridge = 1 << 2,
            Raid = 1 << 3,
            House = 1 << 4,
            PlayerBase = 1 << 5,
            Communications = 1 << 6
        }
        public enum ChunkFlagsEnum
        {
            None = 0,
            ChildGivesControl = 1 << 0,
            Building = 1 << 1,
            DynamicLink = 1 << 2,
            WorldAnchor = 1 << 3,
            NoCover = 1 << 4,
            Propaganda = 1 << 5,
            Kiosk = 1 << 6,
            TouchTerrain = 1 << 7,
            SupplyCrate = 1 << 8,
            Mining = 1 << 9,
            OneOfMany = 1 << 10,
            PlumeOnDeath = 1 << 11,
            Invulnerable = 1 << 12,
            InheritDamagePercentage = 1 << 13,
            RegrowOnStream = 1 << 14,
            CastsDropShadow = 1 << 15,
            DisableCollapseEffect = 1 << 16,
            ForceDynamic = 1 << 17,
            ShowOnMap = 1 << 18,
            Regenerate = 1 << 19,
            CastsShadow = 1 << 20
        }
    }

    public class GeneralMover : ObjectMover
    {
        [EditorProperty]
        public u32 GmFlags;
        [EditorProperty]
        public u32 OriginalObject;
        [EditorProperty]
        public u32 CollisionType; //TODO: Determine if this is really a bitflag defining collision layers. Some example collision types in the RfgTools zonex doc for general_mover
        [EditorProperty]
        public u32 Idx;
        [EditorProperty]
        public u32 MoveType; //Todo: See if this is the same as RfgMover.MoveTypeEnum
        [EditorProperty]
        public u32 DestructionUID;
        [EditorProperty]
        public i32 Hitpoints;
        [EditorProperty]
        public i32 DislodgeHitpoints;
    }

    public class RfgMover : ObjectMover
    {
        [EditorProperty]
        public MoveTypeEnum MoveType;
        //[EditorProperty]
        //public RfgMoverFlags Flags; //TODO: Implement. Currently disabled since it appears to be intended for runtime use only
        [EditorProperty]
        public f32 DamagePercent;

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

    public class ShapeCutter : ZoneObject
    {
        [EditorProperty]
        public i16 ShapeCutterFlags;
        [EditorProperty]
        public f32 OuterRadius;
        [EditorProperty]
        public u32 Source;
        [EditorProperty]
        public u32 Owner;
        [EditorProperty]
        public u8 ExplosionId;
    }

    public class ObjectEffect : ZoneObject
    {
        [EditorProperty]
        public append String EffectType; //Name of an entry in effects.xtbl
    }

    public class Item : ZoneObject
    {
        [EditorProperty]
        public append String ItemType; //Name of an entry in items_3d.xtbl for plain items
    }

    public class Weapon : Item
    {
        [EditorProperty]
        public append String WeaponType; //Name of an entry in weapons.xtbl
    }

    public class Ladder : ZoneObject
    {
        [EditorProperty]
        public i32 LadderRungs;
        [EditorProperty]
        public bool Enabled;
    }

    public class ObjLight : ZoneObject
    {
        [EditorProperty]
        public f32 AttenuationStart;
        [EditorProperty]
        public f32 AttenuationEnd;
        [EditorProperty]
        public f32 AttenuationRange;
        [EditorProperty]
        public ObjLightFlags LightFlags;
        [EditorProperty]
        public LightTypeEnum LightType;
        [EditorProperty]
        public Vec3 Color;
        [EditorProperty]
        public f32 HotspotSize;
        [EditorProperty]
        public f32 HotspotFalloffSize;
        [EditorProperty]
        public Vec3 MinClip;
        [EditorProperty]
        public Vec3 MaxClip;
        [EditorProperty]
        public i32 ClipMesh;

        public enum ObjLightFlags
        {
            None = 0,
            UseClipping = 1 << 0,
            DayTime = 1 << 1,
            NightTime = 1 << 2
        }
        public enum LightTypeEnum
        {
            Spot,
            Omni
        }
    }

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
        [EditorProperty]
        public MultiBackpackType BackpackType;
        [EditorProperty]
        public i32 NumBackpacks;
        [EditorProperty]
        public bool RandomBackpacks;
        [EditorProperty]
        public i32 Group;

        public enum MultiMarkerType
        {
            None,
            SiegeTarget,
            BackpackRack,
            SpawnNode,
            FlagCaptureZone,
            KingOfTheHillTarget,
            SpectatorCamera
        }
    }

    public class MultiFlag : Item
    {
        public Team MpTeam;
    }

    public class NavPoint : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
    }

    public class CoverNode : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
    }

    public enum Team
    {
        None,
        Neutral,
        Guerilla,
        EDF,
        Civilian,
        Marauder
    }

    public enum MultiBackpackType
    {
        Commando,
        Fleetfoot,
        Heal,
        Jetpack,
        Kaboom,
        Rhino,
        Shockwave,
        Stealth,
        Thrust,
        Tremor,
        Vision
    }
}