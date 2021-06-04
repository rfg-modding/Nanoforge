#pragma once
#include "common/Typedefs.h"

class Config;
class ImGuiFontManager;
class Project;

//GUI shown when Nanoforge starts up. Shows recent projects and ensures a valid data path is set.
class WelcomeGui
{
public:
    void Init(Config* config, ImGuiFontManager* fontManager, Project* project, u32 windowWidth, u32 windowHeight);
    void Update();
    void HandleResize(u32 width, u32 height);

    //If set to true this signals that the welcome menu is done and we can move to the next stage
    bool Done = false;

private:
    void DrawDataPathSelector();
    void DrawRecentProjectsList();

    Config* config_ = nullptr;
    ImGuiFontManager* fontManager_ = nullptr;
    Project* project_ = nullptr;
    u32 windowWidth_;
    u32 windowHeight_;

    bool showSettingsWindow_ = false;
    bool showNewProjectWindow_ = false;
    bool dataPathValid_;
    string missingVppName_; //Used in data folder path validation
};