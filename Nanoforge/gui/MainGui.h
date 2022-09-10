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

#define ToolbarEnabled false //Currently disabled since there's no use for it yet. It has undo/redo buttons but support for that hasn't been added yet

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Localization* localization, TextureIndex* textureSearchIndex);
    void Update();
    void SaveFocusedDocument();
    void SetPanelVisibility(const std::string& title, bool visible);
    //Try to close application window. Asks the user if they want to save any unsaved documents first.
    void RequestAppClose();
    //Save project and all open documents
    void SaveAll();

    GuiState State; //Global gui state provided to each panel and document
    f32 mainMenuHeight = 8.0f;
    f32 toolbarHeight = 32.0f;
    bool Shutdown = false;

private:
    void AddMenuItem(std::string_view menuPos, bool open, Handle<IGuiPanel> panel);
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(std::string_view text);
    void DrawOutlinerAndInspector();
    void DrawProjectSaveLoadDialogs();

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
    bool closeNanoforgeRequested_ = false;
    bool imguiDemoWindowOpen_ = true;
    string openRecentProjectRequestData_;

    bool outlinerOpen_ = true;
    bool inspectorOpen_ = true;
    Handle<IDocument> currentDocument_ = nullptr;
};