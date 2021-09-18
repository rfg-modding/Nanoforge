#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include "Log.h"
#include "gui/util/WinUtil.h"
#include "util/Profiler.h"
#include "common/string/String.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <filesystem>
#include <future>
#include <variant>
#include <ext/WindowsWrapper.h>
#include <timeapi.h>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* appInstance = nullptr;

void Application::Run()
{
    appInstance = this;
    Init();

    //Return early if exit was requested
    if (Exit)
        return;

    MainLoop();

    TaskScheduler::Shutdown();
}

void Application::Init()
{
    PROFILER_SET_THREAD_NAME("Main thread");
    PROFILER_FUNCTION();

    //Init logger
    printf("[info] Initializing logger...\n");
    logSinks_.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks_.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("MasterLog.log"));
    logSinks_.push_back(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(120));
    Log = std::make_shared<spdlog::logger>("MainLogger", begin(logSinks_), end(logSinks_));
    Log->flush_on(spdlog::level::level_enum::info); //Always flush
    Log->set_pattern("[%Y-%m-%d, %H:%M:%S][%^%l%$]: %v");
    TRACE();

    //Load settings.xml
    config_.Load();
    config_.EnsureVariableExists("Data path", ConfigType::String);
    auto dataPath = config_.GetVariable("Data path");

    //Init TaskScheduler. Used to manage background threads
    TaskScheduler::Init(&config_);

    //Init renderer and gui
    fontManager_.Init(&config_);
    renderer_.Init(hInstance_, WndProc, windowWidth_, windowHeight_, &fontManager_, &config_);
    WinUtilInit(renderer_.GetSystemWindowHandle());
    gui::SetThemePreset(ThemePreset::Dark);

    packfileVFS_.Init(std::get<string>(dataPath->Value), &project_);
    xtblManager_.Init(&packfileVFS_);
    localization_.Init(&packfileVFS_, &config_);

    gui_.Init(&fontManager_, &packfileVFS_, &renderer_, &project_, &xtblManager_, &config_, &localization_);

    MaximizeWindow();

    //Init frame timing variables
    deltaTime_ = targetFramerateDelta;
}

void Application::MainLoop()
{
    PROFILER_FUNCTION();
    TRACE();
    MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    FrameTimer.Start();
    while (!Exit) //Loop until stage finishes or app exit variable is set to true
    {
        static const char* MainFrame = "MainFrame";
        PROFILER_FRAME_MARK_START(MainFrame);

        //Dispatch window messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        NewFrame();

        //Update cameras and UI
        {
            PROFILER_SCOPED("Render scenes");
            for (auto& scene : renderer_.Scenes)
            {
                scene->Cam.DoFrame(deltaTime_);
            }
        }

        gui_.Update(deltaTime_);

        //Render this frame
        renderer_.DoFrame(deltaTime_);
        PROFILER_FRAME_MARK_END(MainFrame);

        //Sleep until target framerate is reached
        {
            PROFILER_SCOPED("Wait for target framerate");
            //Set resolution of OS timers to 1 ms to get reasonably accurate Sleep() calls
            //For some reason this isn't always needed. It was added when Sleep() started taking 15ms more than it should have after a windows update.
            timeBeginPeriod(1);
            while (FrameTimer.ElapsedSecondsPrecise() < targetFramerateDelta)
            {
                //Sleep is used here instead of busy waiting to minimize cpu usage. Exact target FPS isn't needed for this.
                lastFramelimiterSleepMs_ = (DWORD)((targetFramerateDelta - FrameTimer.ElapsedSecondsPrecise()) * 1000.0f);
                if (lastFramelimiterSleepMs_ > (DWORD)(targetFramerateDelta * 1000.0f))
                    break;

                Sleep((DWORD)lastFramelimiterSleepMs_);
            }
            timeEndPeriod(1); //Disable custom system timer resolution
        }

        deltaTime_ = FrameTimer.ElapsedSecondsPrecise();
        FrameTimer.Reset();
        PROFILER_FRAME_MARK();
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
        //Ctrl + S - Save focused document
        if (wParam == 0x53 && GetKeyState(VK_CONTROL) & 0x8000)
        {
            appInstance->gui_.SaveFocusedDocument();
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