#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include "WorkerThread.h"
#include "Log.h"
#include "gui/util/WinUtil.h"
#include "common/string/String.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <filesystem>
#include <future>
#include <variant>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* appInstance = nullptr;

void Application::Run()
{
    //Init general app state used by all stages
    appInstance = this;
    Init();

    //Stage 1 - Welcome gui
    InitStage1();
    RunStage1();

    //Return early if exit was requested
    if (Exit)
        return;

    //Stage 2 - Main app
    InitStage2();
    RunStage2();
}

void Application::Init()
{
    //Init logger
    logSinks_.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks_.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("MasterLog.log"));
    logSinks_.push_back(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(120));
    Log = std::make_shared<spdlog::logger>("MainLogger", begin(logSinks_), end(logSinks_));
    Log->flush_on(spdlog::level::level_enum::info); //Always flush
    Log->set_pattern("[%Y-%m-%d, %H:%M:%S][%^%l%$]: %v");

    //Load settings.xml
    config_.Load();

    //Init renderer and gui
    fontManager_.Init(&config_);
    renderer_.Init(hInstance_, WndProc, stage1WindowWidth_, stage1WindowHeight_, &fontManager_, &config_);
    gui::SetThemePreset(ThemePreset::Dark);

    //Init utility classes for file/folder browsers
    WinUtilInit(renderer_.GetSystemWindowHandle());

    //Init frame timing variables
    deltaTime_ = targetFramerateDelta;
}

void Application::InitStage1()
{
    welcomeGui_.Init(&config_, &fontManager_, &project_, windowWidth_, windowHeight_);
}

void Application::RunStage1()
{
    //Per frame update
    std::function<void()> update = [&]()
    {
        welcomeGui_.Update();

        //Move to the next stage if the welcome gui is done
        if (welcomeGui_.Done)
            stageDone_ = true;
    };

    //Run main loop with custom update behavior for this stage injected
    MainLoop(update);
}

void Application::InitStage2()
{
    //Maximize window since the gui has a lot more content in stage 2
    MaximizeWindow();

    //Stage 1 ensured that we have a valid data path so we can now initialize code that depends on it
    auto dataPath = config_.GetVariable("Data path");
    packfileVFS_.Init(std::get<string>(dataPath->Value), &project_);
    xtblManager_.Init(&packfileVFS_);
    localization_.Init(&packfileVFS_, &config_);

    //Setup main gui
    gui_.Init(&fontManager_, &packfileVFS_, &renderer_, &project_, &xtblManager_, &config_, &localization_);
    gui_.HandleResize(windowWidth_, windowHeight_);

    //Start worker thread to load packfile metadata in the background
    workerFuture_ = std::async(std::launch::async, &WorkerThread, &gui_.State);
}

void Application::RunStage2()
{
    //Per frame update
    std::function<void()> update = [&]()
    {
        //Update cameras
        for (auto& scene : renderer_.Scenes)
            scene->Cam.DoFrame(deltaTime_);

        //Update main gui
        gui_.Update(deltaTime_);
    };

    //Run main loop with custom update behavior for this stage injected
    MainLoop(update);
}

void Application::MainLoop(std::function<void()> stageUpdateFunc)
{
    stageDone_ = false;
    MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    FrameTimer.Start();
    while (!stageDone_ && !Exit) //Loop until stage finishes or app exit variable is set to true
    {
        //Dispatch messages if present
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        NewFrame();
        stageUpdateFunc();
        renderer_.DoFrame(deltaTime_);

        //Sleep until target framerate is reached
        while (FrameTimer.ElapsedSecondsPrecise() < targetFramerateDelta)
        {
            //Sleep is used here instead of busy waiting to minimize cpu usage. Exact target FPS isn't needed for this. 
            f32 timeToTargetFramerateMs = (targetFramerateDelta - FrameTimer.ElapsedSecondsPrecise()) * 1000.0f;
            Sleep((DWORD)timeToTargetFramerateMs);
        }
        deltaTime_ = FrameTimer.ElapsedSecondsPrecise();
        FrameTimer.Reset();
    }
}

void Application::NewFrame()
{
    std::lock_guard<std::mutex> lock(renderer_.ContextMutex);

    //Start new imgui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    renderer_.NewFrame(deltaTime_);
}

void Application::MaximizeWindow()
{
    ShowWindow(renderer_.GetSystemWindowHandle(), SW_SHOWMAXIMIZED);
}

void Application::HandleResize()
{
    renderer_.HandleResize();
    windowWidth_ = renderer_.WindowWidth();
    windowHeight_ = renderer_.WindowHeight();
    welcomeGui_.HandleResize(windowWidth_, windowHeight_);
    gui_.HandleResize(windowWidth_, windowHeight_);
}

void Application::HandleCameraInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    for (auto& scene : renderer_.Scenes)
        scene->Cam.HandleInput(hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    appInstance->HandleCameraInput(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_CLOSE:
    case WM_QUIT:
        appInstance->Exit = true;
        break;
    case WM_KEYDOWN: //Key down
        //If escape key pressed display popup box
        if (wParam == VK_ESCAPE) 
        {
            if (MessageBox(0, "Are you sure you want to exit?",
                "Confirm exit", MB_YESNO | MB_ICONQUESTION) == IDYES)

            appInstance->Exit = true;
        }
        return 0;
    case WM_DESTROY: //Exit if window close button pressed
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            appInstance->Paused = true;
        }
        else
        {
            appInstance->Paused = false;
            appInstance->FrameTimer.Reset();
        }
        return 0;
    case WM_SIZE:
        appInstance->HandleResize();
        return 0;
    }

    //Return the message for windows to handle it
    return DefWindowProc(hwnd, msg, wParam, lParam);
}