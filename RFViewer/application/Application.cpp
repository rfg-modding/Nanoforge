#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include "WorkerThread.h"
#include "Log.h"
#include "common/string/String.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <tinyxml2/tinyxml2.h>
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
    LoadSettings();

    appInstance = this;
    hInstance_ = hInstance;
    packfileVFS_.Init(packfileFolderPath_);
    //Camera.Init({ -2573.0f, 200.0f, 963.0f }, 80.0f, { (f32)windowWidth_, (f32)windowHeight_ }, 1.0f, 10000.0f);
    
    InitRenderer();
    //Setup gui
    Gui.Init(&fontManager_, &packfileVFS_, &zoneManager_, &renderer_);
    //Set initial territory name
    Gui.State.SetTerritory(territoryFilename_, true);
    zoneManager_.Init(&packfileVFS_, Gui.State.CurrentTerritoryName, Gui.State.CurrentTerritoryShortname);
    Gui.HandleResize();

    //Start worker thread and capture it's future. If future isn't captured it won't actually run async
    workerFuture_ = std::async(std::launch::async, &WorkerThread, &Gui.State, false);

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

        //Reload territory if necessary
        if (Gui.State.ReloadNeeded)
            Reload();

        //Update cameras
        for (auto& scene : renderer_.Scenes)
            scene.Cam.DoFrame(deltaTime_);

        NewFrame();
        UpdateGui();

        //Check for newly streamed terrain instances that need to be initialized
        if (NewTerrainInstanceAdded)
        {
            //If there are new terrain instances lock their mutex and pass them to the renderer
            std::lock_guard<std::mutex> lock(ResourceLock);
            NewTerrainInstanceAdded = false;
            //The renderer will upload the vertex and index buffers of the new instances to the gpu
            //Todo: Update to support new scene system
            //renderer_.InitTerrainMeshes(&TerrainInstances);

            //If the worker is done clear it's temporary data
            if (WorkerDone)
                WorkerThread_ClearData();
        }

        renderer_.DoFrame(deltaTime_);

        while (FrameTimer.ElapsedSecondsPrecise() < maxFrameRateDelta)
            deltaTime_ = FrameTimer.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    renderer_.HandleResize();
    Gui.HandleResize();
}

void Application::HandleCameraInput(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    for (auto& scene : renderer_.Scenes)
        scene.Cam.HandleInput(hwnd, msg, wParam, lParam);
}

void Application::InitRenderer()
{
    renderer_.Init(hInstance_, WndProc, windowWidth_, windowHeight_, &fontManager_);
}

void Application::NewFrame()
{
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

void Application::LoadSettings()
{
    namespace fs = std::filesystem;
    //Load settings file if it exists
    if (fs::exists("./Settings.xml"))
    {
        tinyxml2::XMLDocument settings;
        settings.LoadFile("./Settings.xml");
        const char* dataPath = settings.FirstChildElement("DataPath")->GetText();
        if (!dataPath) //Todo: Make this more fault tolerant. Use default values where possible
            THROW_EXCEPTION("Failed to get <DataPath> from Settings.xml");
        
        const char* territoryFile = settings.FirstChildElement("TerritoryFile")->GetText();
        if (!territoryFile)
            THROW_EXCEPTION("Failed to get <TerritoryFile> from Settings.xml");

        packfileFolderPath_ = string(dataPath);
        territoryFilename_ = string(territoryFile);

        //Temporary compatibility patches for convenience. Previous versions expected vpp_pc files instead of shorthand names
        if (territoryFilename_ == "zonescript_terr01.vpp_pc")
            territoryFilename_ = "terr01";
        if (territoryFilename_ == "zonescript_dlc01.vpp_pc")
            territoryFilename_ = "dlc01";
        if (String::EndsWith(territoryFilename_, ".vpp_pc"))
        {
            size_t postfixIndex = territoryFilename_.find(".vpp_pc");
            territoryFilename_ = territoryFilename_.substr(0, postfixIndex);
        }
    }
    else //Otherwise recreate it with the default values
    {
        tinyxml2::XMLDocument settings;

        auto* dataPath = settings.NewElement("DataPath");
        settings.InsertFirstChild(dataPath);
        dataPath->SetText(packfileFolderPath_.c_str());
        
        auto* territoryFile = settings.NewElement("TerritoryFile");
        settings.InsertFirstChild(territoryFile);
        dataPath->SetText(territoryFilename_.c_str());

        settings.SaveFile("./Settings.xml");
    }
}

void Application::Reload()
{
    //Wait for worker thread to exit
    Log->info("Reload triggered.");
    workerFuture_.wait();
    Log->info("Clearing old territory data.");

    //Clear old territory data
    //Todo: Update to support new scene system
    //renderer_.ResetTerritoryData();
    zoneManager_.ResetTerritoryData();
    zoneManager_.Init(&packfileVFS_, Gui.State.CurrentTerritoryName, Gui.State.CurrentTerritoryShortname);
    WorkerThread_ClearData();

    //Start worker thread and capture it's future. If future isn't captured it won't actually run async
    workerFuture_ = std::async(std::launch::async, &WorkerThread, &Gui.State, true);
    Log->info("Restarted worker thread.");
    Gui.State.ReloadNeeded = false;
    Gui.State.SetSelectedZoneObject(nullptr);
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
        if (wParam == VK_F1)
        {
            CanStartInit = true;
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