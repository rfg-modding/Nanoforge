#include "Config.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include <tinyxml2/tinyxml2.h>

const string mainConfigPath = "./Settings.xml";

void Config::Load()
{
    TRACE();
    EnsureValidConfig();
    Variables.clear();

    lastWriteTime_ = std::filesystem::last_write_time(mainConfigPath);
    tinyxml2::XMLDocument doc;
    doc.LoadFile(mainConfigPath.c_str());
    tinyxml2::XMLElement* settings = doc.FirstChildElement("Settings");

    //Loop through config variables and parse them
    tinyxml2::XMLElement* variableXml = settings->FirstChildElement("Setting");
    u32 i = 0;
    while (variableXml)
    {
        i++;
        //Required values
        tinyxml2::XMLElement* name = variableXml->FirstChildElement("Name");
        tinyxml2::XMLElement* type = variableXml->FirstChildElement("Type");
        tinyxml2::XMLElement* description = variableXml->FirstChildElement("Description");
        tinyxml2::XMLElement* value = variableXml->FirstChildElement("Value");

        //Optional values
        tinyxml2::XMLElement* min = variableXml->FirstChildElement("Min");
        tinyxml2::XMLElement* max = variableXml->FirstChildElement("Max");
        tinyxml2::XMLElement* showInSettings = variableXml->FirstChildElement("ShowInSettings");
        tinyxml2::XMLElement* variableXmlsPath = variableXml->FirstChildElement("SettingsPath");

        auto validXml = [](tinyxml2::XMLElement* xml) -> bool { return xml && xml->GetText(); };

        //Skip variable if any required variables aren't present or if <Type> value is invalid
        if (!validXml(name) || !validXml(type) || !validXml(description) || !value)
        {
            Log->info("Skipped variable {} in {}. Did not have all required fields: <Name>, <Type>, <Description>, and <Value>", i, Path::GetFileName(mainConfigPath));
            variableXml = variableXml->NextSiblingElement("Setting");
            continue;
        }
        ConfigType typeEnum = from_string(type->GetText());
        if (typeEnum == ConfigType::Invalid)
        {
            Log->info("Skipped variable \"{}\" in {}. <Type> value \"{}\" is invalid.", name->GetText(), Path::GetFileName(mainConfigPath), type->GetText());
            variableXml = variableXml->NextSiblingElement("Setting");
            continue;
        }

        //Make the variable
        Handle<ConfigVariable> var = CreateHandle<ConfigVariable>(name->GetText(), description->GetText(), typeEnum);

        //Set metadata values
        if (validXml(min))
            var->Min = std::stof(min->GetText());
        if (validXml(max))
            var->Max = std::stof(max->GetText());
        if (validXml(showInSettings))
            var->ShownInSettings = String::EqualIgnoreCase(string(showInSettings->GetText()), "true") ? true : false;
        if (validXml(variableXmlsPath))
            var->SettingsPath = variableXmlsPath->GetText();

        //Get and set the value based on the type enum
        if (typeEnum == ConfigType::Int)
            var->Value = (i32)value->IntText();
        else if (typeEnum == ConfigType::Uint)
            var->Value = (u32)value->IntText();
        else if (typeEnum == ConfigType::Float)
            var->Value = (f32)value->FloatText();
        else if (typeEnum == ConfigType::Bool)
            var->Value = value->BoolText();
        else if (typeEnum == ConfigType::String)
        {
            var->Value = string(value->GetText());

            //Check if it's a path
            tinyxml2::XMLElement* isFolderPath = variableXml->FirstChildElement("IsFolderPath");
            tinyxml2::XMLElement* isFilePath = variableXml->FirstChildElement("IsFilePath");
            if (validXml(isFolderPath))
                var->IsFolderPath = isFolderPath->BoolText();
            if (validXml(isFilePath))
                var->IsFilePath = isFilePath->BoolText();
        }
        else if (typeEnum == ConfigType::Vec2)
        {
            tinyxml2::XMLElement* x = value->FirstChildElement("X");
            tinyxml2::XMLElement* y = value->FirstChildElement("Y");

            if (!validXml(x) || !validXml(y))
            {
                Log->info("Vec2 config variable does not have expected values: <X>, <Y>. Skipping variable.");
                variableXml = variableXml->NextSiblingElement("Setting");
                continue;
            }
            else
            {
                var->Value = Vec2(x->FloatText(), y->FloatText());
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
                variableXml = variableXml->NextSiblingElement("Setting");
                continue;
            }
            else
            {
                var->Value = Vec3(x->FloatText(), y->FloatText(), z->FloatText());
            }

            //Check if it's a color
            tinyxml2::XMLElement* isColor = variableXml->FirstChildElement("IsColor");
            if (validXml(isColor))
                var->IsColor = isColor->BoolText();
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
                variableXml = variableXml->NextSiblingElement("Setting");
                continue;
            }
            else
            {
                var->Value = Vec4(x->FloatText(), y->FloatText(), z->FloatText(), w->FloatText());
            }

            //Check if it's a color
            tinyxml2::XMLElement* isColor = variableXml->FirstChildElement("IsColor");
            if (validXml(isColor))
                var->IsColor = isColor->BoolText();
        }
        else if (typeEnum == ConfigType::List)
        {
            //Read list of values to vector. No values is treated as an empty list
            std::vector<string> values = {};
            tinyxml2::XMLElement* subvalue = value->FirstChildElement("Item");
            while (subvalue)
            {
                values.push_back(subvalue->GetText());
                subvalue = subvalue->NextSiblingElement("Item");
            }

            var->Value = values;
        }
        else
        {
            //This shouldn't happen since there are previous type checks, but if we do reach this point then pop the incomplete ConfigVariable instance
            Log->error("Unsupported type \"{}\" reached when reading config variable \"{}\". Skipping.", typeEnum, var->Name);
        }

        Variables.push_back(var);
        variableXml = variableXml->NextSiblingElement("Setting");
    }
}

