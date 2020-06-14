#include "Application.h"
#include "render/backend/DX11Renderer.h"

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Ptr to application instance for WndProc use
Application* wndProcAppPtr = nullptr;

Application::Application(HINSTANCE hInstance)
{
    hInstance_ = hInstance;
    InitRenderer();
    wndProcAppPtr = this;
}

Application::~Application()
{
    delete renderer_;
    wndProcAppPtr = nullptr;
}

void Application::Run()
{
    MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    while(true) //Loop while there are messages available
    {
        //Dispatch message if present
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            //Exit on WM_QUIT message
            if (msg.message == WM_QUIT)
                break;

            //Translate messages and dispatch to WinProc
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else //Else render frame
        {
            renderer_->DoFrame();
        }
    }
}

void Application::InitRenderer()
{
    renderer_ = new DX11Renderer(hInstance_, WndProc, windowWidth_, windowHeight_);

}

//Todo: Pass key & mouse messages to InputManager and have it send input messages to other parts of code via callbacks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
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
    }

    //Return the message for windows to handle it
    return DefWindowProc(hwnd, msg, wParam, lParam);
}