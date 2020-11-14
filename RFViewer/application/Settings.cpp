#include "Settings.h"
#include "Log.h"
#include <common/string/String.h>
#include <tinyxml2/tinyxml2.h>
#include <filesystem>

//Set to default values so these are written if a settings file isn't present on load
string Settings_PackfileFolderPath = "C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered";
string Settings_TerritoryFilename = "zonescript_terr01.vpp_pc";
std::vector<string> Settings_RecentProjects = {};

void Settings_Read()
{
    namespace fs = std::filesystem;

    //Create default file if it doesn't exist
    if (!fs::exists("./Settings.xml"))
        Settings_Write();

    tinyxml2::XMLDocument settings;
    settings.LoadFile("./Settings.xml");
    const char* dataPath = settings.FirstChildElement("DataPath")->GetText();
    if (!dataPath) //Todo: Make this more fault tolerant. Use default values where possible
        THROW_EXCEPTION("Failed to get <DataPath> from Settings.xml");

    const char* territoryFile = settings.FirstChildElement("TerritoryFile")->GetText();
    if (!territoryFile)
        THROW_EXCEPTION("Failed to get <TerritoryFile> from Settings.xml");

    Settings_PackfileFolderPath = string(dataPath);
    Settings_TerritoryFilename = string(territoryFile);

    //Temporary compatibility patches for convenience. Previous versions expected vpp_pc files instead of shorthand names
    if (Settings_TerritoryFilename == "zonescript_terr01.vpp_pc")
        Settings_TerritoryFilename = "terr01";
    if (Settings_TerritoryFilename == "zonescript_dlc01.vpp_pc")
        Settings_TerritoryFilename = "dlc01";
    if (String::EndsWith(Settings_TerritoryFilename, ".vpp_pc"))
    {
        size_t postfixIndex = Settings_TerritoryFilename.find(".vpp_pc");
        Settings_TerritoryFilename = Settings_TerritoryFilename.substr(0, postfixIndex);
    }

    //Get recent projects paths
    tinyxml2::XMLElement* recentProjects = settings.FirstChildElement("RecentProjects");
    tinyxml2::XMLElement* path = recentProjects->FirstChildElement("Path");
    while (path)
    {
        const char* pathText = path->GetText();
        if (pathText)
            Settings_RecentProjects.push_back(string(pathText));

        path = path->NextSiblingElement("Path");
    }

}

void Settings_Write()
{
    tinyxml2::XMLDocument settings;

    auto* dataPath = settings.NewElement("DataPath");
    settings.InsertEndChild(dataPath);
    dataPath->SetText(Settings_PackfileFolderPath.c_str());

    auto* territoryFile = settings.NewElement("TerritoryFile");
    settings.InsertEndChild(territoryFile);
    territoryFile->SetText(Settings_TerritoryFilename.c_str());

    //Save most recently opened projects
    auto* recentProjects = settings.NewElement("RecentProjects");
    settings.InsertEndChild(recentProjects);
    for (auto& path : Settings_RecentProjects)
    {
        auto* pathNode = recentProjects->InsertNewChildElement("Path");
        pathNode->SetText(path.c_str());
    }

    settings.SaveFile("./Settings.xml");
}

void Settings_AddRecentProjectPathUnique(const string& path)
{
    for (auto& recentProjectPath : Settings_RecentProjects)
        if (path == recentProjectPath)
            return;

    Settings_RecentProjects.push_back(path);
}