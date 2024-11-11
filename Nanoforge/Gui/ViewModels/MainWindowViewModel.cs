using System;
using System.Diagnostics;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Dock.Model.Core;

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
        
    }

    [RelayCommand]
    public void ExitApp()
    {
        //TODO: Make this trigger popups that warn about unsaved changes and delay closing the app until the user makes a choice 
        Environment.Exit(0);
    }
}