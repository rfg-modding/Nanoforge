using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
using System;
using Win32;

namespace Nanoforge.Misc
{
    //Wrapper for glfw input handling
    [System]
    public class Input : ISystem
    {
        //When set to false all key status functions return false. Added so gTaskDialog can disable keybinds when tasks are running to avoid users doing things like modifying a map mid save.
        public static bool KeysEnabled = true;

        //Input state
        private List<KeyState> _lastFrameKeyDownStates = new .()..Resize(349) ~ delete _;
        private List<KeyState> _keyDownStates = new .()..Resize(349) ~ delete _;
        private List<bool> _mouseDownStates = new .()..Resize(3) ~ delete _;
        private float _lastMouseX = 0.0f;
        private float _lastMouseY = 0.0f;

        //Returns true if the provided is down, including if it's being held down for multiple frames.
        public bool KeyDown(KeyCode keyCode) => KeysEnabled && _keyDownStates[(int)keyCode] > .Up;
        //Returns true if the provided key was pressed this frame. It will return false if the key has been down for several frames
        public bool KeyPressed(KeyCode keyCode) => KeysEnabled && _keyDownStates[(int)keyCode] == .Down;

        private bool _shiftDown = false;
        private bool _controlDown = false;
        private bool _altDown = false;
        public bool ShiftDown => KeysEnabled && _shiftDown;
        public bool ControlDown => KeysEnabled && _controlDown;
        public bool AltDown => KeysEnabled && _altDown;

        //Returns true if the provided mouse button is down
        public bool MouseButtonDown(MouseButtonCode keyCode) => KeysEnabled && _mouseDownStates[(int)keyCode];
        public float MousePosX { get; private set; } = 0.0f;
        public float MousePosY { get; private set; } = 0.0f;
        public float MouseDeltaX { get; private set; } = 0.0f;
        public float MouseDeltaY { get; private set; } = 0.0f;
        public float MouseScrollY { get; private set; } = 0.0f;
        //Returns true if the mouse moved this frame
        public bool MouseMoved { get; private set; } = false;
        //Returns true if any mouse button was pressed this frame
        public bool MouseButtonPressed { get; private set; } = false;

        static void ISystem.Build(App app)
        {
            
        }

        [SystemInit]
        private void Init(App app)
        {

        }

        [SystemStage(.BeginFrame)]
        private void BeginFrame()
        {
            for (int i in 0..<_keyDownStates.Count)
            {
                //Set key to repeat if it's held down for two frames
                //Note: Ideally this should be handled by WndProcKeyDown already, but in practice it doesn't seem to work. At least on my system the second keydown message can be delayed by 1-2 dozen frames
                //      That breaks KeyPressed(). Unsure if it's an issue with the system update code, my system, or maybe my keyboard
                if (_lastFrameKeyDownStates[i] == .Down && _keyDownStates[i] == .Down)
                    _keyDownStates[i] = .Repeat;

                _lastFrameKeyDownStates[i] = _keyDownStates[i];
            }
        }

        //Called at the end of each frame. Resets input state. Should only be called by the engine core
        [SystemStage(.EndFrame)]
        private void EndFrame()
        {
            MouseButtonPressed = false;
            MouseMoved = false;
            MouseScrollY = 0.0f;
            MouseDeltaX = 0;
            MouseDeltaY = 0;
            _shiftDown = KeyDown(.LeftShift) || KeyDown(.RightShift) || KeyDown(.Shift);
            _controlDown = KeyDown(.LeftControl) || KeyDown(.RightControl) || KeyDown(.Control);
            _altDown  = KeyDown(.LeftAlt) || KeyDown(.RightAlt) || KeyDown(.Alt);
        }

        //Takes WndProc WM_KEYDOWN message parameters as input
        private void WndProcKeyDown(WPARAM wParam, LPARAM lParam)
        {
            if (wParam >= (uint)_keyDownStates.Count)
                return;

            var keyState = ref _keyDownStates[(int)wParam];
            switch (keyState)
            {
                case .Up:
                    keyState = .Down;
                case .Down:
                    keyState = .Repeat;
                case .Repeat:
                    fallthrough;
                default:
            }
        }

        //Takes WndProc WM_KEYUP message parameters as input
        private void WndProcKeyUp(WPARAM wParam, LPARAM lParam)
        {
            if (wParam >= (uint)_keyDownStates.Count)
                return;

            _keyDownStates[(int)wParam] = .Up;
        }

        //Takes WndProc mouse message parameters as input
        private void WndProcMouse(u32 msg, WPARAM wParam, LPARAM lParam)
        {
            switch (msg)
            {
                case Win32.WM_LBUTTONDOWN:
                    _mouseDownStates[(int)MouseButtonCode.Left] = true;
                    MouseButtonPressed = true;
                case Win32.WM_LBUTTONUP:
                    _mouseDownStates[(int)MouseButtonCode.Left] = false;
                    MouseButtonPressed = true;
                case Win32.WM_RBUTTONDOWN:
                    _mouseDownStates[(int)MouseButtonCode.Right] = true;
                    MouseButtonPressed = true;
                case Win32.WM_RBUTTONUP:
                    _mouseDownStates[(int)MouseButtonCode.Right] = false;
                    MouseButtonPressed = true;
                case Win32.WM_MBUTTONDOWN:
                    _mouseDownStates[(int)MouseButtonCode.Middle] = true;
                    MouseButtonPressed = true;
                case Win32.WM_MBUTTONUP:
                    _mouseDownStates[(int)MouseButtonCode.Middle] = false;
                    MouseButtonPressed = true;
                case Win32.WM_MOUSEMOVE:
                    int mouseX = Win32.GET_X_LPARAM!(lParam);
                    int mouseY = Win32.GET_Y_LPARAM!(lParam);
                    MouseMoved = true;

                    _lastMouseX = MousePosX;
                    _lastMouseY = MousePosY;
                    MousePosX = mouseX;
                    MousePosY = mouseY;
                    MouseDeltaX = MousePosX - _lastMouseX;
                    MouseDeltaY = MousePosY - _lastMouseY;
                case Win32.WM_MOUSEWHEEL:
                    int distance = Win32.HIWORD!(wParam); //Multiples of Win32.WHEEL_DELTA: https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel
                    MouseScrollY = distance * Win32.WHEEL_DELTA;
                default:
            }
        }
    }

