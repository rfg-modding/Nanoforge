using Avalonia.Controls;
using Avalonia.Interactivity;

namespace Nanoforge;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        WindowState = WindowState.Maximized;
    }
}