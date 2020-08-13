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
    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr; 
    Camera* camera_ = nullptr;
    ZoneManager* zoneManager_ = nullptr;

    bool Visible = true;

    GuiStatus status_ = Ready;
    string customStatusMessage_ = "";
    f32 statusBarHeight_ = 25.0f;

    Im3d::Vec4 boundingBoxColor_ = Im3d::Vec4(0.778f, 0.414f, 0.0f, 1.0f);
    f32 boundingBoxThickness_ = 1.0f;
    f32 labelTextSize_ = 1.0f;
    Vec4 labelTextColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool drawParentConnections_ = false;

    node::EditorContext* nodeEditor_ = nullptr;

    u32 selectedZone = InvalidZoneIndex;

    Im3d::Vec3 gridOrigin_ = { 0.0f, 0.0f, 0.0f };
    bool gridFollowCamera_ = true;
    bool drawGrid_ = false;
    int gridSpacing_ = 10;
    int gridSize_ = 100;

    //Set status message and enum
    void SetStatus(const string& message, GuiStatus status = None)
    {
        //Set status enum too if it's not the default argument value
        if (status != None)
            status_ = status;

        customStatusMessage_ = message;
    }
    //Clear status message and set status enum to 'Ready'
    void ClearStatus()
    {
        status_ = Ready;
        customStatusMessage_ = "";
    }
    //Set selected zone and update any cached data about it's objects
    void SetSelectedZone(u32 index)
    {
        //Deselect if selecting already selected zone
        if (index == selectedZone)
        {
            selectedZone = InvalidZoneIndex;
            return;
        }

        //Otherwise select zone and update any data reliant on the selected zone
        selectedZone = index;
        zoneManager_->UpdateObjectClassInstanceCounts(selectedZone);
    }
};