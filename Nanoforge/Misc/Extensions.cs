using System;
using System.Reflection;

namespace Nanoforge.Misc;

public static class TypeExtensions
{
    public static bool HasBaseType<T>(this Type type)
    {
        Type? curType = type;
        while (curType != null)
        {
            if (curType == typeof(T))
            {
                return true;
            }
            
            curType = curType.BaseType;
        }

        return false;
    }
}
