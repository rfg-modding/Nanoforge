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
using System.Reflection;
using Bon;
using Bon.Integrated;

namespace Nanoforge.App
{
    ///Project database. Tracks editor objects and changes made to them through transactions.
    ///If you're comparing this to the C++ codebase this is really the Project + Registry classes combined. They only ever exist together so they were merged.
    ///This implementation was chosen over the more generic data model used by the C++ codebase for ease of use and comptime type checks.
	public static class ProjectDB
	{
        static append Dictionary<u64, EditorObject> _objects ~ClearDictionaryAndDeleteValues!(_);
        static append Monitor _objectCreationLock;
        static append Monitor _bufferCreationLock;
        static append Monitor _commitLock;

        static u64 _nextObjectUID = 0;
        static u64 _nextBufferUID = 0;
        static append Dictionary<u64, ProjectBuffer> _buffers ~ClearDictionaryAndDeleteValues!(_);

        //Used to patch fields referencing objects and buffers after they've all been loaded.
        static append List<(u64, ValueView)> _deserializationObjectReferences;
        static append List<(u64, ValueView)> _deserializationBufferReferences;

        //Undo/redo stacks. Just using plain lists + Add() & PopBack() to handle pushing onto the stack and popping off it.
        //Eventually should replace with a Stack<T> class that inherits List<T> and hides non stack function. At the moment Beef doesn't have a standard stack class.
        public static List<Commit> UndoStack = new .() ~DeleteContainerAndItems!(_);
        public static List<Commit> RedoStack = new .() ~DeleteContainerAndItems!(_);
        public static Project CurrentProject = new .() ~delete _;

        [BonTarget]
        public class Project
        {
            public String Name = new .()..Append("No project loaded") ~delete _;
            [BonIgnore]
            public String Directory = new .() ~delete _;
            [BonIgnore]
            public String FilePath = new .() ~delete _;
            public bool Loaded { get; private set; } = false;
        }

