using Nanoforge.Misc;
using Common;
using System;
using ImGui;
using Win32;

namespace Nanoforge.Render.ImGui
{
    //Based on imgui_impl_win32.cpp example implementation in main imgui repo
    //Note: Didn't port code for gamepads and multiple viewports since I don't need it yet.
    class ImGuiImplWin32
    {
        public HWND Hwnd = 0;
        public HWND MouseHwnd = 0;
        public bool MouseTracked = false;
        public i64 Time = 0;
        public i64 TicksPerSecond = 0;
        public ImGui.MouseCursor LastMouseCursor;
        public bool HasGamepad = false;
        public bool WantUpdateHasGamepad = false;
        public bool WantUpdateMonitors = false;
        public i32 MouseButtonsDown = 0;
        private static u32 DBT_DEVNODES_CHANGED = 0x0007; //Todo: See if this can be grabed from BeefWin32 instead
        private const i32 IM_VK_KEYPAD_ENTER = (int)KeyCode.Enter + 256; //There is no distinct VK_xxx for keypad enter, instead it is VK_RETURN + KF_EXTENDED, we assign it an arbitrary value to make code more readable (VK_ codes go up to 255)

        public bool Init(Window window)
        {
            if (Win32.QueryPerformanceCounter(var perfFrequency) == 0)
                Runtime.FatalError("Failed to initialize Dear ImGui performance counter.");
            if (Win32.QueryPerformanceFrequency(var perfCounter) == 0)
                Runtime.FatalError("Failed to initialize Dear ImGui performance counter frequency.");

            ImGui.IO* io = ImGui.GetIO();
            io.BackendPlatformName = "Win32";
            io.BackendFlags |= .HasMouseCursors; //We can honor GetMouseCursor() values (optional)
            io.BackendFlags |= .HasSetMousePos; //We can honor io.WantSetMousePos requests (optional, rarely used)
            //io.BackendFlags |= .PlatformHasViewports; //We can create multi-viewports on the Platform side (optional)
            //io.BackendFlags |= .HasMouseHoveredViewport; //We can set io.MouseHoveredViewport correctly (optional, not easy)

            Hwnd = window.Handle;
            WantUpdateHasGamepad = true;
            WantUpdateMonitors = true;
            TicksPerSecond = perfCounter.QuadPart;
            Time = perfFrequency.QuadPart;
            LastMouseCursor = .COUNT;

            //Set platform dependent data in viewport
            ImGui.GetMainViewport().PlatformHandleRaw = (void*)Hwnd;

            //Add ImGui wndproc handler
            window.WndProcExtensions.Add(new => this.WndProcHandler);

            return true;
        }

        public void Shutdown()
        {
            ImGui.IO* io = ImGui.GetIO();
            io.BackendPlatformName = null;
        }

        public void NewFrame()
        {
            ImGui.IO* io = ImGui.GetIO();
            Win32.GetClientRect(Hwnd, var rect);
            io.DisplaySize = .((f32)(rect.right - rect.left), (f32)(rect.bottom - rect.top));

            //Update timestep
            Win32.QueryPerformanceCounter(var currentTime);
            io.DeltaTime = (f32)(currentTime.QuadPart - Time) / (f32)TicksPerSecond;
            Time = currentTime.QuadPart;

            //Update OS mouse position
            UpdateMouseData();

            //Process workarounds for known Windows key handling issues
            ProcessKeyEventsWorkarounds();

            //Update OS cursor with the cursor requested by imgui
            ImGui.MouseCursor mouseCursor = io.MouseDrawCursor ? .None : ImGui.GetMouseCursor();
            if (LastMouseCursor != mouseCursor)
            {
                LastMouseCursor = mouseCursor;
                UpdateMouseCursor();
            }
        }

