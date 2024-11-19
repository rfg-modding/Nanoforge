using System.Collections.Generic;

namespace Nanoforge.Editor;

public class GeneralSettings : EditorObject
{
    public string DataPath = "";
    public List<string> RecentProjects = new List<string>();
    public string NewProjectDirectory = ""; //So you don't have to keep picking the folder every time you make a project

    public static CVar<GeneralSettings> CVar = new("GeneralSettings");
}