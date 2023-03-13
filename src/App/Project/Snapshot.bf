using System.Collections;
using Common.Math;
using Common;
using System;
using Nanoforge.Rfg;

namespace Nanoforge.App.Project
{
    public interface ISnapshot
    {
        public void CreateSnapshot() {} //Create a snapshot of the current state of the target object
        public void GenerateDiffTransactions(List<ITransaction> transactions) //Generate transactions by comparing the initial snapshot state to the current object state
		{
			Runtime.FatalError("The default implementation of ISnapshot.GenerateDiffTransactions() should not be used. It only exists so the compiler doesn't think the impl is missing with Snapshot<T> since it generates them at comptime.");
		}
        public void Apply(EditorObject target) {} //Apply snapshot to object. Reverts any changes made to the object after the snapshot.
    }

    //Takes a snapshot of an editor objects current state and generates a list of transactions describing the changes made to the object after the snapshot.
    public class Snapshot<T> : ISnapshot
    	                       where T : EditorObject
    {
        //Different naming style than the rest of this codebase to make it less likely to conflict with fields on T (since we add the fields to Snapshot<T> at comptime)
        public T S_Target;

        public this(T target)
        {
            S_Target = target;
            Create();
        }

        [OnCompile(.TypeInit), Comptime]
        public static void EmitCode()
        {
            EmitTypeFields();
            EmitCreate();
            EmitGenerateDiffTransactions();
            EmitApply();
        }

        //Generate fields for holding snapshot of the state for an object with the type of T
        [Comptime]
        public static void EmitTypeFields()
        {
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
        }

        //Generate ISnapshot.Create()
        [Comptime]
        public static void EmitCreate()
        {
            Type selfType = typeof(Self);
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Create()");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                StringView fieldName = field.Name;
                Compiler.EmitTypeBody(selfType, scope $"    this.{fieldName} = S_Target.[Friend]{fieldName};\n");
            }
            Compiler.EmitTypeBody(selfType, "}\n");
        }

