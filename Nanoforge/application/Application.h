#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "render/camera/Camera.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/xtbl/XtblManager.h"
#include "gui/MainGui.h"
#include "project/Project.h"
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

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer renderer_;
    ImGuiFontManager fontManager_;
    PackfileVFS packfileVFS_;
    XtblManager xtblManager_;
    Project project_;

    int windowWidth_ = 1600;
    int windowHeight_ = 1000;

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 maxFrameRate = 60;
    const f32 maxFrameRateDelta = 1.0f / static_cast<f32>(maxFrameRate);

    std::vector<spdlog::sink_ptr> logSinks_ = {};

    //Future for worker thread. Need to store it for std::async to actually run it asynchronously
    std::future<void> workerFuture_;
};