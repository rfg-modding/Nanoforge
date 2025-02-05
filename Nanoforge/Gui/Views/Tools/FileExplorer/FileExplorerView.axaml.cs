using System;
using System.Collections.Generic;
using System.IO;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Markup.Xaml;
using MsBox.Avalonia;
using MsBox.Avalonia.Enums;
using Nanoforge.Gui.ViewModels;
using Nanoforge.Gui.ViewModels.Documents.ChunkViewer;
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
        if (sender is not TreeView { SelectedItem: FileExplorerNodeViewModel node })
            return;
        
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
        OpenFile(node);
    }

    private void OpenFile(FileExplorerNodeViewModel node)
    {
        try
        {
            //TODO: DO THIS BEFORE COMMIT - USE XTBL AS EXAMPLE OF ANOTHER FILE FORMAT THAT DOESN'T HAVE A GPU FILE
            //TODO: MAYBE ADD BASIC VIEWER FOR ANOTHER FORMAT LIKE STATIC MESHES TO BE SURE EVERYTHING WORKS CORRECTLY
            //TODO: Rewrite this to support an arbitrary number of formats. Support opening from either the cpu file or gpu file like below
            string extension = Path.GetExtension(node.Text);
            string cpuFilePath;
            if (extension == ".cchk_pc")
            {
                cpuFilePath = node.Path;
            }
            else if (extension == ".gchk_pc")
            {
                string gpuFilePath = node.Path;
                cpuFilePath = gpuFilePath.Replace(".gchk_pc", ".cchk_pc");
            }
            else
            {
                throw new Exception($"Unsupported file extension {extension}");
            }
            
            Console.WriteLine($"Opening {node.Text} from {cpuFilePath}");
            
            DockFactory? dockFactory = MainWindow.DockFactory;
            if (dockFactory is null)
            {
                Log.Error("DockFactory not set when FileExplorerView.OpenFile() was called. Something went wrong.");
                throw new Exception("DockFactory not set when FileExplorerView.OpenFile() was called. Something went wrong.");
            }
        
            ChunkViewerDocumentViewModel document = new(cpuFilePath);
            document.Title = node.Text;
            dockFactory.DocumentDock?.AddNewDocument<ChunkViewerDocumentViewModel>(document);
        }
        catch (Exception ex)
        {
            Log.Error(ex, $"Error opening file {node.Text} in the file explorer.");
        }
    }
}
