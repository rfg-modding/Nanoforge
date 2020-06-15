#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "common/filesystem/Path.h"
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>
#include <filesystem>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* wndProcAppPtr = nullptr;

Application::Application(HINSTANCE hInstance)
{
    wndProcAppPtr = this;
    hInstance_ = hInstance;
    fontManager_ = new ImGuiFontManager;
    InitRenderer();

    ScanPackfiles();

    //Init frame timing variables
    deltaTime_ = maxFrameRateDelta;
    frameTimer_.Start();
}

Application::~Application()
{
    delete fontManager_;
    delete renderer_;
    wndProcAppPtr = nullptr;

    for (auto& packfile : packfiles_)
        packfile.Cleanup();
}

void Application::Run()
{
    //MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    while(msg.message != WM_QUIT) //Loop while there are messages available
    {
        frameTimer_.Reset();

        //Dispatch messages if present
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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
            UpdateGui();
            renderer_->DoFrame(deltaTime_);
        }

        while (frameTimer_.ElapsedSecondsPrecise() < maxFrameRateDelta)
            deltaTime_ = frameTimer_.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    if(renderer_)
        renderer_->HandleResize();
}

void Application::InitRenderer()
{
    renderer_ = new DX11Renderer(hInstance_, WndProc, windowWidth_, windowHeight_, fontManager_);
}

void Application::UpdateGui()
{
    //Start new imgui frame
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    //Run gui code
    static bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);

    
    ImGui::Begin("File explorer");

    fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_ARCHIVE " Packfiles");
    fontManager_->FontL.Pop();
    ImGui::Separator();
    for (auto& packfile : packfiles_)
    {
        gui::LabelAndValue(packfile.Name() + "   |", std::to_string(packfile.Header.NumberOfSubfiles) + " subfiles");
    }

    ImGui::Separator();
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void Application::ScanPackfiles()
{
    for (auto& filePath : std::filesystem::directory_iterator(packfileFolderPath))
    {
        if (Path::GetExtension(filePath) == ".vpp_pc")
        {
            Packfile3 packfile(filePath.path().string());
            packfile.ReadMetadata();
            packfiles_.push_back(packfile);
        }
    }
}

//Todo: Pass key & mouse messages to InputManager and have it send input messages to other parts of code via callbacks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_KEYDOWN: //Key down
        //If escape key pressed display popup box
        if (wParam == VK_ESCAPE) 
        {
            if (MessageBox(0, "Are you sure you want to exit?",
                "Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)

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
            wndProcAppPtr->Paused = true;
        }
        else
        {
            wndProcAppPtr->Paused = false;
            wndProcAppPtr->frameTimer_.Reset();
        }
        return 0;
    case WM_SIZE:
        wndProcAppPtr->HandleResize();
        return 0;
    //case WM_ENTERSIZEMOVE:
    //    wndProcAppPtr->Paused = true;
    //    return 0;
    //case WM_EXITSIZEMOVE:
    //    wndProcAppPtr->Paused = false;
    //    wndProcAppPtr->HandleResize();
    //    return 0;
    }

    //Return the message for windows to handle it
    return DefWindowProc(hwnd, msg, wParam, lParam);
}