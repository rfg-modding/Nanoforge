#pragma once
#include "common/Typedefs.h"
#include "common/string/String.h"
#include "RfgTools++/types/Vec2.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <optional>
#include <variant>
#include <vector>

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
    List, //Note: This currently only supports a list of strings
    None,
    Invalid
};

using ConfigValue = std::variant<i32, u32, f32, bool, string, Vec2, Vec3, Vec4, std::vector<string>>;
class ConfigVariable
{
public:
    ConfigVariable(std::string_view name, const string& description, ConfigType type) : Name(name), Description(description), type_(type) { }

    //Required variables
    string Name;
    string Description = "Not set";
    ConfigValue Value;

    //Optional variables
    std::optional<f32> Min;
    std::optional<f32> Max;
    bool ShownInSettings = false; //Not all config vars should be in the settings menu by default. Many are more suited for development or modding
    string SettingsPath; //For sorting settings by category

    bool IsColor = false; //Optional value for Vec3 and Vec4 values to specify that they're colors
    bool IsFolderPath = false; //Optional value for strings
    bool IsFilePath = false; //Optional value for strings

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
    void CreateVariable(std::string_view name, ConfigType type, ConfigValue value, const char* description = "Not set");
    //Returns a copy of a single variable. Set must use setter functions to edit it but saves having to use multiple getters.
    Handle<ConfigVariable> GetVariable(std::string_view variableName) const;
    //Creates the variable with a default value if it doesn't exist
    void EnsureVariableExists(std::string_view variableName, ConfigType type);
    //Returns true if the variable exists
    bool Exists(std::string_view name) const;

    //Get readonly copies of config values. For convenience.
    std::optional<i32> GetIntReadonly(std::string_view name);
    std::optional<u32> GetUintReadonly(std::string_view name);
    std::optional<f32> GetFloatReadonly(std::string_view name);
    std::optional<bool> GetBoolReadonly(std::string_view name);
    std::optional<string> GetStringReadonly(std::string_view name);
    std::optional<Vec2> GetVec2Readonly(std::string_view name);
    std::optional<Vec3> GetVec3Readonly(std::string_view name);
    std::optional<Vec4> GetVec4Readonly(std::string_view name);
    std::optional<std::vector<string>> GetListReadonly(std::string_view name);

    //List of all loaded config variables
    std::vector<Handle<ConfigVariable>> Variables = {};

private:
    //Create Settings.xml if it doesn't exist. Upgrade it if it's using the old settings format.
    void EnsureValidConfig();
};

#pragma warning(disable:4505)
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
    case ConfigType::List:
        return "List";
    default:
        return "InvalidValue for enum ConfigType";
    }
}

//Return ConfigType from provided string. Ignores case.
static ConfigType from_string(std::string_view str)
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
    else if (String::EqualIgnoreCase(str, "list"))
        return ConfigType::List;
    else
        return ConfigType::Invalid;
}
#pragma warning(default:4505)