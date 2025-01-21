using System;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Nanoforge.Gui.ViewModels.Documents;
using Nanoforge.Gui.Views;
using Serilog;

namespace Nanoforge.Gui.ViewModels;

public partial class MapOptionViewModel(string displayName, string fileName) : ObservableObject
{
    [ObservableProperty]
    private string _displayName = displayName;
        
    [ObservableProperty]
    private string _fileName = fileName;
    
    [RelayCommand]
    private void OpenMap(MapOptionViewModel map)
    {
        DockFactory? dockFactory = MainWindow.DockFactory;
        if (dockFactory is null)
        {
            Log.Error("DockFactory not set when MainWindowViewModel.OpenMap() was called. Something went wrong.");
            throw new Exception("DockFactory not set when MainWindowViewModel.OpenMap() was called. Something went wrong.");
        }
        
        MapEditorDocumentViewModel document = new(map.FileName, map.DisplayName);
        document.Title = map.DisplayName;
        dockFactory.DocumentDock?.AddNewDocument<MapEditorDocumentViewModel>(document);
    }
}