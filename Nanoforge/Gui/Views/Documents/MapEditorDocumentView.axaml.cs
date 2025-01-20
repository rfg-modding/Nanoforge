using Avalonia.Controls;
using Avalonia.Markup.Xaml;

namespace Nanoforge.Gui.Views.Documents;

public partial class MapEditorDocumentView : UserControl
{
    public MapEditorDocumentView()
    {
        InitializeComponent();
        AvaloniaXamlLoader.Load(this);
    }

    private void InitializeComponent()
    {
        AvaloniaXamlLoader.Load(this);
    }
}