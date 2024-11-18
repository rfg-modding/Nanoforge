using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using Serilog;

namespace Nanoforge.Editor;

//Note: Not in use yet. Added so there's something to test global object serialization on. Will be in use by real code soon when the CVar system is ported
public class GeneralSettings : EditorObject
{
    public string DataPath = "";
    public List<string> RecentProjects = new List<string>();
    public string NewProjectDirectory = ""; //So you don't have to keep picking the folder every time you make a project
}

//TODO: Add a more advanced config system that requires less boilerplate. Maybe do something like the old versions CVar system. All you have to is define a static field with the CVar<T> type
public static class Config
{
    public static string DataPath = "";

    public static List<string> RecentProjects = new();

    public static string NewProjectDirectory = "";

    public static string ConfigPath => $"{BuildConfig.AssetsBasePath}config.json";
    
    private class ConfigData
    {
        [JsonInclude]
        public string DataPath = "";
        
        [JsonInclude]
        public List<string> RecentProjects = new();
        
        [JsonInclude]
        public string NewProjectDirectory = "";
    }
    
    public static void Init()
    {
        if (File.Exists(ConfigPath))
        {
            Load();
        }
        else
        {
            Save(); //Generate config file with default values
        }
    }

    public static void Load()
    {
        try
        {
            ConfigData? data = JsonSerializer.Deserialize<ConfigData>(File.ReadAllText(ConfigPath));
            if (data == null)
            {
                throw new Exception($"Failed to load config file: {ConfigPath}");
            }
            
            DataPath = data.DataPath;
            RecentProjects = data.RecentProjects;
            NewProjectDirectory = data.NewProjectDirectory;
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to load config file");
        }
    }

    public static void Save()
    {
        try
        {
            ConfigData data = new()
            {
                DataPath = Config.DataPath,
                RecentProjects = RecentProjects,
                NewProjectDirectory = NewProjectDirectory
            };
            
            string json = JsonSerializer.Serialize<ConfigData>(data);
            File.WriteAllText(ConfigPath, json);
        }
        catch (Exception ex)
        {
            Log.Error(ex, "Failed to save config file");
        }
    }
}