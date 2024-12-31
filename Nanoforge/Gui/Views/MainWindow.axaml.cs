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
using Nanoforge.Gui.Views.Dialogs;
using Nanoforge.Render;

namespace Nanoforge.Gui.Views;

public partial class MainWindow : Window
{
    //TODO: Getting the current window will need to be done differently if the app starts having multiple windows
    public static Window Instance { get; private set; } = null!;

    public Input Input = new();
    
    private DispatcherTimer _timer = new(new TimeSpan(days: 0, hours: 0, minutes: 0, seconds: 0, milliseconds: 2000), DispatcherPriority.Normal, OpenDataFolderSelector);

    public MainWindow()
    {
        InitializeComponent();
        WindowState = WindowState.Maximized;
        Instance = this;
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
        Renderer renderer = (Application.Current as App)!.Renderer;
        renderer.Shutdown();
    }
    
    //Note: Very convoluted way to auto open the data folder selector. On Linux if I try to open the dialog immediately in the constructor or the Loaded event, then the main window does not maximize correctly for some reason.
    //      Better ways of handling this are welcome!
    private static async void OpenDataFolderSelector(object? sender, EventArgs args)
    {
        DispatcherTimer timer = (DispatcherTimer)sender!;
        timer.Stop();

        if (GeneralSettings.CVar.Value.DataPath.Length == 0)
        {
            DataFolderSelectorDialog dataFolderSelector = new();
            await dataFolderSelector.ShowDialog(MainWindow.Instance); 
        }
        else
        {
            PackfileVFS.MountDataFolderInBackground("//data/", GeneralSettings.CVar.Value.DataPath);
        }
    }
}