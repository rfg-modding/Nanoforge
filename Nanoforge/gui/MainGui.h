#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include "common/Handle.h"
#include "GuiState.h"
#include "MenuItem.h"
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;
class DX11Renderer;
class Project;
class Config;
class Localization;
class TextureIndex;
class IGuiPanel;
using ImGuiID = unsigned int;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Localization* localization, TextureIndex* textureSearchIndex);
    void Update();
    void SaveFocusedDocument();

    GuiState State; //Global gui state provided to each panel and document
    f32 mainMenuHeight = 8.0f;
    f32 toolbarHeight = 16.0f;
    bool Shutdown = false;

private:
    void AddMenuItem(std::string_view menuPos, bool open, Handle<IGuiPanel> panel);
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(std::string_view text);
    void SetPanelVisibility(const std::string& title, bool visible);

    std::vector<Handle<IGuiPanel>> panels_ = {};
    std::vector<MenuItem> menuItems_ = {}; //Main menu bar items

    ImGuiID dockspaceId = 0;
    ImGuiID dockspaceCentralNodeId = 0;
    bool showModPackagingPopup_ = false;
    bool showNewProjectWindow_ = false;
    bool showOpenProjectWindow_ = false;
    bool showSaveProjectWindow_ = false;
    bool showSettingsWindow_ = false;
    bool showTextureSearchGenPopup_ = false;
    bool openProjectRequested_ = false;
    bool closeProjectRequested_ = false;
    bool openRecentProjectRequested_ = false;
    string openRecentProjectRequestData_;

    Handle<IDocument> currentDocument_ = nullptr;
};