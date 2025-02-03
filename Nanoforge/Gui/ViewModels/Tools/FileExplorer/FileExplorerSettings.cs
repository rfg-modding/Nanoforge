using Nanoforge.Editor;

namespace Nanoforge.Gui.ViewModels.Tools.FileExplorer;

public class FileExplorerSettings : EditorObject
{
    public bool RegexSearchMode = false;
    public bool HideUnsupportedFormats = false;
    public bool CaseSensitiveSearch = false;
    
    public static CVar<FileExplorerSettings> CVar { get; } = new("FileExplorerSettings");
}