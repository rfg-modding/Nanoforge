using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using Serilog;

namespace Nanoforge.Editor;

public static class NanoDB
{
    //Attached to a project. Loaded/saved whenever a project is.
    private static Dictionary<ulong, EditorObject> _objects = new();
    //Not attached to a project. Loaded when Nanoforge opens. Used to store persistent settings like the data folder path and recent projects list.
    private static Dictionary<ulong, EditorObject> _globalObjects = new();
    public static object _objectCreationLock = new();
    public static object _bufferCreationLock = new();
    
    //TODO: Make a more general system for separating objects into different files and selectively loading them. This will be needed to reduce save/load time bloat on projects with all the maps imported.
    public static bool LoadedGlobalObjects { get; private set; } = false;
    public static bool NewUnsavedGlobalObjects { get; private set; } = false; //Set to true when new global objects have been created but not yet saved
    public const string GlobalObjectFilePath = "./Config.nanodata";
    
    private static ulong _nextObjectUID = 0;
    private static ulong _nextBufferUID = 0;
    private static Dictionary<ulong, ProjectBuffer> _buffers = new();
    
    //TODO: Is this even needed anymore? C# serialization libraries are probably smart enough to handle references in a easier way
    //Used to patch fields referencing objects and buffers after they've all been loaded.
    //private static List<(ulong, ValueView)> _deserializationObjectReferences;
    //private static List<(ulong, ValueView)> _deserializationBufferReferences;

    public static Project CurrentProject = new();
    
    public static bool Loading { get; private set; } = false;
    public static bool Saving { get; private set; } = false;
    public static bool Ready { get; private set; } = false;

    public static string BuffersDirectory => $@"{NanoDB.CurrentProject.Directory}Buffers\";

    public class Project
    {
        [JsonInclude]
        public string Name = "No project loaded";
        [JsonInclude]
        public string Description = "";
        [JsonInclude]
        public string Author = "";
        public string Directory = "";
        public string FilePath = "";
        public bool Loaded = false;
    }
    
    public static T CreateObject<T>(string name = "") where T : EditorObject, new()
    {
        lock (_objectCreationLock)
        {
            ulong uid = _nextObjectUID++;
            T obj = new T();
            obj.SetUID(uid);
            _objects[uid] = obj;
            obj.Name = name;
            return obj;
        }
    }
    
    public static T CreateGlobalObject<T>(string name = "") where T : EditorObject, new()
    {
        lock (_objectCreationLock)
        {
            ulong uid = _nextObjectUID++;
            T obj = new T();
            obj.SetUID(uid);
            _globalObjects[uid] = obj;
            obj.Name = name;
            NewUnsavedGlobalObjects = true;
            return obj;
        }
    }

    public static ProjectBuffer? CreateBuffer(Span<byte> bytes = default, string name = "")
    {
        lock (_bufferCreationLock)
        {
            ulong uid = _nextBufferUID;
            ProjectBuffer buffer = new(uid, bytes.Length, name);
            if (!bytes.IsEmpty)
            {
                if (!buffer.Save(bytes))
                {
                    Log.Error($"Error creating project buffer '{name}'. Failed to save bytes to disk. bytes.Length = {bytes.Length}");
                    return null;
                }
            }
            
            _buffers[uid] = buffer;
            _nextBufferUID++;
            return buffer;
        }
    }

    public static void DeleteObject(EditorObject obj)
    {
        lock (_objectCreationLock)
        {
            _objects.Remove(obj.UID);
        }
    }
    
    //Used to add objects created outside of NanoDB. Like when cloning objects.
    public static void AddObject(EditorObject obj)
    {
        lock (_objectCreationLock)
        {
            ulong uid = _nextObjectUID++;
            obj.SetUID(uid);
            _objects[uid] = obj;
        }
    }

    public static EditorObject? Find(string name)
    {
        foreach (var kv in _objects)
            if (kv.Value.Name.Equals(name))
                return kv.Value;
        
        foreach (var kv in _globalObjects)
            if (kv.Value.Name.Equals(name))
                return kv.Value;
        
        return null;
    }

    public static T? Find<T>(string name) where T : EditorObject
    {
        EditorObject? obj = Find(name);
        if (obj == null)
            return null;
        else
            return (T)obj;
    }
    
    public static T FindOrCreate<T>(string name) where T : EditorObject, new()
    {
        T? find = Find<T>(name);
        if (find == null)
        {
            find = CreateObject<T>(name);
        }
        
        return find;
    }

    public static void Reset()
    {
        lock (_objectCreationLock)
        {
            lock (_bufferCreationLock)
            {
                _objects.Clear();
                _buffers.Clear();
                _nextObjectUID = 0;
                _nextBufferUID = 0;
                CurrentProject = new();
            }
        }
    }

    public static void NewProject(string directory, string name, string author, string description)
    {
        Reset();
        CurrentProject = new()
        {
            Directory = directory,
            FilePath = $"{directory}{name}.nanoproj",
            Author = author,
            Name = name,
            Description = description
        };

        try
        {
            Save();
            
            if (!Config.RecentProjects.Contains(CurrentProject.FilePath))
            {
                Config.RecentProjects.Add(CurrentProject.FilePath);
                Config.Save();
            }
            
            CurrentProject.Loaded = true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error creating project at {Directory}", directory);
        }
    }

    //TODO: Stick this in a separate thread and/or make it async. Once objects are added save time will increase significantly
    public static void Save()
    {
        try
        {
            Saving = true;
            Directory.CreateDirectory(CurrentProject.Directory);

            string nanoprojText = JsonSerializer.Serialize<Project>(CurrentProject);
            File.WriteAllText(CurrentProject.FilePath, nanoprojText);

            Log.Information($"Saved project '{CurrentProject.FilePath}'");
            
            //TODO: Port the code for saving objects and buffers.
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error saving NanoDB");
            throw;
        }
        finally
        {
            Saving = false;
        }
    }

    //TODO: Stick this in a separate thread and/or make it async. Once objects are added load time will increase significantly
    public static void Load(string projectFilePath)
    {
        try
        {
            Loading = true;
            string nanoprojText = File.ReadAllText(projectFilePath);
            string? projectDirectory = Directory.GetParent(projectFilePath)?.FullName;
            if (projectDirectory == null)
            {
                throw new Exception($"Failed to get project directory for '{projectFilePath}' while loading project");
            }
            
            Project? project = JsonSerializer.Deserialize<Project>(nanoprojText);
            if (project == null)
            {
                throw new Exception($"Failed to deserialize project file {projectFilePath}");
            }
            
            project.Directory = projectDirectory;
            project.FilePath = projectFilePath;
            project.Loaded = true;
            CurrentProject = project;

            if (!Config.RecentProjects.Contains(project.FilePath))
            {
                Config.RecentProjects.Add(project.FilePath);
                Config.Save();
            }
            
            Log.Information($"Opened project '{projectFilePath}'");
            
            //TODO: Port code for loading objects and buffer metadata. Make sure to clear any object/buffer collections and UID counters first
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading NanoDB");
            throw;
        }
        finally
        {
            Loading = false;
        }
    }
    
    //TODO: Port the rest of this class
    
}