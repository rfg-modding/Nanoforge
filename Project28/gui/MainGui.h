#pragma once
#include "common/Typedefs.h"
#include <ext/WindowsWrapper.h>
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <vector>

class ImGuiFontManager;
class PackfileVFS;
class Camera;

struct ZoneFile
{
    string Name;
    ZonePc36 Zone;
    bool RenderBoundingBoxes = false;
    bool Selected = false;
};

constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    MainGui(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, HWND hwnd);

    void Update(f32 deltaTime);
    void HandleResize();

    bool Visible = true;

private:
    void DrawMainMenuBar();
    void DrawDockspace();
    void DrawFileExplorer();
    void DrawZoneWindow();
    void DrawZoneObjectsWindow();
    void DrawZonePrimitives();
    void DrawCameraWindow();
    void DrawIm3dPrimitives();

    //Set selected zone and update any cached data about it's objects
    void SetSelectedZone(u32 index);

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
    Camera* camera_ = nullptr;
    HWND hwnd_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;

    std::vector<ZoneFile> zoneFiles_;
    Im3d::Vec4 boundingBoxColor_ = Im3d::Vec4(0.778f, 0.414f, 0.0f, 1.0f);
    f32 boundingBoxThickness_ = 1.0f;
    
    Im3d::Vec3 gridOrigin_ = { 0.0f, 0.0f, 0.0f };
    bool gridFollowCamera_ = true;
    bool drawGrid_ = false;
    int gridSpacing_ = 10;
    int gridSize_ = 100;
    u32 selectedZone = InvalidZoneIndex;
};