using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Platform.Storage;
using Nanoforge.Gui.ViewModels;
using Nanoforge.Gui.Views;

namespace Nanoforge.Services;

public class FileDialogService : IFileDialogService
{
    public async Task<IReadOnlyList<IStorageFolder>?> ShowOpenFolderDialogAsync(ViewModelBase parent)
    {
        var topLevel = TopLevel.GetTopLevel(MainWindow.Instance);
        if (topLevel == null)
            return null;

        var result = await topLevel.StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions()
        {
            AllowMultiple = false,
            Title = "Select the RFG data folder"
        });
        
        return result;

    }
}