void Config::Save()
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* variables = doc.NewElement("Settings");
    doc.InsertFirstChild(variables);

    for (auto& var : Variables)
    {
        //Skip invalid types
        if (var->Type() > ConfigType::Invalid)
            continue;

        //Create node and set metadata
        tinyxml2::XMLElement* variableXml = variables->InsertNewChildElement("Setting");
        variableXml->InsertNewChildElement("Name")->SetText(var->Name.c_str());
        variableXml->InsertNewChildElement("Type")->SetText(to_string(var->Type()).c_str());
        variableXml->InsertNewChildElement("Description")->SetText(var->Description.c_str());
        variableXml->InsertNewChildElement("ShowInSettings")->SetText(var->ShownInSettings);
        variableXml->InsertNewChildElement("SettingsPath")->SetText(var->SettingsPath.c_str());
        if (var->Min)
            variableXml->InsertNewChildElement("Min")->SetText(var->Min.value());
        if (var->Max)
            variableXml->InsertNewChildElement("Max")->SetText(var->Max.value());

        //Set value. Convert to string for storage in xml
        if (var->Type() == ConfigType::Int)
            variableXml->InsertNewChildElement("Value")->SetText(std::get<i32>(var->Value));
        else if (var->Type() == ConfigType::Uint)
            variableXml->InsertNewChildElement("Value")->SetText(std::get<u32>(var->Value));
        else if (var->Type() == ConfigType::Float)
            variableXml->InsertNewChildElement("Value")->SetText(std::get<f32>(var->Value));
        else if (var->Type() == ConfigType::Bool)
            variableXml->InsertNewChildElement("Value")->SetText(std::get<bool>(var->Value));
        else if (var->Type() == ConfigType::String)
        {
            variableXml->InsertNewChildElement("Value")->SetText(std::get<string>(var->Value).c_str());
            variableXml->InsertNewChildElement("IsFolderPath")->SetText(var->IsFolderPath); //Set IsPath
            variableXml->InsertNewChildElement("IsFilePath")->SetText(var->IsFilePath); //Set IsPath
        }
        else if (var->Type() == ConfigType::Vec2)
        {
            tinyxml2::XMLElement* valueXml = variableXml->InsertNewChildElement("Value");
            Vec2 value = std::get<Vec2>(var->Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
        }
        else if (var->Type() == ConfigType::Vec3)
        {
            tinyxml2::XMLElement* valueXml = variableXml->InsertNewChildElement("Value");
            Vec3 value = std::get<Vec3>(var->Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
            valueXml->InsertNewChildElement("Z")->SetText(value.z);

            //Set IsColor
            variableXml->InsertNewChildElement("IsColor")->SetText(var->IsColor);
        }
        else if (var->Type() == ConfigType::Vec4)
        {
            tinyxml2::XMLElement* valueXml = variableXml->InsertNewChildElement("Value");
            Vec4 value = std::get<Vec4>(var->Value);
            valueXml->InsertNewChildElement("X")->SetText(value.x);
            valueXml->InsertNewChildElement("Y")->SetText(value.y);
            valueXml->InsertNewChildElement("Z")->SetText(value.z);
            valueXml->InsertNewChildElement("W")->SetText(value.w);

            //Set IsColor
            variableXml->InsertNewChildElement("IsColor")->SetText(var->IsColor);
        }
        else if (var->Type() == ConfigType::List)
        {
            tinyxml2::XMLElement* valueXml = variableXml->InsertNewChildElement("Value");
            auto value = std::get<std::vector<string>>(var->Value);
            for (auto& str : value)
            {
                tinyxml2::XMLElement* strXml = valueXml->InsertNewChildElement("Item");
                strXml->SetText(str.c_str());
            }
        }
    }

    doc.SaveFile(mainConfigPath.c_str());
    lastWriteTime_ = std::filesystem::last_write_time(mainConfigPath);
}

void Config::CreateVariable(const string& name, ConfigType type, ConfigValue value, const char* description)
{
    //Make sure there's not a variable with this name already
    for (auto& variable : Variables)
        if (variable->Name == name)
            return;

    //Create variable and add to list
    string desc = description ? description : "";
    auto configVariable = CreateHandle<ConfigVariable>(name, desc, type);
    configVariable->Value = value;
    Variables.push_back(configVariable);
}

Handle<ConfigVariable> Config::GetVariable(const string& variableName)
{
    //Find var and return it
    for (auto& var : Variables)
        if (var->Name == variableName)
            return var;

    //Return nullptr if not found
    return nullptr;
}

void Config::EnsureVariableExists(const string& variableName, ConfigType type)
{
    //Check if variable exists
    if (Exists(variableName))
        return;

    //Variable doesn't exist. Create it with default value
    auto var = CreateHandle<ConfigVariable>(variableName, "Not set", type);
    switch (type)
    {
    case ConfigType::Int:
        var->Value = (i32)0;
        break;
    case ConfigType::Uint:
        var->Value = (u32)0;
        break;
    case ConfigType::Float:
        var->Value = 0.0f;
        break;
    case ConfigType::Bool:
        var->Value = false;
        break;
    case ConfigType::String:
        var->Value = string("");
        break;
    case ConfigType::Vec2:
        var->Value = Vec2{ 0.0f, 0.0f };
        break;
    case ConfigType::Vec3:
        var->Value = Vec3{ 0.0f, 0.0f, 0.0f };
        break;
    case ConfigType::Vec4:
        var->Value = Vec4{ 0.0f, 0.0f, 0.0f, 0.0f };
        break;
    case ConfigType::List:
        var->Value = std::vector<string>();
        break;
    case ConfigType::None:
    case ConfigType::Invalid:
        THROW_EXCEPTION("Passed an invalid ConfigType value: \"{}\"", type);
    }

    Variables.push_back(var);
}

bool Config::Exists(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    bool exists = var != nullptr;
    return exists;
}

std::optional<i32> Config::GetIntReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Int)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as an integer. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<u32>(var->Value);
}

