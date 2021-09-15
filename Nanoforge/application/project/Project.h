#pragma once
#include "Common/Typedefs.h"
#include "util/TaskScheduler.h"
#include "FileEdit.h"
#include <tinyxml2/tinyxml2.h>
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
    void Close();
    void PackageMod(const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    string GetCachePath();
    void RescanCache();
    void AddEdit(FileEdit edit);
    bool Loaded() { return loaded_; }

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
    //If true there are changes which haved been saved yet. Used by the GUI
    bool UnsavedChanges = false;
    //If true Nanoforge will repack table.vpp_pc with any edited files in it.
    //Temporary way of generating mods that edit table.vpp_pc.
    bool UseTableWorkaround = false;

    //Edits made in this project
    std::vector<FileEdit> Edits = {};

    //Used by progress bar and worker thread to show packaging status without freezing the whole UI
    bool WorkerRunning = false;
    bool WorkerFinished = false;
    bool PackagingCancelled = false;
    string WorkerState;
    f32 WorkerPercentage = 0.0f;
    Handle<Task> PackageModTask = Task::Create("Packaging mod...");

private:
    bool LoadProjectFile(const string& projectFilePath);

    //Mod packaging functions
    void PackageModThread(Handle<Task> task, const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    bool PackageXtblEdits(tinyxml2::XMLElement* modBlock, PackfileVFS* vfs, XtblManager* xtblManager, const string& outputPath);

    bool loaded_ = false;
};