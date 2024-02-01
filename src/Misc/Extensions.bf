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

    namespace IO
    {
        extension Directory
        {
            //Copy a directory and any subdirectories or files
            public static Result<void, Platform.BfpFileResult> CopyTree(StringView source, StringView destination)
            {
                mixin ValidatePath(StringView path)
                {
                    if (path.Length <= 2)
                    	return .Err(.InvalidParameter);
                    if ((path[0] != '/') && (path[0] != '\\'))
                    {
                    	if (path[1] == ':')
                    	{
                    		if (path.Length < 3)
                    			return .Err(.InvalidParameter);
                    	}
                    }
                }
                ValidatePath!(source);
                ValidatePath!(destination);

                if (!Directory.Exists(destination))
                {
                    Directory.CreateDirectory(destination);
                }

                //Copy subfolders
                for (var entry in Directory.EnumerateDirectories(source))
                {
                    String subdirPath = entry.GetFilePath(.. scope .());
                    String subdirDestPath = scope $"{destination}";
                    if (!destination.EndsWith('/') && !destination.EndsWith('\\'))
                    {
                        subdirDestPath += "\\";
                    }
                    subdirDestPath.Append(entry.GetFileName(.. scope .()));
                    subdirDestPath.Append("\\");

                    Try!(CopyTree(subdirPath, subdirDestPath));
                }

                //Copy subfiles
                for (var entry in Directory.EnumerateFiles(source))
                {
                    String subfilePath = entry.GetFilePath(.. scope .());
                    String subfileDestPath = scope $"{destination}";
                    if (!destination.EndsWith('/') && !destination.EndsWith('\\'))
                    {
                        subfileDestPath += "\\";
                    }
                    subfileDestPath.Append(entry.GetFileName(.. scope .()));

                    Try!(File.Copy(subfilePath, subfileDestPath));
                }

                return .Ok;
            }
        }
    }
}