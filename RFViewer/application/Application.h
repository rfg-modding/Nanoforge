#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "render/camera/Camera.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/ZoneManager.h"
#include "gui/MainGui.h"
#include <ext/WindowsWrapper.h>
#include <chrono>
#include <spdlog/spdlog.h>
#include <future>

class Application
{
public:
    Application(HINSTANCE hInstance);
    ~Application();

    void Run();
    void HandleResize();
    void HandleCameraInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool Paused = false;
    Timer FrameTimer;
    MainGui Gui;

private:
    void InitRenderer();
    void NewFrame();
    void UpdateGui();
    void LoadSettings();
    //Reload map with new territory
    void Reload();

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer renderer_;
    ImGuiFontManager fontManager_;
    PackfileVFS packfileVFS_;
    ZoneManager zoneManager_;

    int windowWidth_ = 1600;
    int windowHeight_ = 1000;

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 maxFrameRate = 60;
    const f32 maxFrameRateDelta = 1.0f / static_cast<f32>(maxFrameRate);

    string packfileFolderPath_ = "C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered";
    //Name of the vpp_pc file that zone data is loaded from at startup
    string territoryFilename_ = "zonescript_terr01.vpp_pc";

    std::vector<spdlog::sink_ptr> logSinks_ = {};

    //Future for worker thread. Need to store it for std::async to actually run it asynchronously
    std::future<void> workerFuture_;
};