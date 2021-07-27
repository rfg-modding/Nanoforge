#pragma once
#include "common/Typedefs.h"
#include "GuiState.h"
#include "IGuiPanel.h"
#include "MenuItem.h"
#include <ext/WindowsWrapper.h>
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;
class DX11Renderer;
class Project;
class Config;
class Localization;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Config* config, Localization* localization);
    void Update(f32 deltaTime);
    void SaveFocusedDocument();

    GuiState State; //Global gui state provided to each panel and document

private:
    void AddMenuItem(string menuPos, bool open, Handle<IGuiPanel> panel);
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(const string& text);

    std::vector<Handle<IGuiPanel>> panels_ = {};
    std::vector<MenuItem> menuItems_ = {}; //Main menu bar items

    ImGuiID dockspaceId = 0;
    ImGuiID dockspaceCentralNodeId = 0;
    bool showModPackagingPopup_ = false;
    bool showNewProjectWindow_ = false;
    bool showOpenProjectWindow_ = false;
    bool showSaveProjectWindow_ = false;
    bool showSettingsWindow_ = false;
    bool openProjectRequested_ = false;
    bool closeProjectRequested_ = false;
    bool openRecentProjectRequested_ = false;
    string openRecentProjectRequestData_;

    Handle<IDocument> currentDocument_ = nullptr;
};