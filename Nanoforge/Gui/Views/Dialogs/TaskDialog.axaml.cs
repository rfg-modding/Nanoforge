using Avalonia.Controls;
using Avalonia.Interactivity;
using Nanoforge.Gui.ViewModels.Dialogs;

namespace Nanoforge.Gui.Views.Dialogs;

public partial class TaskDialog : Window
{
    public TaskDialogViewModel? ViewModel;

    public TaskDialog()
    {
        InitializeComponent();
        DataContext = ViewModel = new TaskDialogViewModel();
        ViewModel.Status = "";
        ViewModel.StatusLog.Clear();
        ViewModel.Step = 0;
        ViewModel.CanClose = false;
        ViewModel.TaskPercentage = 0.0f;
        ViewModel.OnClose += Close;
    }

    private void CloseButton_OnClick(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}