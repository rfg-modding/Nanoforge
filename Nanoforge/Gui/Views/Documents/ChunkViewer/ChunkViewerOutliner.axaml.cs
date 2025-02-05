using Avalonia;
using Avalonia.Controls;
using Nanoforge.Gui.ViewModels.Documents;
using ChunkViewerDocumentViewModel = Nanoforge.Gui.ViewModels.Documents.ChunkViewer.ChunkViewerDocumentViewModel;

namespace Nanoforge.Gui.Views.Documents.ChunkViewer;

public partial class ChunkViewerOutliner : UserControl
{
    public static readonly StyledProperty<ChunkViewerDocumentViewModel?> DocumentProperty = AvaloniaProperty.Register<ChunkViewerInspector, ChunkViewerDocumentViewModel?>(nameof(Document));

    public ChunkViewerDocumentViewModel? Document
    {
        get => GetValue(DocumentProperty);
        set => SetValue(DocumentProperty, value);
    }
    
    public ChunkViewerOutliner()
    {
        InitializeComponent();
    }
}