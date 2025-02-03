using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using MsBox.Avalonia;
using MsBox.Avalonia.Enums;
using Nanoforge.Gui.ViewModels.Tools.FileExplorer;
using Serilog;

namespace Nanoforge.Gui.Views.Tools.FileExplorer;

public partial class FileExplorerView : UserControl
{
    public static readonly List<string> SupportedFileExtensions = [".cchk_pc", ".gchk_pc"];
    
    public FileExplorerView()
    {
        InitializeComponent();
    }

    private void InitializeComponent()
    {
        AvaloniaXamlLoader.Load(this);
    }

    //TODO: See if this can be done in the ViewModel. Ideally without directly referencing. Xaml behaviors might be helpful for this.
    private void FileTreeView_OnDoubleTapped(object? sender, TappedEventArgs e)
    {
        if (sender is TreeView { SelectedItem: FileExplorerNodeViewModel node })
        {
            string extension = Path.GetExtension(node.Text);
            if (!SupportedFileExtensions.Contains(extension))
            {
                if (extension != ".vpp_pc" && extension != ".str2_pc") //Don't plan on letting people open packfiles from the explorer + the popup is annoying when you're just double clicking to expand a node
                {
                    Log.Warning($"Can not open file with extension {extension} in file explorer.");
                    var messageBox = MessageBoxManager.GetMessageBoxStandard("Unsupported file type", $"Nanoforge can't open {extension} files from the file explorer yet.", ButtonEnum.Ok);
                    messageBox.ShowWindowDialogAsync(MainWindow.Instance);   
                }
                return;
            }
            
            Log.Information($"Opening {node.Text} from the file explorer....");
        }
    }
}
