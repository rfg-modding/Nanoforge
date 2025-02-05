using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Mvvm.Controls;

namespace Nanoforge.Gui.ViewModels.Tools;

public partial class OutlinerViewModel : Tool
{
    [ObservableProperty]
    private NanoforgeDocument? _focusedDocument = null;
}
