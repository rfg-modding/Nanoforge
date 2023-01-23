using System.Collections;
using Nanoforge;
using System;

namespace Nanoforge.App.Project
{
    //A named group of transactions. These are what's listed in the history UI and the level that undo/redo operates on
    public class Commit
    {
        public readonly String Name = new .();
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
        private String _name = new .();

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
        public static void GenerateCode()
        {
            if (V != null)
            {
				GenerateApply();
                GenerateRevert();
            }
        }

        [Comptime]
        public static void GenerateApply()
        {
            Type selfType = typeof(Self);
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Apply()");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _finalValue;\n");
            Compiler.EmitTypeBody(selfType, "}\n");
        }

        [Comptime]
        public static void GenerateRevert()
        {
            Type selfType = typeof(Self);
            Compiler.EmitTypeBody(selfType, scope $"\npublic virtual void Revert()");
            Compiler.EmitTypeBody(selfType, "\n{\n");
            Compiler.EmitTypeBody(selfType, scope $"    _target.{V} = _initialValue;\n");
            Compiler.EmitTypeBody(selfType, "}\n");
        }
    }
}