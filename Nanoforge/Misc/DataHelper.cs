using System;
using System.Collections.Generic;
using System.Reflection;
using Serilog;

namespace Nanoforge.Misc;

public static class DataHelper
{
    public static Toggleable<byte[]?> DeepCopyToggleableBytes(Toggleable<byte[]?> input)
    {
        if (input.Value == null)
        {
            return new Toggleable<byte[]?>(null);            
        }
        
        byte[] bytesCopy = new byte[input.Value.Length];
        input.Value.CopyTo(bytesCopy, 0);
        return new Toggleable<byte[]?>(bytesCopy);
    }
    
    public static bool ToRfgName<T>(T value, out string rfgName) where T : Enum
    {
        Type enumType = typeof(T);
        foreach (FieldInfo field in enumType.GetFields(BindingFlags.Static | BindingFlags.Public))
        {
            if (field.GetCustomAttribute<RfgNameAttribute>() is { } attribute)
            {
                //object fieldValue = field.GetValue(value);
                var fieldValue = (T?)field.GetValue(null);
                if (fieldValue != null && EqualityComparer<T>.Default.Equals(fieldValue, value))
                {
                    rfgName = attribute.Name;
                    return true;
                }
            }
        }

        rfgName = string.Empty;
        return false;
    }

    public static bool FromRfgName<T>(string rfgName, out T value) where T : Enum
    {
        Type enumType = typeof(T);
        foreach (FieldInfo field in enumType.GetFields(BindingFlags.Static | BindingFlags.Public))
        {
            if (field.GetCustomAttribute<RfgNameAttribute>() is { } attribute)
            {
                if (rfgName == attribute.Name && field.GetValue(null) is T enumValue)
                {
                    value = enumValue;
                    return true;
                }
            }
        }

        value = default!;
        return false;
    }
    
    //Converts bitflag enums to space separated list of strings based on the [RfgName] for each flag. Used in rfgzone_pc files.
    //For example. An ObjectMover with ChunkFlags = (.Building | .WorldAnchor) would be converted to "building world_anchor"
    public static bool ToRfgFlagsString<T>(T value, out string flagsString) where T : Enum
    {
        flagsString = "";
        Type enumType = typeof(T);
        int valueAsInt = (int)Convert.ChangeType(value, typeof(int));
        foreach (FieldInfo field in enumType.GetFields(BindingFlags.Static | BindingFlags.Public)) //Loop through flags
        {
            T? flagEnum = (T?)field.GetValue(null);
            if (flagEnum == null)
                continue;
            
            int fieldValue = (int)Convert.ChangeType(flagEnum, typeof(int));
            if (fieldValue == 0) 
                continue;  //0 value isn't ever displayed in the string. Only used in code as a default. If not flags are set the result is an empty string.

            if (field.GetCustomAttribute<RfgNameAttribute>() is { } attribute)
            {
                if ((valueAsInt & fieldValue) != 0) //Flag is set
                {
                    if (flagsString.Length > 0)
                    {
                        flagsString += " ";
                    }
                    flagsString += attribute.Name;
                }
            }
            else
            {
                Log.Error("Failed to convert enum of type '{0}' to RFG bitflag string. Field '{1}' is missing [RfgName]", typeof(T).FullName, field.Name);
            }
        }
        
        return true;
    }

    public static unsafe bool FromRfgFlagsString<T>(string flagsString, out T value) where T : unmanaged, Enum
    {
        int flags = 0;
        Type enumType = typeof(T);
        foreach (FieldInfo field in enumType.GetFields(BindingFlags.Static | BindingFlags.Public)) //Loop through flags and see which rfg names are present in str
        {
            T? flagEnum = (T?)field.GetValue(null);
            if (flagEnum == null)
                continue;
        
            int fieldValue = (int)Convert.ChangeType(flagEnum, typeof(int));
            if (fieldValue == 0) 
                continue;  //0 value isn't ever displayed in the string. Only used in code as a default. If not flags are set the result is an empty string.
            if (field.GetCustomAttribute<RfgNameAttribute>() is { } attribute)
            {
                if (flagsString.Contains(attribute.Name))
                {
                    flags |= fieldValue;
                }
            }
        }
        value = *(T*)&flags;
        return true;
    }
}