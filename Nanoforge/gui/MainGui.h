#pragma once
#include "common/Typedefs.h"
#include "GuiState.h"
#include "GuiBase.h"
#include "MenuItem.h"
#include <ext/WindowsWrapper.h>
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;
class DX11Renderer;
class Project;

//Maximum gui panels. Gui list is preallocated so we can have stable pointers to the panels
const u32 MaxGuiPanels = 256;

enum ThemePreset
{
    Dark,
    Blue
};
//Basic enum for handling switch from welcome screen to main gui
enum GuiStateEnum
{
    //Welcome screen. Initial state, must select or create a project to get out of this
    Welcome,
    //Main gui. Where most of the app is
    Main
};

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager);
    void Update(f32 deltaTime);
    void HandleResize(u32 width, u32 height);

    GuiState State;
    GuiStateEnum StateEnum = Welcome;

private: 
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(const string& text);
    void CheckGuiListResize();

    void SetThemePreset(ThemePreset preset);

    void DrawNewProjectWindow();
    void TryOpenProject();
    void DrawSaveProjectWindow();
    void DrawWelcomeWindow();

    //Size is pre-allocated with MaxGuiPanels elements. Crashes if it resizes beyond this
    std::vector<GuiPanel> panels_ = {};
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
};