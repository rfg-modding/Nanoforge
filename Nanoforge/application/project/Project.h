#pragma once
#include "Common/Typedefs.h"
#include "common/String.h"
#include "common/Handle.h"
#include "FileEdit.h"
#include <vector>
#include <future>

class PackfileVFS;
class XtblManager;
class IXtblNode;
namespace tinyxml2
{
    class XMLElement;
}
class Task;

class Project
{
public:
    Project();
    //Starts the load thread. Returns false immediately if the load thread is already running
    bool Load(std::string_view projectFilePath);
    void LoadThread(string projectFilePath);
    //Starts the save thread. Returns false immediately if the save thread is already running
    bool Save();
    void SaveThread();
    void Close();
    void PackageMod(std::string outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    string GetCachePath();
    void RescanCache();
    void AddEdit(FileEdit edit);
    bool Loaded() const { return loaded_; }

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
    //If true mods will be written to CustomOutputPath instead of "your_mod/output/". Useful to save a bit of time having to copy it the the mod manager folder each time
    bool UseCustomOutputPath = false;
    string CustomOutputPath;

    //Edits made in this project
    std::vector<FileEdit> Edits = {};

    //Used by progress bar and worker thread to show packaging status without freezing the whole UI
    bool WorkerRunning = false;
    bool WorkerFinished = false;
    bool PackagingCancelled = false;
    string WorkerState;
    f32 WorkerPercentage = 0.0f;
    Handle<Task> PackageModTask;

    //Threaded saving
    Handle<Task> SaveProjectTask;
    bool Saving = false;
    f32 SaveThreadPercentage = 0.0f;
    string SaveThreadState;

    //Threaded loading
    Handle<Task> LoadProjectTask;
    bool Loading = false;
    f32 LoadThreadPercentage = 0.0f;
    string LoadThreadState;

private:
    bool LoadProjectFile(std::string_view projectFilePath);

    //Mod packaging functions
    void PackageModThread(Handle<Task> task, std::string outputPath, PackfileVFS* vfs, XtblManager* xtblManager);
    bool PackageXtblEdits(tinyxml2::XMLElement* modBlock, PackfileVFS* vfs, XtblManager* xtblManager, std::string_view outputPath);

    bool loaded_ = false;
};