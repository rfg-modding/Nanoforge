#include "Application.h"
#include "render/backend/DX11Renderer.h"
#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_win32.h>
#include <imgui/examples/imgui_impl_dx11.h>

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
    InitRenderer();

    //Init frame timing variables
    deltaTime_ = maxFrameRateDelta;
    frameTimer_.Start();
}

Application::~Application()
{
    delete renderer_;
    wndProcAppPtr = nullptr;
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
        else if (Paused)
        {
            Sleep(100);
        }
        else
        {
            UpdateGui();
            renderer_->DoFrame(deltaTime_);
        }

        //while (frameTimer_.ElapsedSecondsPrecise() < maxFrameRateDelta)
        //    deltaTime_ = frameTimer_.ElapsedSecondsPrecise();
    }
}

void Application::HandleResize()
{
    if(renderer_)
        renderer_->HandleResize();
}

void Application::InitRenderer()
{
    renderer_ = new DX11Renderer(hInstance_, WndProc, windowWidth_, windowHeight_);
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

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        static float f = 0.0f;
        static int counter = 0;

        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
        ImGui::Checkbox("Another Window", &show_demo_window);

        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        //ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

        if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
            counter++;
        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);

        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

    // 3. Show another simple window.
    if (show_demo_window)
    {
        ImGui::Begin("Another Window", &show_demo_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
        ImGui::Text("Hello from another window!");
        if (ImGui::Button("Close Me"))
            show_demo_window = false;
        ImGui::End();
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