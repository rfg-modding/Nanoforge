using System;
using Nanoforge.Misc;
namespace Nanoforge.Misc;

//Used to specify the string used for properties or enums when exporting/importing rfgzone_pc files. E.g. For the BoundingBoxType enum GpsTarget in code, but is "GPS Target" in rfgzone_pc files
//This is only really needed for enums/bitflags that get stored in the zone files.
[Reflect(.All), AttributeUsage(.All, .ReflectAttribute)]
public struct RfgNameAttribute : Attribute
{
    public readonly String Name;

    public this(String name)
    {
        Name = name;
    }
}

namespace System
{
	extension Enum
	{
        public static Result<void> ToRfgName<T>(T value, String str) where T : Enum
        {
            Type enumType = typeof(T);
            for (var field in enumType.GetFields())
            {
                if (field.GetCustomAttribute<Nanoforge.Misc.RfgNameAttribute>() case .Ok(let nameAttribute))
                {
                    T fieldValue = (T)field.[Friend]mFieldData.mData;
                    if (value == fieldValue)
                    {
                        str.Set(nameAttribute.Name);
                        return .Ok;
                    }
                }
            }

            return .Err;
        }

        public static Result<T> FromRfgName<T>(StringView str) where T : Enum
        {
            Type enumType = typeof(T);
            for (var field in enumType.GetFields())
            {
                if (field.GetCustomAttribute<Nanoforge.Misc.RfgNameAttribute>() case .Ok(let nameAttribute))
                {
                    if (str == nameAttribute.Name)
                    {
                        return (T)field.[Friend]mFieldData.mData;
                    }
                }
            }

            return .Err;
        }

        //Converts bitflag enums to space separated list of strings. Used in rfgzone_pc files.
        //For example. An ObjectMover with ChunkFlags = (.Building | .WorldAnchor) would be converted to "building world_anchor"
        public static Result<void> ToRfgFlagsString<T>(T value, String str) where T : Enum
        {
            str.Set("");
            Type enumType = typeof(T);
            int valueAsInt = (int)value;
            for (var field in enumType.GetFields()) //Loop through flags
            {
                int fieldValue = field.[Friend]mFieldData.mData;
                if (fieldValue == 0)
                    continue; //0 value isn't ever displayed in the string. Only used in code as a default. If not flags are set the result is an empty string.

                if (field.GetCustomAttribute<Nanoforge.Misc.RfgNameAttribute>() case .Ok(let nameAttribute))
                {
                    if ((valueAsInt & fieldValue) != 0) //The enum has this flag
                    {
                        if (str.Length > 0)
                        {
                            str.Append(" ");
                        }
                        str.Append(nameAttribute.Name);
                    }
                }
                else
                {
                    Logger.WriteLine("Failed to convert enum of type '{0}' to RFG bitflag string. Field '{1}' is missing [RfgName]", typeof(T).GetName(.. scope .()), field.Name);
                    return .Err;
                }
            }

            return .Ok;
        }

        //Does the opposite of ToRfgFlagsString. E.g. "building world_anchor" -> (.Building | .WorldAnchor)
        public static Result<T> FromRfgFlagsString<T>(StringView str) where T : Enum
        {
            int flags = 0;
            Type enumType = typeof(T);
            for (var field in enumType.GetFields()) //Loop through flags and see which rfg names are present in str
            {
                int fieldValue = field.[Friend]mFieldData.mData;
                if (fieldValue == 0)
                    continue; //0 value isn't ever displayed in the string. Only used in code as a default. If not flags are set the result is an empty string.

                if (field.GetCustomAttribute<Nanoforge.Misc.RfgNameAttribute>() case .Ok(let nameAttribute))
                {
                    if (str.Contains(nameAttribute.Name))
                    {
                        flags |= fieldValue;
                    }
                }
                else
                {
                    Logger.WriteLine("Failed to convert RFG flags string enum of type '{0}' to . Field '{1}' is missing [RfgName]", typeof(T).GetName(.. scope .()), field.Name);
                    return .Err;
                }
            }

            return .Ok((T)flags);
        }
    }
}