        //Generate ISnapshot.GenerateDiffTransactions()
        [Comptime]
        public static void EmitGenerateDiffTransactions()
        {
            Type selfType = typeof(Self);
            StringView genericTypeName = typeof(T).GetFullName(.. scope .());

            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void GenerateDiffTransactions(List<ITransaction> transactions)");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                StringView fieldName = field.Name;
                Type fieldType = field.FieldType;
                StringView fieldTypeName = fieldType.GetFullName(.. scope .());
                switch (fieldType)
                {
                    case typeof(i8), typeof(i16), typeof(i32), typeof(i64), typeof(int), typeof(u8), typeof(u16), typeof(u32), typeof(u64), typeof(bool), typeof(f32), typeof(f64),
						 typeof(Vec2), typeof(Vec3), typeof(Vec4), typeof(Mat3), typeof(Mat4), typeof(Rect), typeof(BoundingBox), typeof(String):
                        Compiler.EmitTypeBody(selfType, scope $"    if (this.{fieldName} != S_Target.{fieldName})\n");
                        Compiler.EmitTypeBody(selfType, "    {\n");
                        Compiler.EmitTypeBody(selfType, scope $"        transactions.Add(new SetPropertyTransaction<{genericTypeName}, {fieldTypeName}, \"{fieldName}\">(S_Target, this.{fieldName}, S_Target.{fieldName}));\n");
                        Compiler.EmitTypeBody(selfType, "    }\n");
                    default:
                        if (fieldType.HasInterface(typeof(IList)))
                        {
                            Compiler.EmitTypeBody(selfType, scope $"    if (this.{fieldName} != S_Target.{fieldName})\n");
                            Compiler.EmitTypeBody(selfType, "    {\n");
                            Compiler.EmitTypeBody(selfType, scope $"        {genericTypeName} target = S_Target;\n");
                            Compiler.EmitTypeBody(selfType, scope $"        {fieldTypeName} initialValue = this.{fieldName}.ShallowClone(.. new {fieldTypeName}());\n");
                            Compiler.EmitTypeBody(selfType, scope $"        {fieldTypeName} finalValue = S_Target.{fieldName}.ShallowClone(.. new {fieldTypeName}());\n");
                            Compiler.EmitTypeBody(selfType, scope $"        var transaction = new SetPropertyTransaction<{genericTypeName}, {fieldTypeName}, \"{fieldName}\">(target, initialValue, finalValue);\n");
                            Compiler.EmitTypeBody(selfType, scope $"        transactions.Add(transaction);\n");
                            Compiler.EmitTypeBody(selfType, "    }\n");
                        }
                        else if (fieldType.BaseType == typeof(Enum) || fieldType.BaseType == typeof(EditorObject))
                        {
                            Compiler.EmitTypeBody(selfType, scope $"    if (this.{fieldName} != S_Target.{fieldName})\n");
                            Compiler.EmitTypeBody(selfType, "    {\n");
                            Compiler.EmitTypeBody(selfType, scope $"        transactions.Add(new SetPropertyTransaction<{genericTypeName}, {fieldTypeName}, \"{fieldName}\">(S_Target, this.{fieldName}, S_Target.{fieldName}));\n");
                            Compiler.EmitTypeBody(selfType, "    }\n");
                        }
                        else
                        {
                            Compiler.EmitTypeBody(selfType, scope $"    //Failed to emit ISnapshot.GenerateDiffTransactions() for {field.FieldType.GetFullName(.. scope .())} {fieldName}\n");
                            StringView baseTypeName = fieldType.BaseType != null ? fieldType.BaseType.GetFullName(.. scope .()) : "";
                            Compiler.EmitTypeBody(selfType, "}\n");
                            Runtime.FatalError(scope $"Unsupported type '{fieldTypeName}' used in editor property {genericTypeName}.{fieldName}. BaseTypeName = {baseTypeName}. Please implement in Snapshot<T>.GenerateDiffTransactions()");
                        }
                }
            }
            Compiler.EmitTypeBody(selfType, "}\n");
        }

        //Generate ISnapshot.Apply()
        [Comptime]
        public static void EmitApply()
        {
            Type selfType = typeof(Self);
            var genericTypeName = typeof(T).GetFullName(.. scope .());

            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Apply(EditorObject target)");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            if (typeof(T).FieldCount > 0)
            {
                Compiler.EmitTypeBody(selfType, scope $"    {genericTypeName} targetTyped = ({genericTypeName})target;\n");
                for (var field in typeof(T).GetFields())
                {
                    //Only track fields with [EditorProperty] attribute
                    var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                    if (result == .Err)
                        continue;

                    Type fieldType = field.FieldType;
                    StringView fieldName = field.Name;
                    StringView fieldTypeName = fieldType.GetFullName(.. scope .());
                    switch (field.FieldType)
                    {
                        case typeof(i8), typeof(i16), typeof(i32), typeof(i64), typeof(int), typeof(u8), typeof(u16), typeof(u32), typeof(u64), typeof(bool), typeof(f32), typeof(f64),
                             typeof(Vec2), typeof(Vec3), typeof(Vec4), typeof(Mat3), typeof(Mat4), typeof(Rect), typeof(BoundingBox):
                            Compiler.EmitTypeBody(selfType, scope $"    targetTyped.[Friend]{fieldName} = this.{fieldName};\n");
                        case typeof(String):
                            Compiler.EmitTypeBody(selfType, scope $"    targetTyped.[Friend]{fieldName}.Set(this.{fieldName});\n");
                        default:
                            if (fieldType.HasInterface(typeof(IList)))
                            {
                                Compiler.EmitTypeBody(selfType, scope $"    targetTyped.[Friend]{fieldName}.Set(this.{fieldName});\n");
                            }
                            else if (fieldType.BaseType == typeof(Enum) || fieldType.BaseType == typeof(EditorObject))
                            {
                                Compiler.EmitTypeBody(selfType, scope $"    targetTyped.[Friend]{fieldName} = this.{fieldName};\n");
                            }
                            else
                            {
                                Compiler.EmitTypeBody(selfType, scope $"    //Failed to emit code for {field.FieldType.GetFullName(.. scope .())} {fieldName}\n");
                                StringView baseTypeName = fieldType.BaseType != null ? fieldType.BaseType.GetFullName(.. scope .()) : "";
                                Compiler.EmitTypeBody(selfType, "}\n");
                                Runtime.FatalError(scope $"Unsupported type '{fieldTypeName}' used in editor property {genericTypeName}.{fieldName}. BaseTypeName = {baseTypeName}. Please implement in Snapshot<T>.GenerateApply()");
                            }
                    }
                }
            }
            Compiler.EmitTypeBody(selfType, "}");
        }
    }
}