        public bool UpdateMouseCursor()
        {
            ImGui.IO* io = ImGui.GetIO();
            if ((io.ConfigFlags & ImGui.ConfigFlags.NoMouseCursorChange) != 0)
                return false;

            ImGui.MouseCursor imguiCursor = ImGui.GetMouseCursor();
            if (imguiCursor == .None || io.MouseDrawCursor)
            {
                Win32.SetCursor(0); //Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
            }
            else
            {
                PWSTR win32Cursor = Win32.IDC_ARROW;
                switch (imguiCursor)
                {
                    case .Arrow:
                        win32Cursor = Win32.IDC_ARROW;
                    case .TextInput:
                        win32Cursor = Win32.IDC_IBEAM;
                    case .ResizeAll:
                        win32Cursor = Win32.IDC_SIZEALL;
                    case .ResizeNS:
                        win32Cursor = Win32.IDC_SIZENS;
                    case .ResizeEW:
                        win32Cursor = Win32.IDC_SIZEWE;
                    case .ResizeNESW:
                        win32Cursor = Win32.IDC_SIZENESW;
                    case .ResizeNWSE:
                        win32Cursor = Win32.IDC_SIZENWSE;
                    case .Hand:
                        win32Cursor = Win32.IDC_HAND;
                    case .NotAllowed:
                        win32Cursor = Win32.IDC_NO;
                    default:
                        //win32Cursor = Win32.IDC_ARROW;
                }
                HCURSOR cursor = Win32.LoadCursorW(0, win32Cursor);
                Win32.SetCursor(cursor);
            }
            return true;
        }

        private bool IsVkDown(i32 vk)
        {
            return (Win32.GetKeyState(vk) & 0x8000) != 0;
        }

        private void AddKeyEvent(ImGui.Key key, bool down, i32 nativeKeycode, i32 nativeScancode = -1)
        {
            ImGui.IO* io = ImGui.GetIO();
            io.AddKeyEvent(key, down);
            io.SetKeyEventNativeData(key, nativeKeycode, nativeScancode);
        }

        private void ProcessKeyEventsWorkarounds()
        {
            //Left & right Shift keys: when both are pressed together, Windows tend to not generate the WM_KEYUP event for the first released one.
            if (ImGui.IsKeyDown(.LeftShift) && !IsVkDown((i32)KeyCode.LeftShift))
                AddKeyEvent(.LeftShift, false, (i32)KeyCode.LeftShift);
            if (ImGui.IsKeyDown(.RightShift) && !IsVkDown((i32)KeyCode.RightShift))
                AddKeyEvent(.RightShift, false, (i32)KeyCode.RightShift);

            //Sometimes WM_KEYUP for Win key is not passed down to the app (e.g. for Win+V on some setups, according to GLFW).
            if (ImGui.IsKeyDown(.LeftSuper) && !IsVkDown((i32)KeyCode.WindowsLeft))
                AddKeyEvent(.LeftSuper, false, (i32)KeyCode.WindowsLeft);
            if (ImGui.IsKeyDown(.RightSuper) && !IsVkDown((i32)KeyCode.WindowsRight))
                AddKeyEvent(.RightSuper, false, (i32)KeyCode.WindowsRight);
        }

        private void UpdateKeyModifiers()
        {
            ImGui.IO* io = ImGui.GetIO();
            io.AddKeyEvent(.Mod_Ctrl, IsVkDown((i32)KeyCode.LeftControl));
            io.AddKeyEvent(.Mod_Shift, IsVkDown((i32)KeyCode.LeftShift));
            io.AddKeyEvent(.Mod_Alt, IsVkDown((i32)KeyCode.LeftAlt));
            io.AddKeyEvent(.Mod_Super, IsVkDown((i32)KeyCode.Applications));
        }

        public void UpdateMouseData()
        {
            ImGui.IO* io = ImGui.GetIO();
            Runtime.Assert(Hwnd != 0);

            let isAppFocused = (Win32.GetForegroundWindow() == Hwnd);
            if (isAppFocused)
            {
                //(Optional) Set OS mouse position from Dear ImGui if requested (rarely used, only when ImGuiConfigFlags_NavEnableSetMousePos is enabled by user)
                if (io.WantSetMousePos)
                {
                    POINT pos = .((i32)io.MousePos.x, (i32)io.MousePos.y);
                    if (Win32.ClientToScreen(Hwnd, &pos) != 0)
                        Win32.SetCursorPos(pos.x, pos.y);
                }

                //(Optional) Fallback to provide mouse position when focused (WM_MOUSEMOVE already provides this when hovered or captured)
                if (!io.WantSetMousePos && !MouseTracked)
                {
                    if (Win32.GetCursorPos(var pos) != 0)
                        if (Win32.ScreenToClient(Hwnd, &pos) != 0)
                            io.AddMousePosEvent((f32)pos.x, (f32)pos.y);
                }
            }
        }

