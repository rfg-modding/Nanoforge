using System;

namespace Nanoforge.Misc;

//Used to specify the string used for properties or enums when exporting/importing rfgzone_pc files. E.g. For the BoundingBoxType enum GpsTarget in code, but is "GPS Target" in rfgzone_pc files
//This is only really needed for enums/bitflags that get stored in the zone files.
public class RfgNameAttribute(string name) : Attribute
{
    public readonly string Name = name;
}