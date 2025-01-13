using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Linq;
using Avalonia.Platform.Storage;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Dock.Model.Core;
using Microsoft.Extensions.DependencyInjection;
using MsBox.Avalonia;
using MsBox.Avalonia.Enums;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Services;
using Serilog;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Threading;

namespace Nanoforge.Gui.ViewModels;

public partial class MapOption(string displayName, string fileName) : ObservableObject
{
    [ObservableProperty]
    private string _displayName = displayName;
        
    [ObservableProperty]
    private string _fileName = fileName;
}

public partial class MainWindowViewModel : ViewModelBase
{
    private readonly IFactory? _factory;
    private IRootDock? _layout;

    [ObservableProperty]
    private string _projectStatus = "No project loaded";

    [ObservableProperty]
    private ObservableCollection<MapOption> _mpMaps = new();
    
    [ObservableProperty]
    private ObservableCollection<MapOption> _wcMaps = new();

    [ObservableProperty]
    private bool _mapListLoaded = false;
    
    [ObservableProperty]
    private bool _projectOpen = false;

    private object _mapListLock = new();
    
    //TODO: Add feature to save/load layouts. The dock library git repo has example code for this in the xaml sample.
    public IRootDock? Layout
    {
        get => _layout;
        set => SetProperty(ref _layout, value);
    }

    public MainWindowViewModel()
    {
        //TODO: When implementing the outliner & inspector try using the IFactory events to detect when the currently selected document changes and switch the views.
        _factory = new DockFactory();

        Layout = _factory?.CreateLayout();
        if (Layout is { })
        {
            _factory?.InitLayout(Layout);
            if (Layout is { } root)
            {
                root.Navigate.Execute("Editor");
            }
        }

        PackfileVFS.DataFolderChanged += OnDataFolderChanged;

        if (Design.IsDesignMode)
        {
            MpMaps.Add(new("Test", "test.vpp_pc"));
            MpMaps.Add(new("Test2", "test2.vpp_pc"));
            MpMaps.Add(new("Test3", "test3.vpp_pc"));
            MpMaps.Add(new("Test4", "test4.vpp_pc"));
            WcMaps.Add(new("Test", "test.vpp_pc"));
            WcMaps.Add(new("Test2", "test2.vpp_pc"));
            WcMaps.Add(new("Test3", "test3.vpp_pc"));
            WcMaps.Add(new("Test4", "test4.vpp_pc"));
            ProjectOpen = true;
            MapListLoaded = true;
        }
    }
    
    public void CloseLayout()
    {
        if (Layout is IDock dock)
        {
            if (dock.Close.CanExecute(null))
            {
                dock.Close.Execute(null);
            }
        }
    }

    public void ResetLayout()
    {
        if (Layout is not null)
        {
            if (Layout.Close.CanExecute(null))
            {
                Layout.Close.Execute(null);
            }
        }

        var layout = _factory?.CreateLayout();
        if (layout is not null)
        {
            Layout = layout;
            _factory?.InitLayout(layout);
        }
    }

    [RelayCommand]
    public async Task NewProject()
    {
        NewProjectDialog newProjectDialog = new();
        await newProjectDialog.ShowDialog(MainWindow.Instance);
        ProjectStatus = NanoDB.CurrentProject.Name;
        ProjectOpen = true;
    }