        private static ImGui.Key VirtualKeyToImGuiKey(WPARAM wParam)
        {
            switch (wParam)
            {
                case (int)KeyCode.Tab: return ImGui.Key.Tab;
                case (int)KeyCode.LeftArrow: return ImGui.Key.LeftArrow;
                case (int)KeyCode.RightArrow: return ImGui.Key.RightArrow;
                case (int)KeyCode.UpArrow: return ImGui.Key.UpArrow;
                case (int)KeyCode.DownArrow: return ImGui.Key.DownArrow;
                case (int)KeyCode.PageUp: return ImGui.Key.PageUp;
                case (int)KeyCode.PageDown: return ImGui.Key.PageDown;
                case (int)KeyCode.Home: return ImGui.Key.Home;
                case (int)KeyCode.End: return ImGui.Key.End;
                case (int)KeyCode.Insert: return ImGui.Key.Insert;
                case (int)KeyCode.Delete: return ImGui.Key.Delete;
                case (int)KeyCode.Backspace: return ImGui.Key.Backspace;
                case (int)KeyCode.Spacebar: return ImGui.Key.Space;
                case (int)KeyCode.Enter: return ImGui.Key.Enter;
                case (int)KeyCode.Escape: return ImGui.Key.Escape;
                case (int)KeyCode.Apostrophe: return ImGui.Key.Apostrophe;
                case (int)KeyCode.Comma: return ImGui.Key.Comma;
                case (int)KeyCode.Minus: return ImGui.Key.Minus;
                case (int)KeyCode.Period: return ImGui.Key.Period;
                case (int)KeyCode.Slash: return ImGui.Key.Slash;
                case (int)KeyCode.Semicolon: return ImGui.Key.Semicolon;
                case (int)KeyCode.Equal: return ImGui.Key.Equal;
                case (int)KeyCode.LeftBracket: return ImGui.Key.LeftBracket;
                case (int)KeyCode.BackSlash: return ImGui.Key.Backslash;
                case (int)KeyCode.RightBracket: return ImGui.Key.RightBracket;
                case (int)KeyCode.AccentGrave: return ImGui.Key.GraveAccent;
                case (int)KeyCode.CapsLock: return ImGui.Key.CapsLock;
                case (int)KeyCode.ScrollLock: return ImGui.Key.ScrollLock;
                case (int)KeyCode.NumLock: return ImGui.Key.NumLock;
                case (int)KeyCode.PrintScreen: return ImGui.Key.PrintScreen;
                case (int)KeyCode.Pause: return ImGui.Key.Pause;
                case (int)KeyCode.Numpad0: return ImGui.Key.Keypad0;
                case (int)KeyCode.Numpad1: return ImGui.Key.Keypad1;
                case (int)KeyCode.Numpad2: return ImGui.Key.Keypad2;
                case (int)KeyCode.Numpad3: return ImGui.Key.Keypad3;
                case (int)KeyCode.Numpad4: return ImGui.Key.Keypad4;
                case (int)KeyCode.Numpad5: return ImGui.Key.Keypad5;
                case (int)KeyCode.Numpad6: return ImGui.Key.Keypad6;
                case (int)KeyCode.Numpad7: return ImGui.Key.Keypad7;
                case (int)KeyCode.Numpad8: return ImGui.Key.Keypad8;
                case (int)KeyCode.Numpad9: return ImGui.Key.Keypad9;
                case (int)KeyCode.NumpadDecimal: return ImGui.Key.KeypadDecimal;
                case (int)KeyCode.NumpadDivide: return ImGui.Key.KeypadDivide;
                case (int)KeyCode.NumpadMultiply: return ImGui.Key.KeypadMultiply;
                case (int)KeyCode.NumpadSubtract: return ImGui.Key.KeypadSubtract;
                case (int)KeyCode.NumpadAdd: return ImGui.Key.KeypadAdd;
                case IM_VK_KEYPAD_ENTER: return ImGui.Key.KeypadEnter;
                case (int)KeyCode.LeftShift: return ImGui.Key.LeftShift;
                case (int)KeyCode.LeftControl: return ImGui.Key.LeftCtrl;
                case (int)KeyCode.LeftAlt: return ImGui.Key.LeftAlt;
                case (int)KeyCode.WindowsLeft: return ImGui.Key.LeftSuper;
                case (int)KeyCode.RightShift: return ImGui.Key.RightShift;
                case (int)KeyCode.RightControl: return ImGui.Key.RightCtrl;
                case (int)KeyCode.RightAlt: return ImGui.Key.RightAlt;
                case (int)KeyCode.WindowsRight: return ImGui.Key.RightSuper;
                case (int)KeyCode.Applications: return ImGui.Key.Menu;
                case (int)KeyCode.Zero: return ImGui.Key.N0;
                case (int)KeyCode.One: return ImGui.Key.N1;
                case (int)KeyCode.Two: return ImGui.Key.N2;
                case (int)KeyCode.Three: return ImGui.Key.N3;
                case (int)KeyCode.Four: return ImGui.Key.N4;
                case (int)KeyCode.Five: return ImGui.Key.N5;
                case (int)KeyCode.Six: return ImGui.Key.N6;
                case (int)KeyCode.Seven: return ImGui.Key.N7;
                case (int)KeyCode.Eight: return ImGui.Key.N8;
                case (int)KeyCode.Nine: return ImGui.Key.N9;
                case (int)KeyCode.A: return ImGui.Key.A;
                case (int)KeyCode.B: return ImGui.Key.B;
                case (int)KeyCode.C: return ImGui.Key.C;
                case (int)KeyCode.D: return ImGui.Key.D;
                case (int)KeyCode.E: return ImGui.Key.E;
                case (int)KeyCode.F: return ImGui.Key.F;
                case (int)KeyCode.G: return ImGui.Key.G;
                case (int)KeyCode.H: return ImGui.Key.H;
                case (int)KeyCode.I: return ImGui.Key.I;
                case (int)KeyCode.J: return ImGui.Key.J;
                case (int)KeyCode.K: return ImGui.Key.K;
                case (int)KeyCode.L: return ImGui.Key.L;
                case (int)KeyCode.M: return ImGui.Key.M;
                case (int)KeyCode.N: return ImGui.Key.N;
                case (int)KeyCode.O: return ImGui.Key.O;
                case (int)KeyCode.P: return ImGui.Key.P;
                case (int)KeyCode.Q: return ImGui.Key.Q;
                case (int)KeyCode.R: return ImGui.Key.R;
                case (int)KeyCode.S: return ImGui.Key.S;
                case (int)KeyCode.T: return ImGui.Key.T;
                case (int)KeyCode.U: return ImGui.Key.U;
                case (int)KeyCode.V: return ImGui.Key.V;
                case (int)KeyCode.W: return ImGui.Key.W;
                case (int)KeyCode.X: return ImGui.Key.X;
                case (int)KeyCode.Y: return ImGui.Key.Y;
                case (int)KeyCode.Z: return ImGui.Key.Z;
                case (int)KeyCode.F1: return ImGui.Key.F1;
                case (int)KeyCode.F2: return ImGui.Key.F2;
                case (int)KeyCode.F3: return ImGui.Key.F3;
                case (int)KeyCode.F4: return ImGui.Key.F4;
                case (int)KeyCode.F5: return ImGui.Key.F5;
                case (int)KeyCode.F6: return ImGui.Key.F6;
                case (int)KeyCode.F7: return ImGui.Key.F7;
                case (int)KeyCode.F8: return ImGui.Key.F8;
                case (int)KeyCode.F9: return ImGui.Key.F9;
                case (int)KeyCode.F10: return ImGui.Key.F10;
                case (int)KeyCode.F11: return ImGui.Key.F11;
                case (int)KeyCode.F12: return ImGui.Key.F12;
                default:
                    return .None;
            }
        }

