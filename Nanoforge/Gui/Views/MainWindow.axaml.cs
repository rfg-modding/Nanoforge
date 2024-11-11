using Avalonia.Controls;

namespace Nanoforge.Gui.Views;

public partial class MainWindow : Window
{
    //TODO: Getting the current window will need to be done differently if the app starts having multiple windows
    public static Window Instance { get; private set; } = null!;
    
    public MainWindow()
    {
        InitializeComponent();
        WindowState = WindowState.Maximized;
        Instance = this;
    }
}