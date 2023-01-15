using System.Collections;
using System.Reflection;
using Nanoforge;
using System;

namespace Nanoforge.App.Project
{
    static
    {
        public static DiffUtil<T> TrackChanges<T>(T obj, StringView commitName) where T : EditorObject
        {
            DiffUtil<T> self = new .(obj, commitName);
            self.CreateSnapshot(obj);
            return self;
        }
    }

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

    public class DiffUtil<T> : IDisposable
		                       where T : EditorObject 
    {
        public T Target;
        public readonly String CommitName = new .() ~delete _;

        public const EditorPropertyInfo[?] PropertyInfo = GetEditorProperties();

        public this(T target, StringView commitName)
        {
            Target = target;
            CommitName.Set(commitName);
        }

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

        [OnCompile(.TypeInit), Comptime]
        public static void GenerateCode()
        {
            //Generate fields for holding snapshot of the object state
            Type selfType = typeof(Self);
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                StringView fieldName = field.Name;
                var fieldTypeName = field.FieldType.GetFullName(.. scope .());
                Compiler.EmitTypeBody(selfType, scope $"private {fieldTypeName} {fieldName};\n");
            }

            //Generate CreateSnapshot()
            Compiler.EmitTypeBody(selfType, scope $"\npublic void CreateSnapshot({typeof(T).GetFullName(.. scope .())} obj)");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            int offset = 0;
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                StringView fieldName = field.Name;
                Compiler.EmitTypeBody(selfType, scope $"    this.{fieldName} = obj.[Friend]{fieldName};\n");
                offset += field.FieldType.InstanceSize;
            }
            Compiler.EmitTypeBody(selfType, "}");

            //TODO: Generate Commit()
        }

        public void Commit()
        {
            //TODO: Iterate through properties and compare snapshot value with current value
            //TODO: Generate transactions for different properties
        }

        public void Dispose()
        {
            Commit();
            delete this;
        }
    }
}