using System.Collections.ObjectModel;
using System.Windows.Input;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Nanoforge.Gui.ViewModels;

public class MenuItemViewModel : ObservableObject
{
    public string Header { get; set; } = string.Empty;
    public ICommand? Command { get; set; } = null;
    public object? CommandParameter { get; set; } = null;
    public bool IsEnabled { get; set; } = true;
    public ObservableCollection<MenuItemViewModel> Items { get; set; } = new();
}
