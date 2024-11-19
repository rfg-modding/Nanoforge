using System;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Extensions.DependencyInjection;
using MsBox.Avalonia;
using MsBox.Avalonia.Enums;
using Nanoforge.Editor;
using Nanoforge.Services;

namespace Nanoforge.Gui.ViewModels.Dialogs;

public partial class NewProjectDialogViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _name = "";

    [ObservableProperty]
    private string _path = "";
    
    [ObservableProperty]
    private string _description = "";
    
    [ObservableProperty]
    private string _author = "";

    [ObservableProperty]
    private bool _createProjectFolder = true;

    public NewProjectDialogViewModel()
    {
        Path = GeneralSettings.CVar.Value.NewProjectDirectory;
        Author = Environment.UserName;
    }
    
    [RelayCommand]
    private async Task BrowseToDirectory()
    {
        IFileDialogService? fileDialog = App.Current.Services.GetService<IFileDialogService>();
        if (fileDialog != null)
        {
            var result = await fileDialog.ShowOpenFolderDialogAsync(this);
            if (result != null)
            {
                if (result.Count > 0)
                {
                    Path = result[0].Path.AbsolutePath;
                }
            }
        }
    }

    [RelayCommand]
    private async Task CreateProject(Window window)
    {
        bool canCreateProject = true;
        string creationBlock = "";
        if (Name.Trim().Length == 0)
        {
            canCreateProject = false;
            creationBlock = "Please enter a name";
        }
        if (Author.Trim().Length == 0)
        {
            canCreateProject = false;
            creationBlock = "Please enter a author";
        }
        if (Path.Trim().Length == 0)
        {
            canCreateProject = false;
            creationBlock = "Please enter a path";
        }
        if (!Directory.Exists(Path))
        {
            canCreateProject = false;
            creationBlock = $"Directory at path '{Path}' does not exist";
        }

        //TODO: Improve this by disabling the create button when there are any problems and highlighting the problem on screen automatically + showing text describing the problem
        if (!canCreateProject)
        {
            var messageBox = MessageBoxManager.GetMessageBoxStandard("Can't create project", creationBlock, ButtonEnum.Ok);
            await messageBox.ShowWindowDialogAsync(window);
            return;
        }
        
        string finalProjectDirectory = CreateProjectFolder ? $"{Path}{Name}/" : $"{Path}";
        if (CreateProjectFolder)
        {
            Directory.CreateDirectory(finalProjectDirectory);
        }
        
        NanoDB.NewProject(finalProjectDirectory, Name, Author, Description);
        GeneralSettings.CVar.Value.NewProjectDirectory = Path;
        GeneralSettings.CVar.Save();
        window.Close();
    }
}