std::optional<u32> Config::GetUintReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Uint)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as an unsigned integer. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<u32>(var->Value);
}

std::optional<f32> Config::GetFloatReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Float)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a float. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<f32>(var->Value);
}

std::optional<bool> Config::GetBoolReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Bool)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a boolean. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<bool>(var->Value);
}

std::optional<string> Config::GetStringReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::String)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a string. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<string>(var->Value);
}

std::optional<Vec2> Config::GetVec2Readonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Vec2)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec2. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<Vec2>(var->Value);
}

std::optional<Vec3> Config::GetVec3Readonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Vec3)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec3. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<Vec3>(var->Value);
}

std::optional<Vec4> Config::GetVec4Readonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::Vec4)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a vec4. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<Vec4>(var->Value);
}

std::optional<std::vector<string>> Config::GetListReadonly(const string& name)
{
    Handle<ConfigVariable> var = GetVariable(name);
    if (!var)
    {
        Log->error("Tried to get config var \"{}\", which does not exist", name);
        return {};
    }
    if (var->Type() != ConfigType::List)
    {
        Log->error("Config var type mismatch! Tried to get \"{}\" as a list. It is a \"{}\".", name, to_string(var->Type()));
        return {};
    }

    return std::get<std::vector<string>>(var->Value);
}

