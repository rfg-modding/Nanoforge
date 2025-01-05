using System.Collections.Generic;
using System.Collections.ObjectModel;

namespace Nanoforge.Editor;

public class GeneralSettings : EditorObject
{
    public string DataPath = "";
    public ObservableCollection<string> RecentProjects { get; set; } = new();
    public string NewProjectDirectory = ""; //So you don't have to keep picking the folder every time you make a project

    public static CVar<GeneralSettings> CVar { get; } = new("GeneralSettings");
}