        public LRESULT WndProcHandler(HWND hwnd, uint32 msg, WPARAM wParam, LPARAM lParam)
        {
            if (ImGui.GetCurrentContext() == null)
                return 0;

            ImGui.IO* io = ImGui.GetIO();
            switch (msg)
            {
                case Win32.WM_MOUSEMOVE:
                    //We need to call TrackMouseEvent in order to receive WM_MOUSELEAVE events
                    MouseHwnd = hwnd;
                    if (!MouseTracked)
                    {
                        TRACKMOUSEEVENT tme = .();
                        tme.cbSize = sizeof(TRACKMOUSEEVENT);
                        tme.dwFlags = .LEAVE;
                        tme.hwndTrack = hwnd;
                        tme.dwHoverTime = 0;
                        Win32.TrackMouseEvent(&tme);
                        MouseTracked = true;
                    }
                    f32 mouseX = (f32)Win32.GET_X_LPARAM!(lParam);
                    f32 mouseY = (f32)Win32.GET_Y_LPARAM!(lParam);
                    io.AddMousePosEvent(mouseX, mouseY);
                    break;

                case Win32.WM_MOUSELEAVE:
                    if (MouseHwnd == hwnd)
                        MouseHwnd = 0;

                    MouseTracked = false;
                    io.AddMousePosEvent(-f32.MaxValue, -f32.MaxValue);
                    break;

                case Win32.WM_LBUTTONDOWN, Win32.WM_LBUTTONDBLCLK, Win32.WM_RBUTTONDOWN, Win32.WM_RBUTTONDBLCLK, Win32.WM_MBUTTONDOWN, Win32.WM_MBUTTONDBLCLK, Win32.WM_XBUTTONDOWN, Win32.WM_XBUTTONDBLCLK:
                    i32 button = 0;
                    if (msg == Win32.WM_LBUTTONDOWN || msg == Win32.WM_LBUTTONDBLCLK)
                        button = 0;
                    if (msg == Win32.WM_RBUTTONDOWN || msg == Win32.WM_RBUTTONDBLCLK)
                        button = 1;
                    if (msg == Win32.WM_MBUTTONDOWN || msg == Win32.WM_MBUTTONDBLCLK)
                        button = 2;
                    if (msg == Win32.WM_XBUTTONDOWN || msg == Win32.WM_XBUTTONDBLCLK)
                        button = (Win32.HIWORD!(wParam) == 1) ? 3 : 4;

                    if (MouseButtonsDown == 0 && Win32.GetCapture() == 0)
                        Win32.SetCapture(Hwnd);

                    MouseButtonsDown |= (1 << button);
                    io.AddMouseButtonEvent(button, true);
                    return 0;

                case Win32.WM_LBUTTONUP,Win32.WM_RBUTTONUP,Win32.WM_MBUTTONUP,Win32.WM_XBUTTONUP:
                    i32 button = 0;
                    if (msg == Win32.WM_LBUTTONUP)
                        button = 0;
                    if (msg == Win32.WM_RBUTTONUP)
                        button = 1;
                    if (msg == Win32.WM_MBUTTONUP)
                        button = 2;
                    if (msg == Win32.WM_XBUTTONUP)
                        button = (Win32.HIWORD!(wParam) == 1) ? 3 : 4;

                    MouseButtonsDown &= ~(1 << button);
                    if (MouseButtonsDown == 0 && Win32.GetCapture() == Hwnd)
                        Win32.ReleaseCapture();

                    io.AddMouseButtonEvent(button, false);
                    return 0;

                case Win32.WM_MOUSEWHEEL:
                    io.AddMouseWheelEvent(0.0f, (f32)(int16)Win32.HIWORD!(wParam) / (f32)Win32.WHEEL_DELTA);
                    return 0;

                case Win32.WM_MOUSEHWHEEL:
                    io.AddMouseWheelEvent((f32)(int16)Win32.HIWORD!(wParam) / (f32)Win32.WHEEL_DELTA, 0.0f);
                    return 0;

                case Win32.WM_KEYDOWN, Win32.WM_KEYUP, Win32.WM_SYSKEYDOWN, Win32.WM_SYSKEYUP:
                    let isKeyDown = (msg == Win32.WM_KEYDOWN || msg == Win32.WM_SYSKEYDOWN);
                    if (wParam < 256)
                    {
                        //Submit modifiers
                        UpdateKeyModifiers();

                        //Obtain virtual key code
                        //(keypad enter doesn't have its own... VK_RETURN with KF_EXTENDED flag means keypad enter, see IM_VK_KEYPAD_ENTER definition for details, it is mapped to ImGuiKey_KeyPadEnter.)
                        i32 vk = (i32)wParam;
                        if ((wParam == (int)KeyCode.Enter) && (Win32.HIWORD!(lParam) & Win32.KF_EXTENDED) != 0)
                            vk = IM_VK_KEYPAD_ENTER;

                        //Submit key event
                        ImGui.Key key = VirtualKeyToImGuiKey((WPARAM)vk);
                        i32 scancode = (i32)Win32.LOBYTE!(Win32.HIWORD!(lParam));
                        if (key != .None)
                            AddKeyEvent(key, isKeyDown, vk, scancode);

                        if (vk == (i32)KeyCode.Shift)
                        {
                            if (IsVkDown((i32)KeyCode.LeftShift) == isKeyDown)
                                AddKeyEvent(.LeftShift, isKeyDown, (i32)KeyCode.LeftShift, scancode);
                            if (IsVkDown((i32)KeyCode.RightShift) == isKeyDown)
                                AddKeyEvent(.RightShift, isKeyDown, (i32)KeyCode.RightShift, scancode);
                        }
                        else if (vk == (i32)KeyCode.Control)
                        {
                            if (IsVkDown((i32)KeyCode.LeftControl) == isKeyDown)
                                AddKeyEvent(.LeftCtrl, isKeyDown, (i32)KeyCode.LeftControl, scancode);
                            if (IsVkDown((i32)KeyCode.RightControl) == isKeyDown)
                                AddKeyEvent(.RightCtrl, isKeyDown, (i32)KeyCode.RightControl, scancode);
                        }
                        else if (vk == (i32)KeyCode.Alt)
                        {
                            if (IsVkDown((i32)KeyCode.LeftAlt) == isKeyDown)
                                AddKeyEvent(.LeftAlt, isKeyDown, (i32)KeyCode.LeftAlt, scancode);
                            if (IsVkDown((i32)KeyCode.RightAlt) == isKeyDown)
                                AddKeyEvent(.RightAlt, isKeyDown, (i32)KeyCode.RightAlt, scancode);
                        }
                    }

                    return 0;

                case Win32.WM_SETFOCUS,Win32.WM_KILLFOCUS:
                    io.AddFocusEvent(msg == Win32.WM_SETFOCUS);
                    return 0;

                case Win32.WM_CHAR:
                    //You can also use ToAscii()+GetKeyboardState() to retrieve characters.
                    if (wParam > 0 && wParam < 0x10000)
                        io.AddInputCharacterUTF16((u16)wParam);

                    return 0;

                case Win32.WM_SETCURSOR:
                    if (Win32.LOWORD!(lParam) == Win32.HTCLIENT)
                        if (UpdateMouseCursor())
                            return 1;

                    return 0;

                case Win32.WM_DEVICECHANGE:
                    if (wParam == DBT_DEVNODES_CHANGED)
                        WantUpdateHasGamepad = true;

                    return 0;

                case Win32.WM_DISPLAYCHANGE:
                    WantUpdateMonitors = true;
                    return 0;

                default:
                    break;
            }

            return 0;
        }
    }
}