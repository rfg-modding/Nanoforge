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
    void AddPanel(string menuPos, bool open, Handle<IGuiPanel> panel);

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
    std::vector<Handle<IGuiPanel>> panels_ = {};
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

    std::vector<const char*> TerritoryList =
    {
        "terr01",
        "dlc01",
        "mp_cornered",
        "mp_crashsite",
        "mp_crescent",
        "mp_crevice",
        "mp_deadzone",
        "mp_downfall",
        "mp_excavation",
        "mp_fallfactor",
        "mp_framework",
        "mp_garrison",
        "mp_gauntlet",
        "mp_overpass",
        "mp_pcx_assembly",
        "mp_pcx_crossover",
        "mp_pinnacle",
        "mp_quarantine",
        "mp_radial",
        "mp_rift",
        "mp_sandpit",
        "mp_settlement",
        "mp_warlords",
        "mp_wasteland",
        "mp_wreckage",
        "mpdlc_broadside",
        "mpdlc_division",
        "mpdlc_islands",
        "mpdlc_landbridge",
        "mpdlc_minibase",
        "mpdlc_overhang",
        "mpdlc_puncture",
        "mpdlc_ruins",
        "wc1",
        "wc2",
        "wc3",
        "wc4",
        "wc5",
        "wc6",
        "wc7",
        "wc8",
        "wc9",
        "wc10",
        "wcdlc1",
        "wcdlc2",
        "wcdlc3",
        "wcdlc4",
        "wcdlc5",
        "wcdlc6",
        "wcdlc7",
        "wcdlc8",
        "wcdlc9"
    };
};