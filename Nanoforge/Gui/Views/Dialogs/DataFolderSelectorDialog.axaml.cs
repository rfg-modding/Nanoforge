using Avalonia.Controls;
using Nanoforge.Gui.ViewModels.Dialogs;

namespace Nanoforge.Gui.Views.Dialogs;

public partial class DataFolderSelectorDialog : Window
{
    public DataFolderSelectorDialog()
    {
        InitializeComponent();
        DataContext = new DataFolderSelectorDialogViewModel();
    }
}