    enum KeyState
    {
        Up,
        Down,
        Repeat
    }

    enum MouseButtonCode : int
    {
        Left,
        Right,
        Middle
    }

    //TODO: Add the rest of the keycodes from here & search for any additional ones: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
    //      I only added the ones on my keyboard + the ones I thought might be useful
    enum KeyCode : int
    {
        None = 0,
        //LeftMouseButton = 1,
        //RightMouseButton = 2, //Storing mouse down state in separate enum & list for the moment
        Cancel = 3,
        //MiddleMouseButton = 4,
        //X1MouseButton = 5,
        //X2MouseButton = 6,
        Backspace = 8,
        Tab = 9,
        Enter = 0xD,
        Shift = 0x10,
        Control = 0x11,
        Alt = 0x12,
        Pause = 0x13,
        CapsLock = 0x14,
        Escape = 0x1B,
        Spacebar = 0x20,
        PageUp = 0x21,
        PageDown = 0x22,
        End = 0x23,
        Home = 0x24,
        LeftArrow = 0x25,
        UpArrow = 0x26,
        RightArrow = 0x27,
        DownArrow = 0x28,
        Select = 0x29,
        Print = 0x2A,
        Execute = 0x2B,
        PrintScreen = 0x2C,
        Insert = 0x2D,
        Delete = 0x2E,
        Help = 0x2F,
        Zero = 0x30,
        One = 0x31,
        Two = 0x32,
        Three = 0x33,
        Four = 0x34,
        Five = 0x35,
        Six = 0x36,
        Seven = 0x37,
        Eight = 0x38,
        Nine = 0x39,
        A = 0x41,
        B = 0x42,
        C = 0x43,
        D = 0x44,
        E = 0x45,
        F = 0x46,
        G = 0x47,
        H = 0x48,
        I = 0x49,
        J = 0x4A,
        K = 0x4B,
        L = 0x4C,
        M = 0x4D,
        N = 0x4E,
        O = 0x4F,
        P = 0x50,
        Q = 0x51,
        R = 0x52,
        S = 0x53,
        T = 0x54,
        U = 0x55,
        V = 0x56,
        W = 0x57,
        X = 0x58,
        Y = 0x59,
        Z = 0x5A,
        WindowsLeft = 0x5B,
        WindowsRight = 0x5C,
        Applications = 0x5D,
        Sleep = 0x5F,
        Numpad0 = 0x60,
        Numpad1 = 0x61,
        Numpad2 = 0x62,
        Numpad3 = 0x63,
        Numpad4 = 0x64,
        Numpad5 = 0x65,
        Numpad6 = 0x66,
        Numpad7 = 0x67,
        Numpad8 = 0x68,
        Numpad9 = 0x69,
        NumpadMultiply = 0x6A,
        NumpadAdd = 0x6B,
        Separator = 0x6C,
        NumpadSubtract = 0x6D,
        NumpadDecimal = 0x6E,
        NumpadDivide = 0x6F,
        F1 = 0x70,
        F2 = 0x71,
        F3 = 0x72,
        F4 = 0x73,
        F5 = 0x74,
        F6 = 0x75,
        F7 = 0x76,
        F8 = 0x77,
        F9 = 0x78,
        F10 = 0x79,
        F11 = 0x7A,
        F12 = 0x7B,
        F13 = 0x7C,
        F14 = 0x7D,
        F15 = 0x7E,
        F16 = 0x7F,
        F17 = 0x80,
        F18 = 0x81,
        F19 = 0x82,
        F20 = 0x83,
        F21 = 0x84,
        F22 = 0x85,
        F23 = 0x86,
        F24 = 0x87,
        NumLock = 0x90,
        ScrollLock = 0x91,
        LeftShift = 0xA0,
        RightShift = 0xA1,
        LeftControl = 0xA2,
        RightControl = 0xA3,
        LeftAlt = 0xA4,
        RightAlt = 0xA5,
        VolumeMute = 0xAD,
        VolumeDown = 0xAE,
        VolumeUp = 0xAF,
        MediaNextTrack = 0xB0,
        MediaPreviousTrack = 0xB1,
        MediaStop = 0xB2,
        MediaPlayPause = 0xB3,
        Comma = 0xBC,
        Minus = 0xBD,
        Period = 0xBE,
        Slash = 0xBF,
        Semicolon = 0xBA,
        AccentGrave = 0xC0,
        LeftBracket = 0xDB,
        BackSlash = 0xDC,
        RightBracket = 0xDD,
        Apostrophe = 0xDE,
        Equal = 0xBB,
        Play = 0xFA,
        Zoom = 0xFB
    }
}