void Config::EnsureValidConfig()
{
    //Create config if it doesn't exist
    if (!std::filesystem::exists(mainConfigPath))
    {
        Save();
    }

    //Upgrade config if it's using the old format
    tinyxml2::XMLDocument doc;
    doc.LoadFile(mainConfigPath.c_str());
    if (doc.FirstChildElement("DataPath") || doc.FirstChildElement("TerritoryFile") || doc.FirstChildElement("RecentProjects") || doc.FirstChildElement("UI_Scale") || doc.FirstChildElement("UseGeometryShaders"))
    {
        tinyxml2::XMLElement* dataPath = doc.FirstChildElement("DataPath");
        tinyxml2::XMLElement* recentProjects = doc.FirstChildElement("RecentProjects");
        tinyxml2::XMLElement* uiScale = doc.FirstChildElement("UI_Scale");
        tinyxml2::XMLElement* useGeometryShaders = doc.FirstChildElement("UseGeometryShaders");

        {
            EnsureVariableExists("Data path", ConfigType::String);
            Handle<ConfigVariable> dataPathVar = GetVariable("Data path");
            dataPathVar->Value = dataPath ? string(dataPath->GetText()) : "C:/";
            dataPathVar->ShownInSettings = false;
            dataPathVar->Description = "The path to RFG Re-mars-tered";
        }

        {
            EnsureVariableExists("Recent projects", ConfigType::List);
            Handle<ConfigVariable> recentProjectsVar = GetVariable("Recent projects");
            recentProjectsVar->ShownInSettings = false;
            recentProjectsVar->Description = "List of projects that were recently opened";

            if (recentProjects)
            {
                tinyxml2::XMLElement* path = recentProjects->FirstChildElement("Path");
                while (path)
                {
                    const char* pathText = path->GetText();
                    if (pathText)
                        std::get<std::vector<string>>(recentProjectsVar->Value).push_back(string(pathText));

                    path = path->NextSiblingElement("Path");
                }
            }
        }

        {
            EnsureVariableExists("UI Scale", ConfigType::Float);
            Handle<ConfigVariable> uiScaleVar = GetVariable("UI Scale");
            uiScaleVar->Value = uiScale ? (f32)uiScale->FloatText() : 1.0f;
            uiScaleVar->ShownInSettings = true;
            uiScaleVar->Description = "Scale of the user interface. Must restart Nanoforge to for this change to apply.";
        }

        {
            EnsureVariableExists("Use geometry shaders", ConfigType::Bool);
            Handle<ConfigVariable> useGeometryShadersVar = GetVariable("Use geometry shaders");
            useGeometryShadersVar->Value = useGeometryShaders ? useGeometryShaders->BoolText() : true;
            useGeometryShadersVar->ShownInSettings = true;
            useGeometryShadersVar->Description = "If enabled geometry shaders are used to draw lines in the 3D map view. If this is disabled lines will be thinner and harder to read. Toggleable since some older computers might not support them.";
        }

        {
            EnsureVariableExists("Show FPS", ConfigType::Bool);
            Handle<ConfigVariable> uiScaleVar = GetVariable("Show FPS");
            uiScaleVar->Value = false;
            uiScaleVar->ShownInSettings = true;
            uiScaleVar->Description = "If enabled an FPS meter is shown on the main menu bar";
        }

        {
            EnsureVariableExists("DefaultLocale", ConfigType::String);
            Handle<ConfigVariable> uiScaleVar = GetVariable("DefaultLocale");
            uiScaleVar->Value = "EN_US";
            uiScaleVar->ShownInSettings = false;
            uiScaleVar->Description = "Default locale to use when viewing localized RFG strings. Nanoforge itself doesn't support localization yet.";
        }

        Save();
    }
}