#pragma once
#include <tinyxml2/tinyxml2.h>
#include "Common/Typedefs.h"
#include "rfg/FileCache.h"
#include "FileEdit.h"
#include <vector>
#include <future>

class PackfileVFS;
class XtblManager;
class IXtblNode;

class Project
{
public:
    bool Load(const string& projectFilePath);
    bool Save();
    void PackageMod(const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    string GetCachePath();
    void RescanCache();
    void AddEdit(FileEdit edit);

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

    //Edits made in this project
    std::vector<FileEdit> Edits = {};

    //Used by progress bar and worker thread to show packaging status without freezing the whole UI
    bool WorkerRunning = false;
    bool PackagingCancelled = false;
    string WorkerState;
    f32 WorkerPercentage = 0.0f;
    std::future<bool> WorkerResult;

private:
    bool LoadProjectFile(const string& projectFilePath);

    //Mod packaging functions
    bool PackageModThread(const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    bool PackageXtblEdits(tinyxml2::XMLElement* modBlock, PackfileVFS* vfs, XtblManager* xtblManager);
};