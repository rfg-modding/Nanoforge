#pragma once
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/ZoneManager.h"
#include "render/camera/Camera.h"
#include <imgui_node_editor.h>

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

    bool Visible = true;

    GuiStatus Status = Ready;
    string CustomStatusMessage = "";
    f32 StatusBarHeight = 25.0f;

    Im3d::Vec4 BoundingBoxColor = Im3d::Vec4(0.778f, 0.414f, 0.0f, 1.0f);
    f32 BoundingBoxThickness = 1.0f;
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
};