    [RelayCommand]
    public async Task OpenProject()
    {
        try
        {
            IFileDialogService? fileDialog = App.Current.Services.GetService<IFileDialogService>();
            if (fileDialog != null)
            {
                List<FilePickerFileType> filters = new()
                {
                    new FilePickerFileType("nanoproj")
                    {
                        Patterns = new[] { "*.nanoproj" }
                    }
                };
                var result = await fileDialog.ShowOpenFileDialog(this, filters);
                if (result != null)
                {
                    if (result.Count > 0)
                    {
                        //TODO: Make this show unsaved changes popup and require user to take action before it continues
                        string projectFilePath = result[0].Path.AbsolutePath;
                        NanoDB.LoadInBackground(projectFilePath, success =>
                        {
                            if (!success)
                                return;
                
                            Dispatcher.UIThread.Post(() =>
                            {
                                ProjectStatus = NanoDB.CurrentProject.Name;
                                ProjectOpen = true;
                            });
                        });
                    }
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while opening project");
            var messageBox = MessageBoxManager.GetMessageBoxStandard("Load failed", "Error loading project. Check Log.txt.", ButtonEnum.Ok);
            await messageBox.ShowWindowDialogAsync(MainWindow.Instance);
        }
    }

    [RelayCommand]
    public async Task SaveProject()
    {
        try
        {
            NanoDB.SaveInBackground();
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while saving project");
            var messageBox = MessageBoxManager.GetMessageBoxStandard("Save failed", "Error saving project. Check Log.txt.", ButtonEnum.Ok);
            await messageBox.ShowWindowDialogAsync(MainWindow.Instance);
        }
    }

    [RelayCommand]
    public void CloseProject()
    {
        NanoDB.CloseProject();
        ProjectStatus = "No project loaded";
        ProjectOpen = false;
    }

    [RelayCommand]
    public async Task OpenRecentProject(string path)
    {
        try
        {
            //TODO: Make this show unsaved changes popup and require user to take action before it continues
            NanoDB.LoadInBackground(path, success =>
            {
                if (!success)
                    return;
                
                Dispatcher.UIThread.Post(() =>
                {
                    ProjectStatus = NanoDB.CurrentProject.Name;
                    ProjectOpen = true;
                });
            });
            ProjectStatus = NanoDB.CurrentProject.Name;
            ProjectOpen = true;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while opening recent project at '{0}'", path);
            var messageBox = MessageBoxManager.GetMessageBoxStandard("Failed to open project", "Error opening project. Check Log.txt.", ButtonEnum.Ok);
            await messageBox.ShowWindowDialogAsync(MainWindow.Instance);
        }
    }

    [RelayCommand]
    public async Task RemoveFromRecentProjectsList(string path)
    {
        if (GeneralSettings.CVar.Value.RecentProjects.Contains(path))
        {
            GeneralSettings.CVar.Value.RecentProjects.Remove(path);
            GeneralSettings.CVar.Save();
        }
    }

    [RelayCommand]
    public void OpenSettings()
    {
        
    }

    [RelayCommand]
    public void SelectDataFolder()
    {
        DataFolderSelectorDialog dataFolderSelector = new();
        dataFolderSelector.ShowDialog(MainWindow.Instance);
    }

    [RelayCommand]
    public void ExitApp()
    {
        //TODO: Make this trigger popups that warn about unsaved changes and delay closing the app until the user makes a choice 
        Environment.Exit(0);
    }

    [RelayCommand]
    public void SetDarkTheme()
    {
        App.ThemeManager?.Switch(1);
    }

    [RelayCommand]
    public void SetLightTheme()
    {
        App.ThemeManager?.Switch(0);
    }

    private void OnDataFolderChanged()
    {
        Log.Information("Data folder changed. Reloading map list for main menu.");
        LoadMapsList();
    }

    private void LoadMapsList()
    {
        try
        {
            lock (_mapListLock)
            {
                MapListLoaded = false;
                MpMaps.Clear();
                WcMaps.Clear();
                LoadMPMapsFromXtbl("//data/misc.vpp_pc/mp_levels.xtbl");
                LoadMPMapsFromXtbl("//data/misc.vpp_pc/dlc02_mp_levels.xtbl");
                LoadWCMapsFromXtbl("//data/misc.vpp_pc/wrecking_crew.xtbl");
                LoadWCMapsFromXtbl("//data/misc.vpp_pc/dlc03_wrecking_crew.xtbl");
                SortMapList(ref _mpMaps);
                SortMapList(ref _wcMaps);
                MapListLoaded = true;
            }
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while loading map list");
        }
    }

    private void LoadMPMapsFromXtbl(string path)
    {
        string? xmlText = PackfileVFS.ReadAllText(path);
        if (xmlText is null)
        {
            Log.Error($"Failed to load maps from '{path}'. Failed to read xml data from packfile.");
            return;
        }

        XDocument doc = XDocument.Parse(xmlText);
        XElement? root = doc.Root;
        XElement? table = root?.Element("Table");
        if (root is null || table is null)
        {
            Log.Error($"Failed to load maps from '{path}'. Could not find xml root or <Table> element. Make sure the xtbl is properly formatted.");
            return;
        }
        
        foreach (XElement levelElement in table.Elements("mp_level_list"))
        {
            string displayName = levelElement.Element("Name")?.Value ?? "Unknown";
            string fileName = levelElement.Element("Filename")?.Value ?? "Unknown";
            MpMaps.Add(new MapOption(displayName, fileName + ".vpp_pc"));
        }
    }
    
    private void LoadWCMapsFromXtbl(string path)
    {
        string? xmlText = PackfileVFS.ReadAllText(path);
        if (xmlText is null)
        {
            Log.Error($"Failed to load maps from '{path}'. Failed to read xml data from packfile.");
            return;
        }

        XDocument doc = XDocument.Parse(xmlText);
        XElement? root = doc.Root;
        XElement? table = root?.Element("Table");
        XElement? entry = table?.Element("entry");
        XElement? maps = entry?.Element("maps");
        if (root is null || table is null || entry is null || maps is null)
        {
            Log.Error($"Failed to load maps from '{path}'. Could not find xml root or <Table>|<entry>|<maps> elements. Make sure the xtbl is properly formatted.");
            return;
        }

        foreach (XElement levelElement in maps.Elements("map"))
        {
            //TODO: Use display_name and localize once localization support is added (display_name is a localization placeholder in this xtbl)
            string displayName = levelElement.Element("file_name")?.Value ?? "Unknown";
            string fileName = levelElement.Element("file_name")?.Value ?? "Unknown";
            WcMaps.Add(new MapOption(displayName, fileName + ".vpp_pc"));
        }
    }
    
    private void SortMapList(ref ObservableCollection<MapOption> maps)
    {
        List<MapOption> tempMapsList = new(); //Use temporary list since you can't use LINQ on ObservableCollection
        foreach (MapOption map in maps)
        {
            tempMapsList.Add(map);
        }
        tempMapsList.Sort((a, b) => String.Compare(a.DisplayName, b.DisplayName, StringComparison.Ordinal));
        maps = new ObservableCollection<MapOption>(tempMapsList);
    }

    [RelayCommand]
    private void OpenMap(MapOption map)
    {
        Console.WriteLine($"Opening {map.DisplayName}...");
        //TODO: IMPLEMENT
    }
}