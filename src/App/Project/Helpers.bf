using System.Collections;
using Common;
using System;

namespace Nanoforge.App.Project
{
    public struct EditorPropertyInfo
    {
        public StringView Name;
        public int Size;
        public int Offset;
        public i32 FieldIndex;
        public Type PropertyType;

        public this(StringView name, int size, int offset, i32 fieldIndex, Type propertyType)
        {
            Name = name;
            Size = size;
            Offset = offset;
            FieldIndex = fieldIndex;
            PropertyType = propertyType;
        }
    }

    public struct TypeUtil<T> where T : EditorObject
    {
        public const EditorPropertyInfo[?] PropertyInfo = GetEditorProperties();

        //Grab a list of tracked properties and their metadata during comptime for quick operations at runtime
        [Comptime(ConstEval=true)]
        private static Span<EditorPropertyInfo> GetEditorProperties()
        {
            int offset = 0;
            List<EditorPropertyInfo> properties = scope .();
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                properties.Add(.(field.Name, field.FieldType.InstanceSize, offset, field.FieldIdx, field.FieldType));
                offset += field.FieldType.InstanceSize;
            }
            return properties;
        }
    }
}