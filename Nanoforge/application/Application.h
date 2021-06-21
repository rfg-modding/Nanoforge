#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "render/camera/Camera.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "rfg/xtbl/XtblManager.h"
#include "rfg/Localization.h"
#include "gui/MainGui.h"
#include "gui/misc/WelcomeGui.h"
#include "project/Project.h"
#include "application/Config.h"
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

private:
    //General init
    void Init();

    //Todo: If more stages are added it'd be a good idea to generalize this or use a state stack to allow more advanced behavior. For now this is good enough.
    //Stage 1 of app. Welcome gui. Makes sure a valid data path has been selected and has the user create or load a project.
    void InitStage1();
    void RunStage1();

    //Stage 2 of app. Main app with all the editors and file viewers.
    void InitStage2();
    void RunStage2();
    
    //General purpose main loop with a lambda argument to inject extra behavior.
    //Done this way since each stage had near identical main loop behavior with slight differences.
    void MainLoop(std::function<void()> stageUpdateFunc);
    void NewFrame();

    void MaximizeWindow();

    //If set to true the main loop of the current stage will exit at the start of next frame
    bool stageDone_ = false;

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer renderer_;
    ImGuiFontManager fontManager_;
    PackfileVFS packfileVFS_;
    XtblManager xtblManager_;
    Project project_;
    Localization localization_;
    Config config_;
    WelcomeGui welcomeGui_;
    MainGui gui_;

    //Size the window is set to at the start of each stage
    const u32 stage1WindowWidth_ = 900;
    const u32 stage1WindowHeight_ = 650;
    const u32 stage2WindowWidth_ = 1200;
    const u32 stage2WindowHeight_ = 720;

    //Actual window size
    int windowWidth_ = 1200;
    int windowHeight_ = 720;

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 targetFramerate = 60;
    const f32 targetFramerateDelta = 1.0f / static_cast<f32>(targetFramerate);

    std::vector<spdlog::sink_ptr> logSinks_ = {};

    //Future for worker thread. Need to store it for std::async to actually run it asynchronously
    std::future<void> workerFuture_;
};