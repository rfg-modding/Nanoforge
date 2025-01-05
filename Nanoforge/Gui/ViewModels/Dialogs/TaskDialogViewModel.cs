using System.Collections.ObjectModel;
using Avalonia.Controls;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Nanoforge.Gui.ViewModels.Dialogs;

public partial class TaskDialogViewModel : ObservableObject
{
    [ObservableProperty]
    private string _status = string.Empty;
    
    [ObservableProperty]
    private ObservableCollection<string> _statusLog = new();
    
    [ObservableProperty]
    private int _numSteps = 1;
    
    [ObservableProperty]
    private int _step = 0;

    [ObservableProperty]
    private bool _canClose = false;

    [ObservableProperty]
    private float _taskPercentage = 0.0f;
    
    private object _lock = new();

    public delegate void CloseDelegate();
    public event CloseDelegate? OnClose;

    public TaskDialogViewModel()
    {
        if (Design.IsDesignMode)
        {
            Status = "Status";
            StatusLog.Add("Status 0...");
            StatusLog.Add("Status 1...");
            StatusLog.Add("Status 2...");
            StatusLog.Add("Status 3...");
            StatusLog.Add("Status 4...");
            StatusLog.Add("Status 5...");
            StatusLog.Add("Status 6...");
            StatusLog.Add("Status 7...");
            StatusLog.Add("Status 8...");
            _taskPercentage = 0.33f;
            CanClose = true;
        }
    }
    
    public void Setup(int numSteps)
    {
        NumSteps = numSteps;
        Status = "";
        StatusLog.Clear();
        Step = 0;
        CanClose = false;
        TaskPercentage = 0.0f;
    }
    
    public void SetStatus(string status)
    {
        Status = status;
        if (Status.Length == 0)
            return;
        
        lock (_lock)
        {
            StatusLog.Insert(0, status);
        }
    }

    //Add message to log without setting current status
    public void Log(string message)
    {
        lock (_lock)
        {
            StatusLog.Add(message);
        }
    }
    
    public void NextStep(string? newStatus = null)
    {
        Step++;
        TaskPercentage = (1.0f / NumSteps) * Step;
        if (newStatus != null)
        {
            SetStatus(newStatus);
        }
    }

    public void CloseDialog()
    {
        Dispatcher.UIThread.Post(() => OnClose?.Invoke());
    }
}