using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Nanoforge.Gui.ViewModels.Tools.FileExplorer;

public partial class FileExplorerNodeViewModel : ObservableObject
{
    [ObservableProperty]
    private ExplorerNodeType _type;

    [ObservableProperty]
    private FileExplorerNodeViewModel? _parent;

    [ObservableProperty]
    private ObservableCollection<FileExplorerNodeViewModel> _children = [];

    [ObservableProperty]
    private string _text = string.Empty;

    [ObservableProperty]
    private bool _matchesSearch = true;

    [ObservableProperty]
    private bool _anyChildMatchesSearch = true;

    public string Path
    {
        get
        {
            string path = Text;
            FileExplorerNodeViewModel? parent = Parent;
            while (parent != null)
            {
                path = $"{parent.Text}/{path}";
                parent = parent.Parent;
            }
            path = $"//data/{path}";
        
            return path;
        }
    }
}