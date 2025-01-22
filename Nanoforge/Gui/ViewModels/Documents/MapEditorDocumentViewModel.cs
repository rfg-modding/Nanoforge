using System;
using System.Numerics;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Mvvm.Controls;
using Nanoforge.Editor;
using Nanoforge.Gui.ViewModels.Dialogs;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Render;
using Nanoforge.Rfg;
using Nanoforge.Rfg.Import;
using Serilog;

namespace Nanoforge.Gui.ViewModels.Documents;

public partial class MapEditorDocumentViewModel : Document
{
    [ObservableProperty]
    private Scene _scene = new();

    [ObservableProperty]
    private bool _loading = false;
    
    [ObservableProperty]
    private bool _loaded = false;
    
    [ObservableProperty]
    private bool _loadFailed = false;
    
    [ObservableProperty]
    private string _loadFailureReason = string.Empty;
    
    public string Filename { get; set; }
    public string DisplayName { get; set; }

    public TimeSpan ImportTime = new TimeSpan(0, 0, 0);
    public TimeSpan ImportAndLoadTime = new TimeSpan(0, 0, 0);

    public Territory? Map;

    public MapEditorDocumentViewModel()
    {
        if (!Design.IsDesignMode)
        {
            throw new Exception("The MapEditorDocumentViewModel parameterless constructor should only be used by the avalonia designer");
        }
        Filename = "DebugFilename.vpp_pc";
        DisplayName = "Debug display name";
    }
    
    public MapEditorDocumentViewModel(string filename, string displayName)
    {
        Filename = filename;
        DisplayName = displayName;
        TaskDialog dialog = new TaskDialog(); //Easier to create this here since it must be created on the avalonia thread
        dialog.ShowDialog(MainWindow.Instance);
        ThreadPool.QueueUserWorkItem(_ => LoadMap(dialog));
    }
    
    private void LoadMap(TaskDialog taskDialog)
    {
        try
        {
            Loading = true;
            DateTime loadingStart = DateTime.Now;
            Renderer renderer = (Application.Current as App)!.Renderer!;

            Log.Information($"Opening {Filename}...");

            Territory? findResult = NanoDB.Find<Territory>(Filename);
            if (findResult is null)
            {
                //Map needs to be imported
                Log.Information($"First time opening {Filename} in this project. Importing...");
                DateTime importStart = DateTime.Now;

                MapImporter importer = new();
                if (importer.ImportMap(Filename, taskDialog.ViewModel) is { } map)
                {
                    Map = map;
                }
                else
                {
                    LoadFailed = true;
                    LoadFailureReason = $"Failed to import {Filename}. See the log for more details";
                    Log.Error(LoadFailureReason);
                    return;
                }

                ImportTime = DateTime.Now - importStart;
                //TODO: Uncomment once unsaved changes popup logic is added
                //UnsavedChanges = true;
            }
            else
            {
                Log.Information($"Found {Filename} in NanoDB. Loading from project files...");
                Map = findResult;
            }

            //Import done. Now load the map from the project files
            if (!Map.Load(renderer, Scene))
            {
                LoadFailed = true;
                LoadFailureReason = $"Failed to load map {Filename}. Check the log for more details.";
                Log.Error(LoadFailureReason);
            }

            //TODO: Auto center camera on only/first zone
            //TODO: Count object class instances
            //TODO: Initialize inspectors if necessary in this port. In the previous version it caused a hitch when loading xml files. May not be necessary in avalonia with async.

            Scene.Init(new Vector2(1920, 1080));
            Scene.Camera!.TargetPosition = new Vector3(65.97262f, 296.2423f, -592.8933f);
            renderer.ActiveScenes.Add(Scene);
            ImportAndLoadTime = DateTime.Now - loadingStart;
            Loaded = true;
            taskDialog.ViewModel!.CloseDialog();
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Failed to load map: {Filename}");
            Loading = false;
            Loaded = false;
            LoadFailed = true;
            LoadFailureReason = ex.Message;
        }
    }

    [RelayCommand]
    private void Update(SceneFrameUpdateParams updateParams)
    {
        Scene.Update(updateParams);
    }
}