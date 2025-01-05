using System.Text.Json.Serialization;
using CommunityToolkit.Mvvm.ComponentModel;

namespace Nanoforge.Editor;

public class Project : ObservableObject
{
    [JsonInclude]
    public string Name = "No project loaded";

    [JsonInclude]
    public string Description = "";

    [JsonInclude]
    public string Author = "";

    public string Directory = "";
    public string FilePath = "";
    public bool Loaded = false;
}