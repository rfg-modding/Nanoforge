using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.VisualTree;

namespace Nanoforge;

public partial class MainView : UserControl
{
    public MainView()
    {
        InitializeComponent();
    }
    
    private void MainView_OnLoaded(object? sender, RoutedEventArgs e)
    {
        //Used to make these collapsed by default. Not sure why you wouldn't call UnpinDockable to this though...
        Dock?.Factory?.PinDockable(FileExplorer);
        Dock?.Factory?.PinDockable(Output);
    }
    
    private void MenuItemNewProject_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemOpenProject_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemSaveProject_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemCloseProject_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemSettings_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemSelectDataFolder_OnClick(object? sender, RoutedEventArgs e)
    {
        throw new System.NotImplementedException();
    }

    private void MenuItemExitApp_OnClick(object? sender, RoutedEventArgs e)
    {
        Environment.Exit(0);
    }
}