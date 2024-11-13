using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using Nanoforge.Gui.ViewModels;

namespace Nanoforge.Services;

public interface IFileDialogService
{
    // public Task<string[]?> ShowOpenFileDialog(ViewModelBase parent);
    public Task<IReadOnlyList<IStorageFolder>?> ShowOpenFolderDialogAsync(ViewModelBase parent);
}