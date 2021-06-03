#include "Config.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include <tinyxml2/tinyxml2.h>

const string mainConfigPath = "./Settings.xml";

void Config::Load()
{
    lastWriteTime_ = std::filesystem::last_write_time(mainConfigPath);
    tinyxml2::XMLDocument doc;
    doc.LoadFile(mainConfigPath.c_str());

    //Loop through config variables and parse them
    u32 i = 0;
    tinyxml2::XMLElement* child = doc.FirstChildElement();
    while (child)
    {
        i++;
        //Required values
        tinyxml2::XMLElement* name = child->FirstChildElement("Name");
        tinyxml2::XMLElement* type = child->FirstChildElement("Type");
        tinyxml2::XMLElement* description = child->FirstChildElement("Description");
        tinyxml2::XMLElement* value = child->FirstChildElement("Value");

        //Optional values
        tinyxml2::XMLElement* min = child->FirstChildElement("Min");
        tinyxml2::XMLElement* max = child->FirstChildElement("Max");
        tinyxml2::XMLElement* showInSettings = child->FirstChildElement("ShowInSettings");
        tinyxml2::XMLElement* settingsPath = child->FirstChildElement("SettingsPath");

        auto validXml = [](tinyxml2::XMLElement* xml) -> bool { return xml && xml->GetText(); };

        //Skip variable if any required variables aren't present or if <Type> value is invalid
        if (!validXml(name) || !validXml(type) || !validXml(description) || !validXml(value))
        {
            Log->info("Skipped variable {} in {}. Did not have all required fields: <Name>, <Type>, <Description>, and <Value>", i, Path::GetFileName(mainConfigPath));
            continue;
        }
        ConfigType typeEnum = from_string(type->GetText());
        if (typeEnum == ConfigType::Invalid)
        {
            Log->info("Skipped variable \"{}\" in {}. <Type> value \"{}\" is invalid.", name->GetText(), type->GetText());
            continue;
        }

        //Make the variable
        ConfigVariable& var = variables_.emplace_back(name->GetText(), description->GetText(), typeEnum);

        //Set metadata values
        if (!validXml(min))
            var.Min = std::stof(min->GetText());
        if (!validXml(max))
            var.Max = std::stof(max->GetText());
        if (!validXml(showInSettings))
            var.ShownInSettings = String::EqualIgnoreCase(string(showInSettings->GetText()), "true") ? true : false;
        if (!validXml(settingsPath))
            var.SettingsPath = settingsPath->GetText();

        //Get and set the value based on the type enum
        if (typeEnum == ConfigType::Int)
            var.Value = (i32)value->IntText();
        else if (typeEnum == ConfigType::Uint)
            var.Value = (u32)value->IntText();
        else if (typeEnum == ConfigType::Float)
            var.Value = (f32)value->FloatText();
        else if (typeEnum == ConfigType::Bool)
            var.Value = value->BoolText();
        else if (typeEnum == ConfigType::String)
            var.Value = string(value->GetText());
        else if (typeEnum == ConfigType::Vec2)
        {
            tinyxml2::XMLElement* x = value->FirstChildElement("X");
            tinyxml2::XMLElement* y = value->FirstChildElement("Y");

            if (!validXml(x) || !validXml(y))
            {
                Log->info("Vec2 config variable does not have expected values: <X>, <Y>. Skipping variable.");
                variables_.pop_back();
                continue;
            }
            else
            {
                var.Value = Vec2(x->FloatText(), y->FloatText());
            }
        }
        else if (typeEnum == ConfigType::Vec3)
        {
            tinyxml2::XMLElement* x = value->FirstChildElement("X");
            tinyxml2::XMLElement* y = value->FirstChildElement("Y");
            tinyxml2::XMLElement* z = value->FirstChildElement("Z");

            if (!validXml(x) || !validXml(y) || !validXml(z))
            {
                Log->info("Vec3 config variable does not have expected values: <X>, <Y>, <Z>. Skipping variable.");
                variables_.pop_back();
                continue;
            }
            else
            {
                var.Value = Vec3(x->FloatText(), y->FloatText(), z->FloatText());
            }

            //Check if it's a color
            tinyxml2::XMLElement* isColor = child->FirstChildElement("IsColor");
            if (validXml(isColor))
                var.IsColor = isColor->BoolText();
        }
        else if (typeEnum == ConfigType::Vec4)
        {
            tinyxml2::XMLElement* x = value->FirstChildElement("X");
            tinyxml2::XMLElement* y = value->FirstChildElement("Y");
            tinyxml2::XMLElement* z = value->FirstChildElement("Z");
            tinyxml2::XMLElement* w = value->FirstChildElement("W");

            if (!validXml(x) || !validXml(y) || !validXml(z) || !validXml(w))
            {
                Log->info("Vec4 config variable does not have expected values: <X>, <Y>, <Z>, <W>. Skipping variable.");
                variables_.pop_back();
                continue;
            }
            else
            {
                var.Value = Vec4(x->FloatText(), y->FloatText(), z->FloatText(), w->FloatText());
            }

            //Check if it's a color
            tinyxml2::XMLElement* isColor = child->FirstChildElement("IsColor");
            if (validXml(isColor))
                var.IsColor = isColor->BoolText();
        }
        else
        {
            //This shouldn't happen since there are previous type checks, but if we do reach this point then pop the incomplete ConfigVariable instance
            Log->error("Unsupported type \"{}\" reached when reading config variable \"{}\". Skipping.", typeEnum, var.Name);
            variables_.pop_back();
        }

        child = child->NextSiblingElement();
    }
}

