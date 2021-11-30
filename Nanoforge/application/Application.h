#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "render/camera/Camera.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/TextureIndex.h"
#include "rfg/xtbl/XtblManager.h"
#include "rfg/Localization.h"
#include "gui/MainGui.h"
#include "project/Project.h"
#include "util/TaskScheduler.h"
#include <ext/WindowsWrapper.h>
#include <chrono>
#include <spdlog/spdlog.h>
#include <future>

class Application
{
public:
    Application(HINSTANCE hInstance) : hInstance_(hInstance) {}

    void Run();
    void HandleResize();
    void HandleCameraInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool Exit = false;
    bool Paused = false;
    Timer FrameTimer;
    MainGui gui_;

private:
    void Init();
    void MainLoop();
    void NewFrame();

    //These all have global lifetime
    DX11Renderer renderer_;
    ImGuiFontManager fontManager_;
    PackfileVFS packfileVFS_;
    TextureIndex textureSearchIndex_;
    XtblManager xtblManager_;
    Project project_;
    Localization localization_;

    //Window data
    HINSTANCE hInstance_ = nullptr;
    int windowWidth_ = 1200;
    int windowHeight_ = 720;

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    DWORD lastFramelimiterSleepMs_ = 0;
    const u32 targetFramerate = 60;
    const f32 targetFramerateDelta = 1.0f / static_cast<f32>(targetFramerate);

    //Logger sinks
    std::vector<spdlog::sink_ptr> logSinks_ = {};
};