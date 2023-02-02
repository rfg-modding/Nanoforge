using Nanoforge.App;
using Common;
using System;

namespace System
{
	extension Enum
	{
		//The number of values the enum has
		[Comptime]
		public static u32 Count<T>() where T : enum
		{
			return (u32)typeof(T).FieldCount;
		}

        public static String ValueToString(Type type, String str, int value)
        {
            Runtime.Assert(type.IsEnum);
            for (var enumField in type.GetFields())
            {
                if (enumField.[Friend]mFieldData.mData == value)
                    return str..Set(enumField.[Friend]mFieldData.mName);
            }
            return str;
        }

        public static String ValueToString<T>(String str, T value) where T : operator explicit int, enum
        {
            int val = (int)value;
            Type type = typeof(T);
            Runtime.Assert(type.IsEnum);
            for (var enumField in type.GetFields())
            {
                if (enumField.[Friend]mFieldData.mData == *(int*)(void*)&val)
                    return str..Set(enumField.[Friend]mFieldData.mName);
            }
            return str;
        }
	}
}