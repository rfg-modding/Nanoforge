using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;

namespace Nanoforge.Rfg
{
    //Root structure of any RFG map. Each territory is split into one or more zones which contain the map objects.
	public class Territory : EditorObject
	{
        public append String PackfileName;

        [EditorProperty]
        public append List<Zone> Zones = .();

        public Result<void, StringView> Load()
        {
            return .Ok;
        }
	}

    //A cubic section of a map. Contains map objects
    public class Zone : EditorObject
    {
        [EditorProperty]
        public append String Name;
        [EditorProperty]
        public append String District;
        [EditorProperty]
        public u32 DistrictFlags = 0;
        [EditorProperty]
        public bool ActivityLayer = false;
        [EditorProperty]
        public bool MissionLayer = false;
        [EditorProperty]
        public append List<ZoneObject> Objects = .();
    }

    //Most objects seen in game are zone objects. With the exceptions of terrain, rocks, and grass.
    public class ZoneObject : EditorObject
    {
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

        [EditorProperty]
        public append String Classname;
        [EditorProperty]
        public u32 Handle;
        [EditorProperty]
        public u32 Num;
        [EditorProperty]
        public Flags Flags;
        [EditorProperty]
        public Vec3<f32> Bmin;
        [EditorProperty]
        public Vec3<f32> Bmax;
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
        public Vec3<f32> Position = .Zero;
        [EditorProperty]
        public Mat3 Orient = .Identity;
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

    public class NavPoint : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
    }

    public class CoverNode : ZoneObject
    {
        //TODO: Add properties. Only added an empty class so the instances in the Nordic Special map can be loaded. Full support for this class will be added along with SP map editing.
    }
}