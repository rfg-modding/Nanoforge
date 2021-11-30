#include "Config.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include <tinyxml2/tinyxml2.h>

const string mainConfigPath = "./Settings.xml";
std::vector<CVar*> CVar::Instances = {};
const u32 ConfigFileFormatVersion = 3;

CVar CVar_DataPath("Data path", ConfigType::String, "The path of the RFG Re-mars-tered folder.", ConfigValue(""),
    false,  //ShowInSettings
    true, //IsFolderPath
    false //IsFilePath
);
CVar CVar_RecentProjects("Recent projects", ConfigType::List, "List of projects that were recently opened.",
    ConfigValue(std::vector<string>{})
);

void Config::Load()
{
    TRACE();
    //If config file doesn't exist generate a default one
    if (!std::filesystem::exists(mainConfigPath))
    {
        Save();
        return;
    }

    //Load config
    tinyxml2::XMLDocument doc;
    doc.LoadFile(mainConfigPath.c_str());
    tinyxml2::XMLElement* settings = doc.FirstChildElement("Settings");

    //Get config format version
    const tinyxml2::XMLAttribute* formatVersion = settings->FindAttribute("FormatVersion");
    if (!formatVersion || formatVersion->IntValue() != ConfigFileFormatVersion)
    {
        //Unsupported version. Delete and resave config file using default cvar values to fix it.
        std::filesystem::remove(mainConfigPath);
        Save();
        return;
    }

    //Loop through config variables and parse them
    tinyxml2::XMLElement* varXml = settings->FirstChildElement("Setting");
    u32 i = 0;
    while (varXml)
    {
        //Required values
        const tinyxml2::XMLAttribute* name = varXml->FindAttribute("Name");

        //Skip invalid variables
        if (!varXml || !name)
        {
            Log->info("Skipped variable {} in {}. Must have an attribute 'Name' and a value inside its braces", i, Path::GetFileName(mainConfigPath));
            varXml = varXml->NextSiblingElement("Setting");
            i++;
            continue;
        }

        //Find the variable instance
        auto find = std::ranges::find_if(CVar::Instances, [&](const CVar* cvar) { return String::EqualIgnoreCase(cvar->Name, name->Value()); });
        if (find == CVar::Instances.end())
        {
            Log->info("Skipped variable {} in {}. No variable definition found. Variables must be defined at compile time.", i, Path::GetFileName(mainConfigPath));
            varXml = varXml->NextSiblingElement("Setting");
            i++;
            continue;
        }
        CVar* var = *find;

        //Ensure xml has a value
        if (var->Type != ConfigType::List && !varXml->GetText())
        {
            Log->info("Skipped variable {} in {}. Value missing.", i, Path::GetFileName(mainConfigPath));
            varXml = varXml->NextSiblingElement("Setting");
            i++;
            continue;
        }

        //Parse variable value
        switch (var->Type)
        {
        case ConfigType::Int:
            var->Value = varXml->IntText();
            break;
        case ConfigType::Uint:
            var->Value = varXml->IntText();
            break;
        case ConfigType::Float:
            var->Value = varXml->FloatText();
            break;
        case ConfigType::Bool:
            var->Value = varXml->BoolText();
            break;
        case ConfigType::String:
            var->Value = varXml->GetText();
            break;
        case ConfigType::List:
            {
                //Read list of values to vector. No values is treated as an empty list
                std::vector<string> values = {};
                tinyxml2::XMLElement* subvalue = varXml->FirstChildElement("Item");
                while (subvalue)
                {
                    values.push_back(subvalue->GetText());
                    subvalue = subvalue->NextSiblingElement("Item");
                }

                var->Value = values;
            }
            break;
        case ConfigType::Invalid:
        default:
            LOG_ERROR("Unsupported type \"{}\" reached when reading config variable \"{}\". Skipping.", var->Type, name->Value());
            break;
        }

        //Next one
        varXml = varXml->NextSiblingElement("Setting");
        i++;
    }

    //Save so any missing values are set to their defaults
    Save();
}

void Config::Save()
{
    tinyxml2::XMLDocument doc;
    tinyxml2::XMLElement* variables = doc.NewElement("Settings");
    variables->SetAttribute("FormatVersion", ConfigFileFormatVersion);
    doc.InsertFirstChild(variables);

    //Create variable entries
    for (CVar* var : CVar::Instances)
    {
        if (var->Type >= ConfigType::Invalid)
            continue; //Invalid var type

        tinyxml2::XMLElement* varXml = variables->InsertNewChildElement("Setting");
        varXml->SetAttribute("Name", var->Name.c_str());

        //Set value
        switch (var->Type)
        {
        case ConfigType::Int:
            varXml->SetText(var->Get<i32>());
            break;
        case ConfigType::Uint:
            varXml->SetText(var->Get<u32>());
            break;
        case ConfigType::Float:
            varXml->SetText(var->Get<f32>());
            break;
        case ConfigType::Bool:
            varXml->SetText(var->Get<bool>());
            break;
        case ConfigType::String:
            varXml->SetText(var->Get<string>().c_str());
            break;
        case ConfigType::List:
        {
            for (string& str : var->Get<std::vector<string>>())
            {
                tinyxml2::XMLElement* strXml = varXml->InsertNewChildElement("Item");
                strXml->SetText(str.c_str());
            }
        }
        break;
        case ConfigType::Invalid:
        default:
            //Shouldn't be possible due to type check at top of loop
            LOG_ERROR("Unsupported type \"{}\" reached when writing config variable \"{}\". This shouldn't be able to happen...", var->Type, var->Name);
            break;
        }
    }

    //Save to file
    doc.SaveFile(mainConfigPath.c_str());
}

CVar& Config::GetCvar(std::string_view variableName) const
{
    for (CVar* var : CVar::Instances)
        if (String::EqualIgnoreCase(var->Name, variableName))
            return *var;

    THROW_EXCEPTION("Requested non-existent config var '{}'", variableName);
}

Config* Config::Get()
{
    static Config gConfig;
    return &gConfig;
}