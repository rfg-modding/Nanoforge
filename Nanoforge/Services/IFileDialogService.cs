using System.Collections.Generic;
using System.Threading.Tasks;
using Avalonia.Platform.Storage;
using Nanoforge.Gui.ViewModels;

namespace Nanoforge.Services;

public interface IFileDialogService
{
    public Task<IReadOnlyList<IStorageFile>?> ShowOpenFileDialog(ViewModelBase parent, IReadOnlyList<FilePickerFileType>? filters = null);
    public Task<IReadOnlyList<IStorageFolder>?> ShowOpenFolderDialogAsync(ViewModelBase parent);
}