using Nanoforge.App.Project;
using System.Diagnostics;
using System.Collections;
using System.Threading;
using Nanoforge.Misc;
using Common.Math;
using System.Linq;
using Common;
using System.IO;
using System;

namespace Nanoforge.App
{
    ///Project database. Tracks editor objects and changes made to them through transactions.
    ///If you're comparing this to the C++ codebase this is really the Project + Registry classes combined. They only ever exist together so they were merged.
    ///This implementation the more generic data model used by the C++ codebase for ease of use and type checks.
	public static class ProjectDB
	{
        public static String ProjectFilePath { get; private set; } = new .() ~delete _; //Path of project file
        public static String ProjectFolderPath { get; private set; } = new .() ~delete _;
        public static bool Ready { get; private set; } = false;

        static List<EditorObject> _objects = new .() ~DeleteContainerAndItems!(_);
        static Dictionary<EditorObject, String> _objectNames = new .() ~DeleteDictionaryAndValues!(_); //Names owned by Project so we don't add unnecessary bloat to unnamed objects
        static append Monitor _objectNameDictionaryLock;
        static append Monitor _objectCreationLock;
        static append Monitor _commitLock;

        //Undo/redo stacks. Just using plain lists + Add() & PopBack() to handle pushing onto the stack and popping off it.
        //Eventually should replace with a Stack<T> class that inherits List<T> and hides non stack function. At the moment Beef doesn't have a standard stack class.
        public static List<Commit> UndoStack = new .() ~DeleteContainerAndItems!(_);
        public static List<Commit> RedoStack = new .() ~DeleteContainerAndItems!(_);
         
        public static T CreateObject<T>(StringView name = "") where T : EditorObject, new
        {
            ScopedLock!(_objectCreationLock);
            T obj = new T();
            _objects.Add(obj);
            if (name != "")
                SetObjectName(obj, name);
            return obj;
        }

        //Used to add objects created by DiffUtil when it commits changes. It creates its own objects so they can be destroyed on rollback and don't exist program wide until commit.
        public static void AddObject(EditorObject obj)
        {
            ScopedLock!(_objectCreationLock);
            _objects.Add(obj);
        }

        //Removes object from ProjectDB. Not recommended for direct use. Doesn't delete the object. That way transactions can still hold object info on the redo stack to restore it as required
        //Currently doesn't remove any references to this object that others might have. If used in the undo/redo stack that shouldn't frequently be a problem.
        public static void RemoveObject(EditorObject obj)
        {
            ScopedLock!(_objectCreationLock);
            _objects.Remove(obj);
            _objectNames.Remove(obj);
        }

        public static Result<String> GetObjectName(EditorObject object)
        {
            ScopedLock!(_objectNameDictionaryLock);
            if (_objectNames.ContainsKey(object))
            {
                return .Ok(_objectNames[object]);
            }
            else
            {
                return .Err;
            }
        }

        public static void SetObjectName(EditorObject object, StringView name)
        {
            ScopedLock!(_objectNameDictionaryLock);
            if (_objectNames.ContainsKey(object))
            {
                _objectNames[object].Set(name);
            }
            else
            {
                _objectNames[object] = new String()..Set(name);
            }
        }

        public static EditorObject Find(StringView name)
        {
            for (var kv in _objectNames)
                if (StringView.Equals(kv.value, name))
                    return kv.key;

            return null;
        }

        public static T Find<T>(StringView name) where T : EditorObject
        {
            Object obj = Find(name);
            return (obj != null && obj.GetType() == typeof(T)) ? (T)obj : null;
        }

        public static void Reset()
        {
            ScopedLock!(_objectCreationLock);
            ScopedLock!(_objectNameDictionaryLock);
            for (var kv in _objectNames)
	            delete kv.value;
            for (EditorObject obj in _objects)
                delete obj;

            _objects.Clear();
            _objectNames.Clear();
            Ready = false;
        }

        //Commit changes to undo stack. ProjectDB takes ownership of the transactions
        public static void Commit(Span<ITransaction> transactions, StringView commitName)
        {
            ScopedLock!(_commitLock);
            UndoStack.Add(new Commit(commitName, transactions));
        }

        //Undo single commit
        public static void Undo()
        {
            ScopedLock!(_commitLock);
            if (UndoStack.Count > 0)
            {
                Commit undo = UndoStack.PopBack();
                for (ITransaction transaction in undo.Transactions)
                {
                    transaction.Revert();
                }
                RedoStack.Add(undo);
            }
        }

        //Redo single commit
        public static void Redo()
        {
            ScopedLock!(_commitLock);
            if (RedoStack.Count > 0)
            {
                Commit redo = RedoStack.PopBack();
                for (ITransaction transaction in redo.Transactions)
                {
                    transaction.Apply();
                }
                UndoStack.Add(redo);
            }
        }
	}
}