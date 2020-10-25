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
class ZoneManager;
class DX11Renderer;

//Maximum gui panels. Gui list is preallocated so we can have stable pointers to the panels
const u32 MaxGuiPanels = 256;

enum ThemePreset
{
    Dark,
    Blue
};

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    ~MainGui();

    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, ZoneManager* zoneManager, DX11Renderer* renderer);
    void Update(f32 deltaTime);
    void HandleResize();

    GuiState State;

private: 
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(const string& text);
    void CheckGuiListResize();

    void SetThemePreset(ThemePreset preset);

    //Size is pre-allocated with MaxGuiPanels elements. Crashes if it resizes beyond this
    std::vector<GuiPanel> panels_ = {};
    std::vector<MenuItem> menuItems_ = {};
};