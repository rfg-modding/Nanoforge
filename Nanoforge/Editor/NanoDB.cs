using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using Nanoforge.Gui.ViewModels.Dialogs;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Serilog;

namespace Nanoforge.Editor;

public static class NanoDB
{
    //Attached to a project. Loaded/saved whenever a project is.
    private static Dictionary<ulong, EditorObject> _objects = new();

    //Not attached to a project. Loaded when Nanoforge opens. Used to store persistent settings like the data folder path and recent projects list.
    private static Dictionary<ulong, EditorObject> _globalObjects = new();
    private static object _objectCreationLock = new();

    //TODO: Make a more general system for separating objects into different files and selectively loading them. This will be needed to reduce save/load time bloat on projects with all the maps imported.
    public static bool LoadedGlobalObjects { get; private set; } = false;
    public static bool NewUnsavedGlobalObjects { get; private set; } = false; //Set to true when new global objects have been created but not yet saved
    private const string GlobalObjectFilePath = "./Config.nanodata";

    private static ulong _nextObjectUID = 0;

    public static Project CurrentProject = new();

    public static bool Loading { get; private set; } = false;
    public static bool Saving { get; private set; } = false;
    public static bool Ready { get; private set; } = false;

    public static string BuffersDirectory => $@"{CurrentProject.Directory}Buffers/";
    
    public delegate void ProjectDoneLoadingHandler(bool success);
    public delegate void ProjectDoneSavingHandler(bool success);

