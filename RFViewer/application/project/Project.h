#pragma once
#include "Common/Typedefs.h"
#include "rfg/FileCache.h"

class Project
{
public:
    bool Load(const string& projectFilePath);
    bool Save();
    bool PackageMod(const string& outputPath);
    string GetCachePath();

    //Name of the project
    string Name;
    //Description of the project
    string Description;
    //Author of the project
    string Author;
    //The path of the folder that contains the project file (.nanoproj) and project cache
    string Path;
    //Name of the project file
    string ProjectFilename;
    //The project cache. Located at Path + "\\Cache\\"
    FileCache Cache;
    //If true there are changes which haved been saved yet. Used by the GUI
    bool UnsavedChanges = false;

private:
    bool LoadProjectFile(const string& projectFilePath);
};