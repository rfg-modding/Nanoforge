#pragma once
#include "common/Typedefs.h"
#include "GuiState.h"
#include "GuiBase.h"
#include <ext/WindowsWrapper.h>
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;
class ZoneManager;
class DX11Renderer;

struct MenuItem
{
    string Text;
    std::vector<MenuItem> Items = {};
    //Todo: Must update if we start adding panels after init. Currently do not do this
    GuiPanel* panel = nullptr;

    MenuItem* GetItem(const string& text)
    {
        for (auto& item : Items)
        {
            if (item.Text == text)
                return &item;
        }
        return nullptr;
    }
    void Draw()
    {
        if (panel)
        {
            ImGui::MenuItem(Text.c_str(), "", &panel->Open);
            return;
        }
        
        if (ImGui::BeginMenu(Text.c_str()))
        {
            for (auto& item : Items)
            {
                item.Draw();
            }
            ImGui::EndMenu();
        }
    }
};

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    ~MainGui();

    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, ZoneManager* zoneManager, DX11Renderer* renderer);
    void Update(f32 deltaTime);
    void HandleResize();

    GuiState State;

private: 
    void DrawMainMenuBar();
    void DrawDockspace();
    void GenerateMenus();
    MenuItem* GetMenu(const string& text);

    std::vector<GuiPanel> panels_ = {};
    std::vector<MenuItem> menuItems_ = {};
};