void Config::Save()
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* variables = doc.NewElement("Variables");
    doc.InsertFirstChild(variables);

    for (auto& var : variables_)
    {
        //Skip invalid types
        if (var.Type() > ConfigType::Invalid)
            continue;

        //Create node and set metadata
        tinyxml2::XMLElement* variable = variables->InsertNewChildElement("Variable");
        variable->InsertNewChildElement("Name")->SetText(var.Name.c_str());
        variable->InsertNewChildElement("Type")->SetText(to_string(var.Type()).c_str());
        variable->InsertNewChildElement("Description")->SetText(var.Description.c_str());
        variable->InsertNewChildElement("ShowInSettings")->SetText(var.ShownInSettings);
        variable->InsertNewChildElement("SettingsPath")->SetText(var.SettingsPath.c_str());
        if (var.Min)
            variable->InsertNewChildElement("Min")->SetText(var.Min.value());
        if (var.Max)
            variable->InsertNewChildElement("Max")->SetText(var.Max.value());

        //Set value. Convert to string for storage in xml
        if (var.Type() == ConfigType::Int)
            variable->InsertNewChildElement("Value")->SetText(std::get<i32>(var.Value));
        else if (var.Type() == ConfigType::Uint)
            variable->InsertNewChildElement("Value")->SetText(std::get<u32>(var.Value));
        else if (var.Type() == ConfigType::Float)
            variable->InsertNewChildElement("Value")->SetText(std::get<f32>(var.Value));
        else if (var.Type() == ConfigType::Bool)
            variable->InsertNewChildElement("Value")->SetText(std::get<bool>(var.Value));
        else if (var.Type() == ConfigType::String)
            variable->InsertNewChildElement("Value")->SetText(std::get<string>(var.Value).c_str());
        else if (var.Type() == ConfigType::Vec2)
        {
            tinyxml2::XMLElement* valueXml = variable->InsertNewChildElement("Value");
            Vec2 value = std::get<Vec2>(var.Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
        }
        else if (var.Type() == ConfigType::Vec3)
        {
            tinyxml2::XMLElement* valueXml = variable->InsertNewChildElement("Value");
            Vec3 value = std::get<Vec3>(var.Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
            valueXml->InsertNewChildElement("Z")->SetText(value.z);

            //Set IsColor
            variable->InsertNewChildElement("IsColor")->SetText(var.IsColor);
        }
        else if (var.Type() == ConfigType::Vec4)
        {
            tinyxml2::XMLElement* valueXml = variable->InsertNewChildElement("Value");
            Vec4 value = std::get<Vec4>(var.Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
            valueXml->InsertNewChildElement("Z")->SetText(value.z);
            valueXml->InsertNewChildElement("W")->SetText(value.w);

            //Set IsColor
            variable->InsertNewChildElement("IsColor")->SetText(var.IsColor);
        }
    }

    doc.SaveFile(mainConfigPath.c_str());
    lastWriteTime_ = std::filesystem::last_write_time(mainConfigPath);
}

void Config::CreateVariable(ConfigVariable var)
{
    //Make sure there's not a variable with this name already
    for (auto& variable : variables_)
        if (variable.Name == var.Name)
            return;

    variables_.push_back(var);
}

void Config::Update()
{
    if (std::filesystem::last_write_time(mainConfigPath) != lastWriteTime_)
    {
        //Todo: Come up with smarter behavior. Should save any variables that were edited in memory. Probably can just add an edited bool to each that gets reset whenever a load or save happens
        variables_.clear();
        Log->info("Reloading {}...", mainConfigPath);
        Load();
    }
}

std::optional<i32> Config::GetInt(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Int)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as an integer. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<i32>(var->Value);
}

std::optional<u32> Config::GetUint(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Uint)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as an unsigned integer. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<u32>(var->Value);
}

