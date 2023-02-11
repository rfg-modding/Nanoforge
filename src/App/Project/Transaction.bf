using System.Collections;
using Common.Math;
using Common;
using System;

namespace Nanoforge.App.Project
{
    //A named group of transactions. These are what's listed in the history UI and the level that undo/redo operates on
    public class Commit
    {
        public readonly String Name = new .() ~delete _;
        public List<ITransaction> Transactions = new .() ~DeleteContainerAndItems!(_);

        public this(StringView name, Span<ITransaction> transactions)
        {
            Name.Set(name);
            Transactions.Set(transactions);
        }
    }

    public interface ITransaction
    {
        public void Apply() {}
        public void Revert() {}
    }

    public class CreateObjectTransaction : ITransaction
    {
        private EditorObject _target;
        private String _name = new .() ~delete _;

        public this(EditorObject target, StringView name = "")
        {
            _target = target;
            _name.Set(name);
        }

        public virtual void Apply()
        {
            ProjectDB.AddObject(_target);
            ProjectDB.SetObjectName(_target, _name);
        }

        public virtual void Revert()
        {
            ProjectDB.RemoveObject(_target);
        }
    }

    public class SetPropertyTransaction<T, U, V> : ITransaction
                                              where T : EditorObject
                                              where V : const String
    {
        T _target;
        U _initialValue;
        U _finalValue;

        public this(T target, U initialValue, U finalValue)
        {
            _target = target;
            _initialValue = initialValue;
            _finalValue = finalValue;
        }

        [OnCompile(.TypeInit), Comptime]
        public static void EmitCode()
        {
            if (V != null)
            {
				EmitApply();
                EmitRevert();
            }
        }

        [Comptime]
        public static void EmitApply()
        {
            Type selfType = typeof(Self);
            Type fieldType = typeof(U);
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Apply()");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            switch (fieldType)
            {
                case typeof(i8), typeof(i16), typeof(i32), typeof(i64), typeof(int), typeof(u8), typeof(u16), typeof(u32), typeof(u64), typeof(bool), typeof(f32), typeof(f64),
                     typeof(Vec2<f32>), typeof(Vec3<f32>), typeof(Vec4<f32>), typeof(Mat2), typeof(Mat3), typeof(Mat4), typeof(Rect), typeof(BoundingBox):
                    Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _finalValue;\n");
                case typeof(String):
                    Compiler.EmitTypeBody(selfType, scope $"    _target.{V}.Set(_finalValue);\n");
                default:
                    if (fieldType.HasInterface(typeof(IList)))
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    _target.{V}.Set(_finalValue);\n");
                    }
                    else if (fieldType.BaseType == typeof(Enum) || fieldType.BaseType == typeof(EditorObject))
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _finalValue;\n");
                    }
                    else
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    //Failed to emit code for {typeof(U).GetFullName(.. scope .())} {V}\n");
                        Compiler.EmitTypeBody(selfType, "}\n");
                        Runtime.FatalError(scope $"Unsupported type '{typeof(U).GetFullName(.. scope .())}' used in editor property {typeof(T).GetFullName(.. scope .())}.{V}. Please implement in Snapshot<T>.GenerateCommit()");
                    }
            }
            Compiler.EmitTypeBody(selfType, "}\n");
        }

        [Comptime]
        public static void EmitRevert()
        {
            Type selfType = typeof(Self);
            Type fieldType = typeof(U);
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Revert()");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            switch (typeof(U))
            {
                case typeof(i8), typeof(i16), typeof(i32), typeof(i64), typeof(int), typeof(u8), typeof(u16), typeof(u32), typeof(u64), typeof(bool), typeof(f32), typeof(f64),
                     typeof(Vec2<f32>), typeof(Vec3<f32>), typeof(Vec4<f32>), typeof(Mat2), typeof(Mat3), typeof(Mat4), typeof(Rect), typeof(BoundingBox):
                    Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _initialValue;\n");
                case typeof(String):
                    Compiler.EmitTypeBody(selfType, scope $"    _target.{V}.Set(_initialValue);\n");
                default:
                    if (fieldType.HasInterface(typeof(IList)))
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    _target.{V}.Set(_initialValue);\n");
                    }
                    else if (fieldType.BaseType == typeof(Enum) || fieldType.BaseType == typeof(EditorObject))
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _initialValue;\n");
                    }
                    else
                    {
                        Compiler.EmitTypeBody(selfType, scope $"    //Failed to emit code for {typeof(U).GetFullName(.. scope .())} {V}\n");
                        Compiler.EmitTypeBody(selfType, "}\n");
                        Runtime.FatalError(scope $"Unsupported type '{typeof(U).GetFullName(.. scope .())}' used in editor property {typeof(T).GetFullName(.. scope .())}.{V}. Please implement in Snapshot<T>.GenerateRevert()");
                    }
            }
            Compiler.EmitTypeBody(selfType, "}\n");
        }
    }
}