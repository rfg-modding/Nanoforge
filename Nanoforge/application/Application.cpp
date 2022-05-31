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
#include "render/resources/Scene.h"
#include "application/Config.h"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <imgui.h>
#include <imgui/backends/imgui_impl_win32.h>
#include <imgui/backends/imgui_impl_dx11.h>
#include <filesystem>
#include <future>
#include <variant>
#include <ext/WindowsWrapper.h>
#include <timeapi.h>
#include "render/imgui/imgui_ext.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <windowsx.h>

//Callback that handles windows messages such as keypresses
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
//Pointer to application instance for WndProc
Application* appInstance = nullptr;

//Cursors used depending on mouse position (in window, at corners/sides of window, etc)
HCURSOR CursorArrow = nullptr;
HCURSOR CursorSizeWE = nullptr;
HCURSOR CursorSizeNS = nullptr;
HCURSOR CursorSizeNWSE = nullptr;
HCURSOR CursorSizeNESW = nullptr;

void Application::Run()
{
    //Init app and run main loop
    appInstance = this;
    Init();
    MainLoop();
    delete Log;

    //Main loop finished. Cleanup resources
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
    Log = new spdlog::logger("MainLogger", begin(logSinks_), end(logSinks_));
    Log->flush_on(spdlog::level::level_enum::info); //Always flush
    Log->set_pattern("[%Y-%m-%d, %H:%M:%S][%^%l%$]: %v");
    TRACE();

    //Load settings.xml
    Config* config = Config::Get();
    config->Load();
    CVar& dataPath = config->GetCvar("Data path");

    //Init task scheduler. Run code (tasks) on background threads
    TaskScheduler::Init();

    //Init core application classes
    renderer_.Init(hInstance_, WndProc, windowWidth_, windowHeight_, &fontManager_);
    WinUtilInit(renderer_.GetSystemWindowHandle());
    gui::SetThemePreset(ThemePreset::Dark);
    packfileVFS_.Init(dataPath.Get<string>(), &project_);
    textureSearchIndex_.Init(&packfileVFS_);
    xtblManager_.Init(&packfileVFS_);
    localization_.Init(&packfileVFS_);
    gui_.Init(&fontManager_, &packfileVFS_, &renderer_, &project_, &xtblManager_, &localization_, &textureSearchIndex_);

    //Preload cursors for custom titlebar logic in wndproc
    CursorArrow = LoadCursor(nullptr, IDC_ARROW);
    CursorSizeWE = LoadCursor(nullptr, IDC_SIZEWE);
    CursorSizeNS = LoadCursor(nullptr, IDC_SIZENS);
    CursorSizeNWSE = LoadCursor(nullptr, IDC_SIZENWSE);
    CursorSizeNESW = LoadCursor(nullptr, IDC_SIZENESW);

    //Maximize window by default
    ShowWindow(renderer_.GetSystemWindowHandle(), SW_SHOWMAXIMIZED);

    //Init frame timing variables
    deltaTime_ = targetFramerateDelta;
}

