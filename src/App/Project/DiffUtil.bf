using System.Collections;
using System.Reflection;
using Common;
using System;

namespace Nanoforge.App.Project
{
    static
    {
        public static mixin BeginCommit(StringView commitName)
        {
            DiffUtil diffUtil = scope:: .(commitName);
            diffUtil
        }

        //Override for when you're only tracking changes on one object
        public static mixin BeginCommit<T>(StringView commitName, T obj) where T : EditorObject
        {
            DiffUtil diffUtil = scope:: .(commitName);
            diffUtil.Track<T>(obj);
            diffUtil
        }
    }

    //Tracks changes made to editor objects, generates a set of transactions representing those changes, and commits them to the project DB.
    //Transactions can be rolled back prior to committing them. Useful if an error occurs.
    //DiffUtil is NOT thread safe. You should only use an instance with one thread at a time.
    public class DiffUtil : IDisposable
    {
        private Dictionary<EditorObject, ISnapshot> _snapshots = new .() ~DeleteDictionaryAndValues!(_);
        private List<EditorObject> _newObjects = new .() ~delete _;
        private Dictionary<EditorObject, String> _newObjectNames = new .() ~DeleteDictionaryAndValues!(_);
        public readonly String CommitName = new .() ~delete _;
        public bool Cancelled { get; private set; } = false;

        public this(StringView commitName)
        {
            CommitName.Set(commitName);
        }

        public void Track<T>(T target) where T : EditorObject
        {
            _snapshots[target] = new Snapshot<T>(target);
        }

        public void Commit()
        {
            //Generate property change transactions
            List<ITransaction> transactions = scope .();
            for (var kv in _snapshots)
            {
                EditorObject target = kv.key;
                var snapshot = kv.value;

                //For new objects generate a creation transaction + add the object to the DB
                if (_newObjects.Contains(target))
                {
					transactions.Add(new CreateObjectTransaction(target, _newObjectNames.GetValueOrDefault(target)));
                    ProjectDB.AddObject(target);
                    if (_newObjectNames.ContainsKey(target))
                        ProjectDB.SetObjectName(target, _newObjectNames[target]);
                }

                //Generate transactions describing property changes
                snapshot.GenerateDiffTransactions(transactions);
            }

            ProjectDB.Commit(transactions, CommitName);
        }

        public void Rollback()
        {
            Cancelled = true;

            //Delete new objects + their names
            for (EditorObject obj in _newObjects)
                delete obj;

            DeleteDictionaryAndValues!(_newObjectNames);

            //Apply snapshots to pre-existing objects to undo changes
            for (var kv in _snapshots)
            {
                EditorObject obj = kv.key;
                ISnapshot snapshot = kv.value;
                snapshot.Apply(obj);
            }
        }

        public void Cancel()
        {
            Cancelled = true;
        }

        public void Dispose()
        {
            if (Cancelled)
            {
                Rollback();
            }
            else
            {
                Commit();
            }
        }

        public T CreateObject<T>(StringView name = "") where T : EditorObject
        {
            T obj = new T();
            _newObjects.Add(obj);
            if (name != "")
                _newObjectNames[obj] = new .()..Set(name);

            Track<T>(obj);
            return obj;
        }
    }
}