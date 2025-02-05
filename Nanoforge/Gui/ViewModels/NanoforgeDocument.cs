
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;

namespace Nanoforge.Gui.ViewModels;

public partial class NanoforgeDocument : Document
{
    //Object that should be displayed in the inspector when this document is selected
    [ObservableProperty]
    private object? _inspectorTarget = null;
    
    [ObservableProperty]
    private object? _outlinerTarget = null;
}