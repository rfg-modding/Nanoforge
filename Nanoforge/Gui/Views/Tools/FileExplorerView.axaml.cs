using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Nanoforge.Gui.Views.Tools;

public partial class FileExplorerView : UserControl
{
    public FileExplorerView()
    {
        InitializeComponent();
    }

    private void InitializeComponent()
    {
        AvaloniaXamlLoader.Load(this);
    }
}
