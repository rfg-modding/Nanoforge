#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "gui/MainGui.h"
#include "rfg/PackfileVFS.h"
#include "render/camera/Camera.h"
#include "WorkerThread.h"
#include <tinyxml2/tinyxml2.h>
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
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
    LoadSettings();
    appInstance = this;
    hInstance_ = hInstance;
    packfileVFS_.Init(packfileFolderPath_);
    zoneManager_.Init(&packfileVFS_);
    Camera.Init({ -2573.0f, 2337.0f, 963.0f }, 80.0f, { (f32)windowWidth_, (f32)windowHeight_ }, 1.0f, 10000.0f);
    
    InitRenderer();
    Gui.Init(&fontManager_, &packfileVFS_, &Camera, &zoneManager_);
    Gui.HandleResize();

    //Start worker thread and capture it's future. If future isn't captured it won't actually run async
    static std::future<void> dummy = std::async(std::launch::async, &WorkerThread, &Gui.State);

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
        if (Paused)
        {
            Sleep(100);
        }
        else
        {
            Camera.DoFrame(deltaTime_);
            NewFrame();
            UpdateGui();

            //Check for newly streamed terrain instances that need to be initialized
            if (NewTerrainInstanceAdded)
            {
                //If there are new terrain instances lock their mutex and pass them to the renderer
                std::lock_guard<std::mutex> lock(ResourceLock);
                //The renderer will upload the vertex and index buffers of the new instances to the gpu
                renderer_.InitTerrainMeshes(&TerrainInstances);
            }

            renderer_.DoFrame(deltaTime_);
        }

        while (FrameTimer.ElapsedSecondsPrecise() < maxFrameRateDelta)
            deltaTime_ = FrameTimer.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    renderer_.HandleResize();
    Gui.HandleResize();
}

void Application::InitRenderer()
{
    renderer_.Init(hInstance_, WndProc, windowWidth_, windowHeight_, &fontManager_, &Camera);
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
        if (!dataPath)
            throw std::exception("Error! Failed to get DataPath from Settings.xml");

        packfileFolderPath_ = string(dataPath);
    }
    else //Otherwise recreate it with the default values
    {
        tinyxml2::XMLDocument settings;
        auto* dataPath = settings.NewElement("DataPath");
        settings.InsertFirstChild(dataPath);
        dataPath->SetText(packfileFolderPath_.c_str());
        settings.SaveFile("./Settings.xml");
    }
}

//Todo: Pass key & mouse messages to InputManager and have it send input messages to other parts of code via callbacks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    appInstance->Camera.HandleInput(hwnd, msg, wParam, lParam);

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