    private static JsonSerializerOptions ObjectJsonSerializerOptions => new()
    {
        WriteIndented = true,
        IncludeFields = true,
        IgnoreReadOnlyFields = true,
        TypeInfoResolver = new NanoDBTypeResolver(),
        ReferenceHandler = new NanoDBReferenceHandler(),
    };

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
        lock (_objectCreationLock)
        {
            ulong uid = _nextObjectUID;
            ProjectBuffer buffer = new(uid, bytes.Length, name);
            if (!bytes.IsEmpty)
            {
                if (!buffer.Save(bytes))
                {
                    Log.Error($"Error creating project buffer '{name}'. Failed to save bytes to disk. bytes.Length = {bytes.Length}");
                    return null;
                }
            }

            _objects[uid] = buffer;
            _nextObjectUID++;
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

    public static void AddGlobalObject(EditorObject obj)
    {
        lock (_objectCreationLock)
        {
            ulong uid = _nextObjectUID++;
            obj.SetUID(uid);
            _globalObjects[uid] = obj;
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

    public static EditorObject? Find(ulong uid)
    {
        foreach (var kv in _objects)
            if (kv.Value.UID == uid)
                return kv.Value;

        foreach (var kv in _globalObjects)
            if (kv.Value.UID == uid)
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

    public static T? Find<T>(ulong uid) where T : EditorObject
    {
        EditorObject? obj = Find(uid);
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
            _objects.Clear();
            _nextObjectUID = 0;
            CurrentProject = new();
        }
    }

    public static void NewProject(string directory, string name, string author, string description, bool showTaskDialog = true)
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
            TaskDialogViewModel? status = null;
            if (showTaskDialog)
            {
                TaskDialog dialog = new TaskDialog();
                dialog.ShowDialog(MainWindow.Instance);
            }
            Save(status);

            if (!GeneralSettings.CVar.Value.RecentProjects.Contains(CurrentProject.FilePath))
            {
                GeneralSettings.CVar.Value.RecentProjects.Insert(0, CurrentProject.FilePath);
                GeneralSettings.CVar.Save();
            }

            CurrentProject.Loaded = true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error creating project at {Directory}", directory);
        }
    }

    private static string GetObjectsFilePath()
    {
        return $"{CurrentProject.Directory}objects.nanodata";
    }
    
    public static void SaveInBackground(ProjectDoneSavingHandler? doneSavingHandler = null)
    {
        TaskDialog dialog = new TaskDialog();
        dialog.ShowDialog(MainWindow.Instance);
        ThreadPool.QueueUserWorkItem(_ => Save(dialog.ViewModel, doneSavingHandler));
    }
    
    public static void Save(TaskDialogViewModel? status = null, ProjectDoneSavingHandler? doneSavingHandler = null)
    {
        try
        {
            status?.Setup(1, $"Saving {CurrentProject.Name}");
            status?.SetStatus("Saving...");
            lock (_objectCreationLock)
            {
                Saving = true;
                Directory.CreateDirectory(CurrentProject.Directory);

                string nanoprojText = JsonSerializer.Serialize<Project>(CurrentProject);
                File.WriteAllText(CurrentProject.FilePath, nanoprojText);

                using var objectStream = new FileStream(GetObjectsFilePath(), FileMode.Create, FileAccess.Write, FileShare.None);
                JsonSerializer.Serialize(objectStream, _objects, ObjectJsonSerializerOptions);

                Log.Information($"Saved project '{CurrentProject.FilePath}'");
            }
            status?.NextStep();
            status?.SetStatus($"Done!");
            status?.CloseDialog();
            
            doneSavingHandler?.Invoke(true);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error saving NanoDB");
            if (status != null)
            {
                status.SetStatus("Error saving project. Check the log for more details.");
                status.CanClose = true;
            }
            doneSavingHandler?.Invoke(false);
        }
        finally
        {
            Saving = false;
        }
    }
    
    public static void LoadInBackground(string projectFilePath, ProjectDoneLoadingHandler? doneLoadingHandler)
    {
        TaskDialog dialog = new TaskDialog();
        dialog.ShowDialog(MainWindow.Instance);
        ThreadPool.QueueUserWorkItem(_ => Load(projectFilePath, dialog.ViewModel, doneLoadingHandler));
    }

    public static void Load(string projectFilePath, TaskDialogViewModel? status = null, ProjectDoneLoadingHandler? doneLoadingHandler = null)
    {
        try
        {
            status?.Setup(2, $"Loading {CurrentProject.Name}");
            status?.SetStatus("Loading...");
            lock (_objectCreationLock)
            {
                Reset();
                Loading = true;
                string nanoprojText = File.ReadAllText(projectFilePath);
                string? projectDirectory = Directory.GetParent(projectFilePath)?.FullName;
                if (projectDirectory == null)
                {
                    throw new Exception($"Failed to get project directory for '{projectFilePath}' while loading project");
                }

                status?.SetStatus($"Loading .nanoproj file...");
                Project? project = JsonSerializer.Deserialize<Project>(nanoprojText);
                if (project == null)
                {
                    throw new Exception($"Failed to deserialize project file {projectFilePath}");
                }
                status?.NextStep();

                project.Directory = projectDirectory;
                if (!Path.EndsInDirectorySeparator(project.Directory))
                {
                    project.Directory += Path.DirectorySeparatorChar;
                }

                project.FilePath = projectFilePath;
                project.Loaded = true;
                CurrentProject = project;

                if (!GeneralSettings.CVar.Value.RecentProjects.Contains(project.FilePath))
                {
                    GeneralSettings.CVar.Value.RecentProjects.Insert(0, project.FilePath);
                    GeneralSettings.CVar.Save();
                }

                status?.SetStatus($"Loading editor objects...");
                using var objectStream = new FileStream(GetObjectsFilePath(), FileMode.Open, FileAccess.Read, FileShare.None);
                _objects.Clear();
                _objects = JsonSerializer.Deserialize<Dictionary<ulong, EditorObject>>(objectStream, ObjectJsonSerializerOptions) ?? new();
                _nextObjectUID = Math.Max(_objects.Keys.DefaultIfEmpty().Max(), _globalObjects.Keys.DefaultIfEmpty().Max()) + 1;

                Log.Information($"Opened project '{projectFilePath}'. Project name: {CurrentProject.Name}");
            }
            status?.NextStep();
            status?.SetStatus($"Done!");
            status?.CloseDialog();

            doneLoadingHandler?.Invoke(true);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error loading NanoDB");
            if (status != null)
            {
                status.SetStatus("Error loading project. Check the log for more details.");
                status.CanClose = true;
            }
            doneLoadingHandler?.Invoke(false);
        }
        finally
        {
            Loading = false;
        }
    }

    public static void SaveGlobals()
    {
        try
        {
            lock (_objectCreationLock)
            {
                Saving = true;
                using var globalObjectsStream = new FileStream(GlobalObjectFilePath, FileMode.Create, FileAccess.Write, FileShare.None);
                JsonSerializer.Serialize(globalObjectsStream, _globalObjects, ObjectJsonSerializerOptions);
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error saving global NanoDB objects");
            throw;
        }
        finally
        {
            Saving = false;
        }
    }

    public static void LoadGlobals()
    {
        try
        {
            lock (_objectCreationLock)
            {
                if (!File.Exists(GlobalObjectFilePath))
                {
                    return;
                }

                Saving = true;
                using var globalObjectStream = new FileStream(GlobalObjectFilePath, FileMode.Open, FileAccess.Read, FileShare.None);
                _globalObjects.Clear();
                _globalObjects = JsonSerializer.Deserialize<Dictionary<ulong, EditorObject>>(globalObjectStream, ObjectJsonSerializerOptions) ?? new();
                _nextObjectUID = Math.Max(_objects.Keys.DefaultIfEmpty().Max(), _globalObjects.Keys.DefaultIfEmpty().Max()) + 1;
                LoadedGlobalObjects = true;
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error saving global NanoDB objects");
            throw;
        }
        finally
        {
            Saving = false;
        }
    }

    public static void ClearGlobals()
    {
        _globalObjects.Clear();
    }

    public static void CloseProject()
    {
        Reset();
        CurrentProject = new()
        {
            Loaded = false
        };
    }
}