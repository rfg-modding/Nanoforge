using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Nanoforge.Render;

namespace Nanoforge.Gui.Views;

public partial class MainWindow : Window
{
    //TODO: Getting the current window will need to be done differently if the app starts having multiple windows
    public static Window Instance { get; private set; } = null!;

    public Input Input = new();
    
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
}