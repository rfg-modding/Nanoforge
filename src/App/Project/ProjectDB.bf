using System.Diagnostics;
using System.Collections;
using System.Threading;
using Nanoforge.Misc;
using Nanoforge.Math;
using System.Linq;
using Nanoforge;
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

        public static T CreateObject<T>() where T : EditorObject, new
        {
            ScopedLock!(_objectCreationLock);
            T obj = new T();
            _objects.Add(obj);
            return obj;
        }

        public static String GetObjectName(EditorObject object)
        {
            ScopedLock!(_objectNameDictionaryLock);
            if (_objectNames.ContainsKey(object))
            {
                return _objectNames[object];
            }
            else
            {
                return null;
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

        public static EditorObject GetObjectByName(StringView name)
        {
            for (var kv in _objectNames)
                if (StringView.Equals(kv.value, name))
                    return kv.key;

            return null;
        }

        public static T GetObjectByName<T>(StringView name) where T : EditorObject
        {
            Object obj = GetObjectByName(name);
            return obj.GetType() == typeof(T) ? (T)obj : null;
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
	}
}