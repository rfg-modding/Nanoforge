#pragma once
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/ZoneManager.h"
#include "render/camera/Camera.h"
#include <imgui_node_editor.h>

class DX11Renderer;
namespace node = ax::NodeEditor;

//Used to color status bar
enum GuiStatus
{
    Ready, //Used when the app isn't doing any background work and the gui / overlays are completely ready for use
    Working, //Used when the app is doing background work. User may have to wait for the work to complete to use the entire gui
    Error, //Used when a serious error that requires user attention is encountered
    None //Used for default function arguments
};

class GuiState
{
public:
    ImGuiFontManager* FontManager = nullptr;
    PackfileVFS* PackfileVFS = nullptr; 
    Camera* Camera = nullptr;
    ZoneManager* ZoneManager = nullptr;
    //Todo: Hide this behind a RendererFrontend class so the UI isn't directly interacting with the renderer
    DX11Renderer* Renderer = nullptr;

    bool Visible = true;

    GuiStatus Status = Ready;
    string CustomStatusMessage = "";
    f32 StatusBarHeight = 25.0f;

    f32 BoundingBoxThickness = 3.0f;
    f32 LabelTextSize = 1.0f;
    Vec4 LabelTextColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool DrawParentConnections = false;

    node::EditorContext* NodeEditor = nullptr;

    u32 SelectedZone = InvalidZoneIndex;

    Im3d::Vec3 GridOrigin = { 0.0f, 0.0f, 0.0f };
    bool GridFollowCamera = true;
    bool DrawGrid = false;
    int GridSpacing = 10;
    int GridSize = 100;

    ZoneObjectNode36* SelectedObject = nullptr;

    //Used to trigger and reload and load a different territory
    bool ReloadNeeded = false;
    string CurrentTerritoryName;
    string CurrentTerritoryShortname;

    //Set status message and enum
    void SetStatus(const string& message, GuiStatus status = None)
    {
        //Set status enum too if it's not the default argument value
        if (status != None)
            Status = status;

        CustomStatusMessage = message;
    }
    //Clear status message and set status enum to 'Ready'
    void ClearStatus()
    {
        Status = Ready;
        CustomStatusMessage = "";
    }
    //Set selected zone and update any cached data about it's objects
    void SetSelectedZone(u32 index)
    {
        //Deselect if selecting already selected zone
        if (index == SelectedZone)
        {
            SelectedZone = InvalidZoneIndex;
            return;
        }

        //Otherwise select zone and update any data reliant on the selected zone
        SelectedZone = index;
        ZoneManager->UpdateObjectClassInstanceCounts(SelectedZone);
    }
    void SetSelectedZoneObject(ZoneObjectNode36* object)
    {
        SelectedObject = object;
    }
    void SetTerritory(const string& newTerritory, bool firstLoad = false)
    {
        string terr = newTerritory;
        if (terr == "terr01")
            terr = "zonescript_terr01";
        if (terr == "dlc01")
            terr = "zonescript_dlc01";

        terr += ".vpp_pc";
        
        CurrentTerritoryName = terr;
        CurrentTerritoryShortname = newTerritory;
        if (!firstLoad)
            ReloadNeeded = true;
    }
};