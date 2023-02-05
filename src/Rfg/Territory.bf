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
}