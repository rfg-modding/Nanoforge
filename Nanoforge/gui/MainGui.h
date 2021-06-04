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

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Config* config);
    void Update(f32 deltaTime);
    void HandleResize(u32 width, u32 height);
    void AddPanel(string menuPos, bool open, Handle<IGuiPanel> panel);

    GuiState State; //Global gui state provided to each panel and document

private: 
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(const string& text);

    void TryOpenProject();
    void DrawSaveProjectWindow();

    //gui_ panels
    std::vector<Handle<IGuiPanel>> panels_ = {};
    //Tree of gui panels. Used to categorize them in the main menu bar.
    std::vector<MenuItem> menuItems_ = {};

    //Docking data
    ImGuiID dockspaceId = 0;

    bool showNewProjectWindow_ = false;
    bool showOpenProjectWindow_ = false;
    bool showSaveProjectWindow_ = false;

    u32 windowWidth_;
    u32 windowHeight_;

    //Used by popup that tells you if your data path in Settings.xml doesn't have one of the expected vpp_pc files
    bool showDataPathErrorPopup_ = false;
    string dataPathValidationErrorMessage_;

    bool showSettingsWindow_ = false;
};