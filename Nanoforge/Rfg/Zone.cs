using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using RFGM.Formats.Zones;

namespace Nanoforge.Rfg;

//A cubic section of a map. Contains map objects
public class Zone : EditorObject
{
    public string District = string.Empty;
    public DistrictFlags DistrictFlags = 0;
    public uint DistrictHash = 0;
    public bool ActivityLayer = false;
    public bool MissionLayer = false;
    public List<ZoneObject> Objects = new();
    public ZoneTerrain? Terrain;
}