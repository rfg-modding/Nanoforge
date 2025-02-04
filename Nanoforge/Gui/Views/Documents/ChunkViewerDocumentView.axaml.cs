using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Nanoforge.Gui.Views.Documents;

public partial class ChunkViewerDocumentView : UserControl
{
    public ChunkViewerDocumentView()
    {
        InitializeComponent();
        AvaloniaXamlLoader.Load(this);
    }

    private void InitializeComponent()
    {
        AvaloniaXamlLoader.Load(this);
    }
}