std::optional<f32> Config::GetFloat(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Float)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a float. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<f32>(var->Value);
}

std::optional<bool> Config::GetBool(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Bool)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a boolean. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<bool>(var->Value);
}

std::optional<string> Config::GetString(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::String)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a string. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<string>(var->Value);
}

std::optional<Vec2> Config::GetVec2(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Vec2)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec2. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<Vec2>(var->Value);
}

std::optional<Vec3> Config::GetVec3(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Vec3)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec3. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<Vec3>(var->Value);
}

std::optional<Vec4> Config::GetVec4(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", variableName);
        return {};
    }
    if (var->Type() != ConfigType::Vec4)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec4. It is a \"{}\".", variableName, to_string(var->Type()));
        return {};
    }

    return std::get<Vec4>(var->Value);
}

void Config::SetInt(const string& variableName, i32 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Int);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Int)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as an integer. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetUint(const string& variableName, u32 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Uint);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Uint)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as an unsigned integer. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetFloat(const string& variableName, f32 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Float);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Float)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a float. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetBool(const string& variableName, bool value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Bool);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Bool)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a boolean. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetString(const string& variableName, string value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::String);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::String)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a string. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetVec2(const string& variableName, Vec2 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Vec2);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Vec2)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a vec2. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetVec3(const string& variableName, Vec3 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Vec3);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Vec3)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a vec3. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

void Config::SetVec4(const string& variableName, Vec4 value)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    if (!var)
    {
        ConfigVariable& newVar = variables_.emplace_back(variableName, "", ConfigType::Vec4);
        newVar.Value = value;
        return;
    }
    else if (var->Type() != ConfigType::Vec4)
    {
        Log->error("Config var type mismatch! Tried to set \"{}\" as a vec4. It is a \"{}\".", variableName, to_string(var->Type()));
        return;
    }

    var->Value = value;
}

std::optional<string> Config::GetDescription(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->Description : std::optional<string>{};
}

std::optional<f32> Config::GetMin(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->Min : std::optional<f32>{};
}

std::optional<f32> Config::GetMax(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->Max : std::optional<f32>{};
}

std::optional<ConfigType> Config::GetType(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->Type() : std::optional<ConfigType>{};
}

std::optional<bool> Config::IsShownInSettings(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->ShownInSettings : std::optional<bool>{};
}

std::optional<string> Config::SettingsPath(const string& variableName)
{
    ConfigVariable* var = GetVariableMutable(variableName);
    return var ? var->SettingsPath : std::optional<string>{};
}

std::optional<ConfigVariable> Config::GetVariable(const string& variableName)
{
    //Find var and return it
    for (auto& var : variables_)
        if (var.Name == variableName)
            return var;

    //Return nullptr if not found
    return {};
}

ConfigVariable* Config::GetVariableMutable(const string& variableName)
{
    //Find var and return it
    for (auto& var : variables_)
        if (var.Name == variableName)
            return &var;

    //Return nullptr if not found
    return nullptr;
}

const std::span<ConfigVariable> Config::GetVariableView()
{
    return std::span<ConfigVariable>{variables_.data(), variables_.size()};
}