        public static this()
        {
            //Temporary hardcoded project folder while porting from C++. Will replace once I add project loading and saving
            CurrentProject.Name.Set("NFRewriteProjectTest"); //TODO: DE-HARDCODE
            CurrentProject.Directory.Set(@"D:\_NFRewriteProjectDBTest\");
            CurrentProject.FilePath.Set(scope $"{CurrentProject.Directory}NFRewriteTest.nanoproj");
        }

        public static void Init()
		{
            BonInit();
        }	

        public static T CreateObject<T>(StringView name = "") where T : EditorObject, new
        {
            ScopedLock!(_objectCreationLock);
            let uid = _nextObjectUID++; 
            T obj = new T();
            obj.[Friend]_uid = uid;
            _objects[uid] = (obj);
            obj.Name.Set(name);
            return obj;
        }

        public static Result<ProjectBuffer> CreateBuffer(Span<u8> bytes = .Empty, StringView name = "")
        {
            _bufferCreationLock.Enter();
            u64 uid = _nextBufferUID;
            ProjectBuffer buffer = new .(uid, bytes.Length, name);
            if (!bytes.IsEmpty)
            {
                if (buffer.Save(bytes) case .Err)
                {
                    Logger.Error("Error creating new project buffer '{}'. Failed to save.", name);
                    delete buffer;
                    return .Err;
                }
            }

            _buffers[uid] = buffer;
            _nextBufferUID++;
            _bufferCreationLock.Exit();
            return buffer;
        }

        //Used to add objects created by DiffUtil when it commits changes. It creates its own objects so they can be destroyed on rollback and don't exist program wide until commit.
        public static void AddObject(EditorObject obj)
        {
            ScopedLock!(_objectCreationLock);
            let uid = _nextObjectUID++;
            obj.[Friend]_uid = uid;
            _objects[uid] = obj;
        }

        //Removes object from ProjectDB. Not recommended for direct use. Doesn't delete the object. That way transactions can still hold object info on the redo stack to restore it as required
        //Currently doesn't remove any references to this object that others might have. If used in the undo/redo stack that shouldn't frequently be a problem.
        public static void RemoveObject(EditorObject obj)
        {
            ScopedLock!(_objectCreationLock);
            _objects.Remove(obj.UID);
        }

        public static EditorObject Find(StringView name)
        {
            for (var kv in _objects)
                if (StringView.Equals(kv.value.Name, name))
                    return kv.value;

            return null;
        }

        public static T Find<T>(StringView name) where T : EditorObject
        {
            Object obj = Find(name);
            return (obj != null && obj.GetType() == typeof(T)) ? (T)obj : null;
        }

        public static T FindOrCreate<T>(StringView name) where T : EditorObject
        {
            T find = Find<T>(name);
            if (find == null)
            {
                find = CreateObject<T>(name);
            }
            return find;
        }

        public static void Reset()
        {
            ScopedLock!(_objectCreationLock);
            ClearDictionaryAndDeleteValues!(_objects);
            ClearDictionaryAndDeleteValues!(_buffers);
            CurrentProject.[Friend]Loaded = false;
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

        //Save to text file
        [Trace(.All)]
        public static Result<void, StringView> Save()
        {
            Directory.CreateDirectory(CurrentProject.Directory);
            String objectsText = scope .()..Reserve(1000000);
            String buffersText = scope .()..Reserve(100000);
            String nanoprojText = scope .();

            BonInit();

            for (var kv in _objects)
            {
                Bon.Serialize(kv.value, objectsText);
            }

            for (var kv in _buffers)
            {
                ProjectBuffer buffer = kv.value;
                Bon.Serialize(buffer, buffersText);
            }

            //Write general project metadata
            Bon.Serialize(CurrentProject, nanoprojText);

            File.WriteAllText(scope $"{CurrentProject.Directory}objects.nanodata", objectsText);
            File.WriteAllText(scope $"{CurrentProject.Directory}buffers.nanodata", buffersText);
            File.WriteAllText(scope $"{CurrentProject.Directory}{CurrentProject.Name}.nanoproj", nanoprojText);
            return .Ok;
        }

        public static Result<void, StringView> Load(StringView projectFilePath)
        {
            CurrentProject.FilePath.Set(projectFilePath);
            CurrentProject.Directory..Set(Path.GetDirectoryPath(projectFilePath, .. scope .())).Append("\\");
            _deserializationObjectReferences.Clear();
            _deserializationBufferReferences.Clear();
            defer _deserializationObjectReferences.Clear();
            defer _deserializationBufferReferences.Clear();
            BonInit();

            //Parse objects
            {
                String objectsText = File.ReadAllText(scope $"{CurrentProject.Directory}objects.nanodata", .. scope .());
                BonContext context = .(objectsText);
                while (context.[Friend]hasMore)
                {
                    EditorObject obj = null;
                    switch (Bon.Deserialize(ref obj, context))
                    {
                        case .Ok(var updatedContext):
                            context = updatedContext;
                            _objects[obj.UID] = obj;
                        case .Err:
                            Logger.Error("Failed to load {}objects.nanodata", CurrentProject.Directory);
                            return .Err("Editor object serialization failed");
                    }
                }
            }

            //Patch object references
            for (var pair in _deserializationObjectReferences)
            {
                u64 uid = pair.0;
                ValueView val = pair.1;
                if (!_objects.ContainsKey(uid))
                {
                    Logger.Error("Error loading ProjectDB. An object with UID {} was referenced. No object with that UID exists in this project.", uid);
            		return .Err("Referenced object UID doesn't exist");
            	}
                EditorObject obj = _objects[uid];
                val.Assign(obj);
            }
            _nextObjectUID = _objects.Keys.Max() + 1;

            //Parse buffers
            {
                String buffersText = File.ReadAllText(scope $"{CurrentProject.Directory}buffers.nanodata", .. scope .());
                BonContext context = .(buffersText);
                while (context.[Friend]hasMore)
                {
                    ProjectBuffer buffer = null;
                    switch (Bon.Deserialize(ref buffer, context))
                    {
                        case .Ok(var updatedContext):
                            context = updatedContext;
                            _buffers[buffer.UID] = buffer;
                        case .Err:
                            Logger.Error("Failed to load {}buffers.nanodata", CurrentProject.Directory);
                            return .Err("Project buffer serialization failed");
                    }
                }
            }

            //Patch project buffer references
            for (var pair in _deserializationBufferReferences)
            {
                u64 uid = pair.0;
                ValueView val = pair.1;
                if (!_buffers.ContainsKey(uid))
                {
                    Logger.Error("Error loading ProjectDB. A buffer with UID {} was referenced. No buffer with that UID exists in this project.", uid);
            		return .Err("Referenced buffer UID doesn't exist");
            	}
                ProjectBuffer buffer = _buffers[uid];
                val.Assign(buffer);
            }
            _nextBufferUID = _buffers.Keys.Max() + 1;

            //Load .nanoproj file
            {
                String nanoprojText = File.ReadAllText(projectFilePath, .. scope .());
                BonContext context = .(nanoprojText);
                if (Bon.Deserialize(ref CurrentProject, context) case .Ok(var updatedContext))
                {
                    context = updatedContext;
                    CurrentProject.[Friend]Loaded = true;
                }
                else
                {
                    Logger.Error("Failed to load {}", projectFilePath);
                    return .Err("Failed to load nanoproj file");
                }
            }

            return .Ok;
        }

        private static void SerializeEditorObject(BonWriter writer, ValueView val, BonEnvironment env, SerializeValueState state = default)
        {
            if (writer.[Friend]objDepth == 0)
            {
                //Use normal object serializer at depth 0
                Bon.Integrated.Serialize.Class(writer, val, env);
            }
            else
            {
                //Any deeper and we're on the properties of depth 0 objects. Only write the UID.
                //All editor objects get written at the root level of the file. References to them only have UIDs
                EditorObject obj = val.Get<EditorObject>();
                u64 uid = obj.UID;
                Bon.Integrated.Serialize.[Friend]Integer(typeof(u64), writer, .(typeof(u64), &uid));
            }
        }
        
        private static Result<void> DeserializeEditorObject(BonReader reader, ValueView val, BonEnvironment env, DeserializeValueState state = default)
        {
            if (reader.inStr[0] == '{')
            {
                //Full object definition
                return Bon.Integrated.Deserialize.Class(reader, val, env);
            }
            else
            {
                //Object UID. Make temporary object and replace with the real object after all of them have been loaded
                switch (reader.Integer())
                {
                    case .Ok(StringView uidString):
                        switch (u64.Parse(uidString))
                        {
                            case .Ok(u64 uid):
                                EditorObject obj = val.Get<EditorObject>();
                                delete obj; //TODO: See if bon alloc handlers can be used to avoid allocating these in the first place
                                _deserializationObjectReferences.Add((uid, val));
                            case .Err(var parseError):
                                Logger.Error("Failed to parse UID {} for editor object. Error: {}", uidString, parseError.ToString(.. scope .()));
                                return .Err;
                        }
                    case .Err:
                        return .Err;
                }
            }
            return .Ok;
        }

        private static void SerializeProjectBuffer(BonWriter writer, ValueView val, BonEnvironment env, SerializeValueState state = default)
        {
            if (writer.[Friend]objDepth == 0)
            {
                //Use normal object serializer at depth 0
                Bon.Integrated.Serialize.Class(writer, val, env);
            }
            else
            {
                //Any deeper and we're on the properties of depth 0 objects. Only write the UID.
                //All editor objects get written at the root level of the file. References to them only have UIDs
                ProjectBuffer buffer = val.Get<ProjectBuffer>();
                u64 uid = buffer.UID;
                Bon.Integrated.Serialize.[Friend]Integer(typeof(u64), writer, .(typeof(u64), &uid));
            }
        }

        private static Result<void> DeserializeProjectBuffer(BonReader reader, ValueView val, BonEnvironment env, DeserializeValueState state = default)
        {
            if (reader.inStr[0] == '{')
            {
                //Full definition
                return Bon.Integrated.Deserialize.Class(reader, val, env);
            }
            else
            {
                //Buffer UID. Make temporary object and replace with the real object after all of them have been loaded
                switch (reader.Integer())
                {
                    case .Ok(StringView uidString):
                        switch (u64.Parse(uidString))
                        {
                            case .Ok(u64 uid):
                                ProjectBuffer buffer = val.Get<ProjectBuffer>();
                                delete buffer; //TODO: See if bon alloc handlers can be used to avoid allocating these in the first place
                                _deserializationBufferReferences.Add((uid, val));
                            case .Err(var parseError):
                                Logger.Error("Failed to parse UID {} for project buffer. Error: {}", uidString, parseError.ToString(.. scope .()));
                                return .Err;
                        }
                    case .Err:
                        return .Err;
                }
            }
            return .Ok;
        }

        private static void BonInit()
        {
            static bool _bonInit = false;

            //Bon serializer initialization
            if (!_bonInit)
            {
                Bon.onDeserializeError.Add(new => OnDeserializeError);

                for (Type type in Type.Types)
                {
        			if (type.HasBaseType(typeof(EditorObject)))
        		    {
                        //Make sure editor objects have the correct attributes
                        if (!type.HasCustomAttribute<ReflectAllAttribute>() && !type.HasCustomAttribute<ReflectAttribute>())
                        {
                            Logger.Fatal("{} is missing [ReflectAll] or [Reflect]! Serialization for this type most likely won't work. Fix this.", type.GetFullName(.. scope .()));
                        }

                        //Register custom handler for editor objects
                        gBonEnv.typeHandlers.Add(type, ((.)new => SerializeEditorObject, (.)new => DeserializeEditorObject));
                        gBonEnv.RegisterPolyType!(type);
        			}		
                }

                gBonEnv.typeHandlers.Add(typeof(ProjectBuffer), ((.)new => SerializeProjectBuffer, (.)new => DeserializeProjectBuffer));

                gBonEnv.RegisterPolyType!(typeof(ProjectDB.Project));
                gBonEnv.RegisterPolyType!(typeof(ProjectBuffer));
                gBonEnv.RegisterPolyType!(typeof(EditorObject));

                gBonEnv.serializeFlags |= .Verbose;
                _bonInit = true;
            }
        }

        private static void OnDeserializeError(StringView errorMessage)
        {
            Logger.Error("\n{}", errorMessage);
        }
	}

    //Binary blob of data attached to the project. Useful for bulk binary data such as textures and meshes.
    [BonTarget]
    public class ProjectBuffer
    {
        public const u64 NullUID = u64.MaxValue;

        public readonly u64 UID = NullUID;
        [BonInclude]
        private int _size = 0;
        public int Size => _size;
        public String Name = new .() ~delete _;
        private append Monitor _lock;

        public this(u64 uid, int size = 0, StringView name = "")
        {
            UID = uid;
            _size = size;
            Name.Set(name);
        }

        public void GetPath(String path)
        {
            path.Set(scope $@"{ProjectDB.CurrentProject.Directory}Buffers\{Name}.{UID}.buffer");
        }

        //Read buffer from hard drive. Caller takes ownership of the result if it returns .Ok
        public Result<List<u8>> Load()
        {
            ScopedLock!(_lock);
            var bytes = new List<u8>();
            if (File.ReadAll(GetPath(.. scope .()), bytes) case .Ok)
            {
                return .Ok(bytes);
            }
            else
            {
                delete bytes;
                return .Err;
            }
        }

        public Result<void> Save(Span<u8> data)
        {
            //TODO: Port tiny buffer merge optimization from the C++ version. Prevents having 1000s of 1KB or less files in the buffers folder since that causes performance issues.
            ScopedLock!(_lock);
            String path = GetPath(.. scope .());
            Directory.CreateDirectory(Path.GetDirectoryPath(path, .. scope .()));
            return File.WriteAll(path, data, false);
        }
    }
}