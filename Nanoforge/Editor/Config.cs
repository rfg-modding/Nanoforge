using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace Nanoforge.Editor;

//TODO: Add a more advanced config system that requires less boilerplate. Maybe do something like the old versions CVar system. All you have to is define a static field with the CVar<T> type
public static class Config
{
    public static string DataPath = "";

    public static string ConfigPath => $"{BuildConfig.AssetsBasePath}config.json";
    
    private class ConfigData
    {
        [JsonInclude]
        public string DataPath = "";
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
        }
        catch (Exception ex)
        {
            //TODO: Add logging
        }
    }

    public static void Save()
    {
        try
        {
            ConfigData data = new()
            {
                DataPath = Config.DataPath,
            };
            
            string json = JsonSerializer.Serialize<ConfigData>(data);
            File.WriteAllText(ConfigPath, json);
        }
        catch (Exception ex)
        {
            //TODO: Add logging
        }
    }
}