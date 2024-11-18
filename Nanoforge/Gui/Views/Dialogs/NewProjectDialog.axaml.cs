using Avalonia.Controls;
using Nanoforge.Gui.ViewModels.Dialogs;

namespace Nanoforge.Gui.Views.Dialogs;

public partial class NewProjectDialog : Window
{
    public NewProjectDialog()
    {
        InitializeComponent();
        DataContext = new NewProjectDialogViewModel();
    }
}