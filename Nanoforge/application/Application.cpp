#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include "WorkerThread.h"
#include "Log.h"
#include "common/string/String.h"
#include "application/Settings.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <filesystem>
#include <future>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* appInstance = nullptr;

Application::Application(HINSTANCE hInstance)
{
    //Init logger
    logSinks_.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    logSinks_.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("MasterLog.log"));
    logSinks_.push_back(std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(120));
    Log = std::make_shared<spdlog::logger>("MainLogger", begin(logSinks_), end(logSinks_));
    Log->flush_on(spdlog::level::level_enum::info); //Always flush
    Log->set_pattern("[%Y-%m-%d, %H:%M:%S][%^%l%$]: %v");

    //Load settings.xml
    Settings_Read();

    appInstance = this;
    hInstance_ = hInstance;
    packfileVFS_.Init(Settings_PackfileFolderPath, &project_);
    
    InitRenderer();
    //Setup gui
    Gui.Init(&fontManager_, &packfileVFS_, &renderer_, &project_);
    //Set initial territory name
    Gui.State.SetTerritory(Settings_TerritoryFilename, true);
    Gui.HandleResize(windowWidth_, windowHeight_);

    //Start worker thread and capture it's future. If future isn't captured it won't actually run async
    workerFuture_ = std::async(std::launch::async, &WorkerThread, &Gui.State);

    //Init frame timing variables
    deltaTime_ = maxFrameRateDelta;
    FrameTimer.Start();
}

Application::~Application()
{
    appInstance = nullptr;
}

void Application::Run()
{
    MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    while(msg.message != WM_QUIT) //Loop while there are messages available
    {
        FrameTimer.Reset();

        //Dispatch messages if present
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        //Update cameras
        for (auto& scene : renderer_.Scenes)
            scene->Cam.DoFrame(deltaTime_);

        NewFrame();
        UpdateGui();
        renderer_.DoFrame(deltaTime_);

        while (FrameTimer.ElapsedSecondsPrecise() < maxFrameRateDelta)
            deltaTime_ = FrameTimer.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    renderer_.HandleResize();
    windowWidth_ = renderer_.WindowWidth();
    windowHeight_ = renderer_.WindowHeight();
    Gui.HandleResize(windowWidth_, windowHeight_);
}

void Application::HandleCameraInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    for (auto& scene : renderer_.Scenes)
        scene->Cam.HandleInput(hwnd, msg, wParam, lParam);
}

void Application::InitRenderer()
{
    renderer_.Init(hInstance_, WndProc, windowWidth_, windowHeight_, &fontManager_);
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

void Application::UpdateGui()
{
    Gui.Update(deltaTime_);
}

//Todo: Pass key & mouse messages to InputManager and have it send input messages to other parts of code via callbacks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    appInstance->HandleCameraInput(hwnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_KEYDOWN: //Key down
        //If escape key pressed display popup box
        if (wParam == VK_ESCAPE) 
        {
            if (MessageBox(0, "Are you sure you want to exit?",
                "Confirm exit", MB_YESNO | MB_ICONQUESTION) == IDYES)

                //Release the windows allocated memory  
                DestroyWindow(hwnd);
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