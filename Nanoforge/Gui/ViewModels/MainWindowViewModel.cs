using System;
using System.Threading.Tasks;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Dock.Model.Core;
using Nanoforge.Editor;
using Nanoforge.Gui.Views;
using Nanoforge.Gui.Views.Dialogs;

namespace Nanoforge.Gui.ViewModels;

public partial class MainWindowViewModel : ViewModelBase
{
    private readonly IFactory? _factory;
    private IRootDock? _layout;

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

        //TODO: Make the data folder selector auto open when the app opens if you don't have one selected. This code is disabled since calling it immediately was causing weird
        //TODO: and inconsistent behavior like the main window not maximizing correctly, or the data selector dialog not opening at all.
        //MakeSureDataFolderIsSelectedAsync();
    }

    private async Task MakeSureDataFolderIsSelectedAsync()
    {
        if (Config.DataPath.Length == 0)
        {
            DataFolderSelectorDialog dataFolderSelector = new();
            await dataFolderSelector.ShowDialog(MainWindow.Instance);
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
    public void NewProject()
    {
        NewProjectDialog newProjectDialog = new();
        newProjectDialog.ShowDialog(MainWindow.Instance);
    }

    [RelayCommand]
    public void OpenProject()
    {
        
    }

    [RelayCommand]
    public void SaveProject()
    {
        
    }

    [RelayCommand]
    public void CloseProject()
    {
        
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
}