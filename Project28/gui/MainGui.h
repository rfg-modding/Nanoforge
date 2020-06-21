#pragma once
#include "common/Typedefs.h"
#include <ext/WindowsWrapper.h>
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>

class ImGuiFontManager;
class PackfileVFS;
class Camera;

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
    void DrawZonePrimitives();
    void DrawCameraWindow();
    void DrawIm3dPrimitives();
    //Another function pulled from the im3d dx11 example
    Im3d::Vec2 GetWindowRelativeCursor() const;


    bool SystemWindowFocused();

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
    Camera* camera_ = nullptr;
    HWND hwnd_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;

    ZonePc36 zoneFile_;
};