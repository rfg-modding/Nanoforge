#pragma once
#include "common/Typedefs.h"
#include "RfgTools++/types/Vec2.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <filesystem>
#include <optional>
#include <variant>
#include <vector>
#include <span>

enum class ConfigType : u32
{
    Int,
    Uint,
    Float,
    Bool,
    String,
    Vec2,
    Vec3,
    Vec4,
    None,
    Invalid
};

class ConfigVariable
{
public:
    ConfigVariable(const string& name, const string& description, ConfigType type) : Name(name), Description(description), type_(type) { }

    //Required variables
    string Name;
    string Description;
    std::variant<i32, u32, f32, bool, string, Vec2, Vec3, Vec4> Value;

    //Optional variables
    std::optional<f32> Min;
    std::optional<f32> Max;
    bool ShownInSettings = false; //Not all config vars should be in the settings menu by default. Many are more suited for development or modding
    string SettingsPath; //For sorting settings by category
    bool IsColor = false; //Optional value for Vec3 and Vec4 values to specify that they're colors

    ConfigType Type() { return type_; }

private:
    ConfigType type_ = ConfigType::None;
};

class Config
{
public:
    //Load config variables from file. Discards values in memory
    void Load();
    //Saves config variables to config files.
    void Save();
    //Create a new variable if a variable with the same name doesn't already exist.
    void CreateVariable(ConfigVariable var);
    //Reloads config files if they've changed on the hard drive.
    void Update();

    //IConfig interface functions

    //Get the value of a single variable. These return an empty std::optional<T> and log an error if the incorrect type is used.
    std::optional<i32> GetInt(const string& variableName);
    std::optional<u32> GetUint(const string& variableName);
    std::optional<f32> GetFloat(const string& variableName);
    std::optional<bool> GetBool(const string& variableName);
    std::optional<string> GetString(const string& variableName);
    std::optional<Vec2> GetVec2(const string& variableName);
    std::optional<Vec3> GetVec3(const string& variableName);
    std::optional<Vec4> GetVec4(const string& variableName);

    //Set the value of a single variable. Doesn't change the variable and logs an error if the incorrect type is used.
    void SetInt(const string& variableName, i32 value);
    void SetUint(const string& variableName, u32 value);
    void SetFloat(const string& variableName, f32 value);
    void SetBool(const string& variableName, bool value);
    void SetString(const string& variableName, string value);
    void SetVec2(const string& variableName, Vec2 value);
    void SetVec3(const string& variableName, Vec3 value);
    void SetVec4(const string& variableName, Vec4 value);

    std::optional<string> GetDescription(const string& variableName);
    std::optional<f32> GetMin(const string& variableName);
    std::optional<f32> GetMax(const string& variableName);
    std::optional<ConfigType> GetType(const string& variableName);
    std::optional<bool> IsShownInSettings(const string& variableName);
    std::optional<string> SettingsPath(const string& variableName);

    //Returns a copy of a single variable. Set must use setter functions to edit it but saves having to use multiple getters.
    std::optional<ConfigVariable> GetVariable(const string& variableName);
    //Returns a view of all config variables. You should prefer the functions that operate on a single variable when possible. 
    //This function is mainly provided to make GUIs like the variable viewer easier to code. 
    //May be invalidated if the config file is reloaded or variables change so you must get a new view each frame.
    const std::span<ConfigVariable> GetVariableView();

private:
    //Get mutable pointer to a config variable for use in GetTypeXXX() functions. Returns nullptr if the variable isn't found.
    ConfigVariable* GetVariableMutable(const string& variableName);

    //List of all loaded config variables
    std::vector<ConfigVariable> variables_ = {};
    //Last write time of config file. Used to determine if it should be reloaded due to edits.
    std::filesystem::file_time_type lastWriteTime_;
};

static string to_string(ConfigType type)
{
    switch (type)
    {
    case ConfigType::None:
        return "None";
    case ConfigType::Int:
        return "Int";
    case ConfigType::Uint:
        return "Uint";
    case ConfigType::Float:
        return "Float";
    case ConfigType::Bool:
        return "Bool";
    case ConfigType::String:
        return "String";
    case ConfigType::Vec2:
        return "Vec2";
    case ConfigType::Vec3:
        return "Vec3";
    case ConfigType::Vec4:
        return "Vec4";
    default:
        return "InvalidValue for enum ConfigType";
    }
}

//Return ConfigType from provided string. Ignores case.
static ConfigType from_string(const string& str)
{
    if (String::EqualIgnoreCase(str, "int"))
        return ConfigType::Int;
    else if (String::EqualIgnoreCase(str, "uint"))
        return ConfigType::Uint;
    else if (String::EqualIgnoreCase(str, "float"))
        return ConfigType::Float;
    else if (String::EqualIgnoreCase(str, "bool"))
        return ConfigType::Bool;
    else if (String::EqualIgnoreCase(str, "string"))
        return ConfigType::String;
    else if (String::EqualIgnoreCase(str, "vec2"))
        return ConfigType::Vec2;
    else if (String::EqualIgnoreCase(str, "vec3"))
        return ConfigType::Vec3;
    else if (String::EqualIgnoreCase(str, "vec4"))
        return ConfigType::Vec4;
    else
        return ConfigType::Invalid;
}