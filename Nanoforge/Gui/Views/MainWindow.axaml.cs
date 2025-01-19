using System;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using Nanoforge.Gui.ViewModels;
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Render;

namespace Nanoforge.Gui.Views;

public partial class MainWindow : Window
{
    //TODO: Getting the current window will need to be done differently if the app starts having multiple windows
    public static MainWindow Instance { get; private set; } = null!;

    public static DockFactory? DockFactory = null;
    
    public Input Input = new();
    
    public bool WindowLoaded = false; 
    
    public MainWindow()
    {
        InitializeComponent();
        WindowState = WindowState.Maximized;
        Instance = this;
        
        //Note: Convoluted way to auto open the data folder selector. On Linux if I try to open the dialog immediately in the constructor or the Loaded event, then the main window does not maximize correctly for some reason.
        //      Better ways of handling this are welcome! This approach might still have issues on slower machiens (longer wait could be required)
        //      Might be better to have a separate window that opens first if the data folder isn't set. Then open the main window.
        Dispatcher.UIThread.InvokeAsync(async () =>
        {
            while (!WindowLoaded)
            {
                await Task.Delay(50);
            }

            await Task.Delay(100);
            if (GeneralSettings.CVar.Value.DataPath.Length == 0)
            {
                DataFolderSelectorDialog dataFolderSelector = new();
                await dataFolderSelector.ShowDialog(MainWindow.Instance);
            }
            else
            {
                PackfileVFS.MountDataFolderInBackground("//data/", GeneralSettings.CVar.Value.DataPath);
            }
        });
    }
    
    private void MainWindow_OnLoaded(object? sender, RoutedEventArgs e)
    {
        WindowLoaded = true;
    }

    protected override void OnKeyDown(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        Input.SetKeyDown(e.Key);
    }
    
    protected override void OnKeyUp(KeyEventArgs e)
    {
        base.OnKeyDown(e);
        Input.SetKeyUp(e.Key);
    }

    private void Window_OnClosing(object? sender, WindowClosingEventArgs e)
    {
        Renderer? renderer = (Application.Current as App)!.Renderer;
        renderer?.Shutdown();
    }
}