void Application::MainLoop()
{
    PROFILER_FUNCTION();
    TRACE();
    MSG msg; //Create a new message structure
    ZeroMemory(&msg, sizeof(MSG)); //Clear message structure to NULL

    //Loop until exit is requested
    FrameTimer.Start();
    while (!Exit && !gui_.Shutdown)
    {
        static const char* MainFrame = "MainFrame";
        PROFILER_FRAME_MARK_START(MainFrame);

        //Handle window messages
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
        gui_.Update();

        //Render this frame
        renderer_.DoFrame(deltaTime_);
        PROFILER_FRAME_MARK_END(MainFrame);

        //Sleep until target framerate is reached
        {
            PROFILER_SCOPED("Wait for target framerate");
            //Set resolution of OS timers to 1 ms to get reasonably accurate Sleep() calls
            //This isn't always needed. It was added when Sleep() started taking 15ms more than it should have after a windows update.
            timeBeginPeriod(1);
            while (FrameTimer.ElapsedSecondsPrecise() < targetFramerateDelta)
            {
                lastFramelimiterSleepMs_ = (DWORD)((targetFramerateDelta - FrameTimer.ElapsedSecondsPrecise()) * 1000.0f);
                if (lastFramelimiterSleepMs_ > (DWORD)(targetFramerateDelta * 1000.0f))
                    break;

                //Sleep is used instead of busy waiting to minimize cpu usage. Exact target FPS isn't needed for this.
                Sleep(lastFramelimiterSleepMs_);
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
    //Reset per frame data
    std::lock_guard<std::mutex> lock(renderer_.ContextMutex);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
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
    static bool registryExplorerVisible = false;
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
    case WM_KEYUP:
        if (wParam == VK_F2)
        {
            registryExplorerVisible = !registryExplorerVisible;
            appInstance->gui_.SetPanelVisibility("Registry explorer", registryExplorerVisible);
        }
    case WM_DESTROY: //Exit if window close button pressed
        PostQuitMessage(0);
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE)
        {
            appInstance->MainWindowFocused = true;
            appInstance->gui_.State.MainWindowFocused = true;
        }
        else
        {
            appInstance->MainWindowFocused = false;
            appInstance->gui_.State.MainWindowFocused = false;
            appInstance->FrameTimer.Reset();
        }
        return 0;
    case WM_SIZE:
        appInstance->HandleResize();
        return 0;
    case WM_NCPAINT:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_NCCALCSIZE: //Calculate client area size
        {
            RECT& clientAreaRect = *reinterpret_cast<RECT*>(lParam);
            RECT originalClientArea = clientAreaRect;
            DefWindowProc(hwnd, msg, wParam, lParam);

            if (IsZoomed(hwnd))
            {
                //Get window size/shape
                WINDOWINFO windowInfo = {};
                windowInfo.cbSize = sizeof(WINDOWINFO);
                GetWindowInfo(hwnd, &windowInfo);

                //Calculate size of new client area
                clientAreaRect.left = originalClientArea.left + windowInfo.cyWindowBorders;
                clientAreaRect.top = originalClientArea.top + windowInfo.cyWindowBorders;
                clientAreaRect.right = originalClientArea.right - windowInfo.cyWindowBorders;
                clientAreaRect.bottom = originalClientArea.bottom - windowInfo.cyWindowBorders + 1;
            }
            else
            {
                clientAreaRect = originalClientArea;
            }

            return 0;
        }
    case WM_NCHITTEST: //Test if the mouse is colliding with any regions of the window
        {
            POINT mousePos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const POINT border =
            {
                (GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)),
                (GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER))
            };
            RECT window;
            if (!GetWindowRect(hwnd, &window))
                return HTNOWHERE;

            constexpr i32 regionClient = 0b0000;
            constexpr i32 regionLeft = 0b0001;
            constexpr i32 regionRight = 0b0010;
            constexpr i32 regionTop = 0b0100;
            constexpr i32 regionBottom = 0b1000;
            const i32 result =
                regionLeft * (mousePos.x < (window.left + border.x)) |
                regionRight * (mousePos.x >= (window.right - border.x)) |
                regionTop * (mousePos.y < (window.top + border.y)) |
                regionBottom * (mousePos.y >= (window.bottom - border.y));

            switch (result)
            {
            case regionLeft:
                return HTLEFT;
            case regionRight:
                return HTRIGHT;
            case regionTop:
                return HTTOP;
            case regionBottom:
                return HTBOTTOM;
            case regionTop | regionLeft:
                return HTTOPLEFT;
            case regionTop | regionRight:
                return HTTOPRIGHT;
            case regionBottom | regionLeft:
                return HTBOTTOMLEFT;
            case regionBottom | regionRight:
                return HTBOTTOMRIGHT;
            case regionClient:
            default:
                if ((mousePos.y < (window.top + appInstance->gui_.mainMenuHeight * 2)) && !(ImGui::IsAnyItemHovered() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)))
                    return HTCAPTION;
                else
                    break;
            }
            break;
        }
    case WM_SETCURSOR: //Update cursor based on hit test result
        {
            WORD hitPos = LOWORD(lParam);
            switch (hitPos)
            {
            case HTCAPTION:
            case HTCLIENT:
                SetCursor(CursorArrow);
                break;
            case HTRIGHT:
            case HTLEFT:
                SetCursor(CursorSizeWE);
                break;
            case HTTOP:
            case HTBOTTOM:
                SetCursor(CursorSizeNS);
                break;
            case HTTOPLEFT:
            case HTBOTTOMRIGHT:
                SetCursor(CursorSizeNWSE);
                break;
            case HTTOPRIGHT:
            case HTBOTTOMLEFT:
                SetCursor(CursorSizeNESW);
                break;
            default:
                break;
            }
            return TRUE;
        }
    }

    //Return the message for windows to handle it
    return DefWindowProc(hwnd, msg, wParam, lParam);
}