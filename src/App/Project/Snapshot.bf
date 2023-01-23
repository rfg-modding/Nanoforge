using System.Collections;
using Nanoforge.Math;
using Nanoforge;
using System;

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
        public static void GenerateCode()
        {
            GenerateTypeFields();
            GenerateCreate();
            GenerateCommit();
            GenerateApply();
        }

        //Generate fields for holding snapshot of the state for an object with the type of T
        [Comptime]
        public static void GenerateTypeFields()
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
        public static void GenerateCreate()
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

        //Generate ISnapshot.Commit()
        [Comptime]
        public static void GenerateCommit()
        {
            Type selfType = typeof(Self);
            var genericTypeName = typeof(T).GetFullName(.. scope .());

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
                    case typeof(i8), typeof(i16), typeof(i32), typeof(i64), typeof(u8), typeof(u16), typeof(u32), typeof(u64), typeof(bool), typeof(f32), typeof(f64),
						 typeof(Vec2<f32>), typeof(Vec3<f32>), typeof(Vec4<f32>), typeof(Mat2), typeof(Mat3), typeof(Mat4), typeof(Rect), typeof(StringView), typeof(char8*):
                        Compiler.EmitTypeBody(selfType, scope $"    if (this.{fieldName} != S_Target.{fieldName})\n");
                        Compiler.EmitTypeBody(selfType, "    {\n");
                        Compiler.EmitTypeBody(selfType, scope $"        transactions.Add(new SetPropertyTransaction<{genericTypeName}, {fieldTypeName}, \"{fieldName}\">(S_Target, this.{fieldName}, S_Target.{fieldName}));\n");
                        Compiler.EmitTypeBody(selfType, "    }\n");
                    default:
                        Runtime.FatalError(scope $"Unsupported type '{fieldTypeName}' used in editor property {genericTypeName}.{fieldName}. Please implement in Snapshot<T>.GenerateCommit()");
                }
            }
            Compiler.EmitTypeBody(selfType, "}\n");
        }

        //Generate ISnapshot.Apply()
        [Comptime]
        public static void GenerateApply()
        {
            Type selfType = typeof(Self);
            var genericTypeName = typeof(T).GetFullName(.. scope .());
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Apply(EditorObject target)");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            Compiler.EmitTypeBody(selfType, scope $"    {genericTypeName} targetTyped = ({genericTypeName})target;\n");
            for (var field in typeof(T).GetFields())
            {
                //Only track fields with [EditorProperty] attribute
                var result = field.GetCustomAttribute<EditorPropertyAttribute>();
                if (result == .Err)
                    continue;

                StringView fieldName = field.Name;
                Compiler.EmitTypeBody(selfType, scope $"    targetTyped.[Friend]{fieldName} = this.{fieldName};\n");
            }
            Compiler.EmitTypeBody(selfType, "}");
        }
    }
}