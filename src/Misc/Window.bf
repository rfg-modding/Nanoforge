using Nanoforge.Render;
using Nanoforge.Math;
using Nanoforge.App;
using System.Text;
using Nanoforge;
using System;
using Win32;

namespace Nanoforge.Misc
{
    //Wrapper for a GLFW window
    [System]
    public class Window : ISystem
    {
        public i32 Width { get; private set; }
        public i32 Height { get; private set; }
        public Vec2<i32> Size => .(Width, Height);
        public HWND Handle { get; private set; } = 0;
        public bool Focused { get; private set; } = true;
        public bool ShouldClose { get; private set; } = false;
        //Use to add other functions to pass window messages to
        public Event<delegate LRESULT(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)> WndProcExtensions ~ _.Dispose();
        private Input _input = null;

        ///The actual WndProc logic is in a member function called by this delegate. That way it has access to the window fields
        private static delegate LRESULT(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam) WndProcDelegate ~ delete _;

        static void ISystem.Build(App app)
        {
        }

        [SystemInit]
        void Init(App app)
        {
            Width = 1080;
            Height = 720;
            WndProcDelegate = new => InstanceWNDPROC;

            BuildConfig config = app.GetResource<BuildConfig>();

            static char16[?] windowClassName = L("Nanoforge");
            static char16[?] windowName = L("Nanoforge");
            int hInstance = (int)System.Windows.GetModuleHandleW(null); //0;

            //Define window class
            WNDCLASSEXW wc = .();
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = .HREDRAW | .VREDRAW;
            wc.lpfnWndProc = => WNDPROC;
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hInstance = hInstance;
            wc.hIcon = Win32.LoadIconW(0, Win32.IDI_WINLOGO);
            wc.hCursor = Win32.LoadCursorW(0, Win32.IDC_SIZEWE);
            wc.hbrBackground = (int)SYS_COLOR_INDEX.COLOR_MENUTEXT;
            wc.lpszMenuName = null;
            wc.lpszClassName = &windowClassName;
            wc.hIconSm = Win32.LoadIconW(0, Win32.IDI_WINLOGO);
            if (Win32.RegisterClassExW(wc) == 0)
                Win32.FatalError!("RegisterClassExW() failed while creating window");

            //Create window
            Handle = Win32.CreateWindowExW(.ACCEPTFILES, &windowClassName, &windowName, .OVERLAPPEDWINDOW, Win32.CW_USEDEFAULT, Win32.CW_USEDEFAULT, Width, Height, 0, 0, hInstance, null);
            if (Handle == 0)
                Win32.FatalError!("CreateWindowExW() failed while creating window");

            //Show window
            Win32.ShowWindow(Handle, .SHOW);
        }

        //Process window events
        [SystemStage(.BeginFrame)]
        void BeginFrame(App app)
        {
            if (_input == null) //Get reference to input on first call so WndProc can pass any input events to it
                _input = app.GetResource<Input>();

            MSG msg = .();
            ZeroMemory(&msg);
            while (Win32.PeekMessageW(out msg, Handle, 0, 0, .REMOVE) != 0)
            {
                Win32.TranslateMessage(msg);
                Win32.DispatchMessageW(msg);
            }

            if (ShouldClose)
                app.Exit = true;
        }

        LRESULT InstanceWNDPROC(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
        {
            for (var dlg in WndProcExtensions)
                if (dlg(hwnd, msg, wParam, lParam) != 0)
                    return 1;

            switch (msg)
            {
                case Win32.WM_CLOSE, Win32.WM_QUIT, Win32.WM_DESTROY:
                    ShouldClose = true;
                    return 0;
                case Win32.WM_SIZE:
                    i32 width = Win32.LOWORD!(lParam);
                    i32 height = Win32.HIWORD!(lParam);
                    Events.Send<WindowResizeEvent>(.(width, height, (WindowResize)wParam));
                    return 0;
                case Win32.WM_ACTIVATE:
                    u32 messageType = Win32.LOWORD!(wParam);
                    Focused = (messageType == Win32.WA_ACTIVE || messageType == Win32.WA_CLICKACTIVE);
                    return 0;
                case Win32.WM_KEYDOWN:
                    if (_input != null)
                        _input.[Friend]WndProcKeyDown(wParam, lParam);

                    return 0;
                case Win32.WM_KEYUP:
                    if (_input != null)
                        _input.[Friend]WndProcKeyUp(wParam, lParam);

                    return 0;
                case Win32.WM_LBUTTONDOWN, Win32.WM_LBUTTONUP, Win32.WM_MBUTTONDOWN, Win32.WM_MBUTTONUP, Win32.WM_RBUTTONUP, Win32.WM_RBUTTONDOWN, Win32.WM_MOUSEMOVE, Win32.WM_MOUSEWHEEL:
                    if (_input != null)
                        _input.[Friend]WndProcMouse(msg, wParam, lParam);
                    //TODO: Handle double click and XButton messages
                default:
            }

            return Win32.DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        static LRESULT WNDPROC(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
        {
            return WndProcDelegate(hwnd, msg, wParam, lParam);
        }

        public void Maximize()
        {
            Win32.ShowWindow(Handle, .MAXIMIZE);
        }    
    }

    //Matches wParam of WndProc WM_SIZE message: https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-size
    public enum WindowResize
    {
        Restored = 0,
        Minimized = 1,
        Maximized = 2,
        MaxShow = 3,
        MaxHide = 4,
    }

    public struct WindowResizeEvent
    {
        public i32 Width;
        public i32 Height;
        public WindowResize Type;

        public this(i32 width, i32 height, WindowResize type)
        {
            Width = width;
            Height = height;
            Type = type;
        }
    }
}
