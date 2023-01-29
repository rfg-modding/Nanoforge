using Nanoforge.Misc;
using Nanoforge.Gui;
using Nanoforge.App;
using Direct3D;
using Common;
using System;
using ImGui;
using Win32;

namespace Nanoforge.Render.ImGui
{
    public class ImGuiRenderer
    {
        private append ImGuiImplWin32 ImplWin32;
        private append ImGuiImplDX11 ImplDX11;
        private ID3D11DeviceContext* _context = null;
        private ID3D11Device* _device = null;
        private IDXGIFactory* _factory = null;
        private HWND _hwnd = 0;
        private u32 _drawCount = 0;

        public void Shutdown()
        {
            ImplDX11.Shutdown();
            ImplWin32.Shutdown();
            ImGui.DestroyContext();
        }

        public void BeginFrame(App app)
        {
            ImplDX11.NewFrame();
            ImplWin32.NewFrame();
            ImGui.NewFrame();
        }

        public void Render(App app)
        {
            ImGui.Render();
            ImplDX11.RenderDrawData(ImGui.GetDrawData());

            //Clear font texture data after a few frames. Uses a ton of memory.
            //Cleared after 60 frames since that ensures the font atlas was built and sent to the gpu so we can delete the cpu-side copy.
#if !DEBUG  //Don't do it in debug builds because it causes a crash
            if (_drawCount < 60)
            {
                if (_drawCount == 59)
                {
                    var io = ImGui.GetIO();
                    io.Fonts.ClearTexData();
                }

                _drawCount++;
            }
#endif
        }

        public void EndFrame(App app)
        {
            ImGui.EndFrame();
        }

        public bool Init(App app, ID3D11DeviceContext* context, ID3D11Device* device, IDXGIFactory* factory)
        {
            BuildConfig buildConfig = app.GetResource<BuildConfig>();
            Window window = app.GetResource<Window>();

            _device = device;
            _context = context;
            _factory = factory;
            _hwnd = window.Handle;

            /*if (!ImGui.CHECKVERSION())
                return false;*/
            if (ImGui.CreateContext() == null)
                return false;
            //ImplWin32.EnableDpiAwareness();

            ImGui.IO* io = ImGui.GetIO();
            io.DisplaySize = .(window.Width, window.Height);
            io.DisplayFramebufferScale = .(1.0f, 1.0f);
            io.ConfigFlags |= ImGui.ConfigFlags.NavEnableKeyboard;
            io.ConfigFlags |= ImGui.ConfigFlags.DockingEnable;
            //io.ConfigFlags |= ImGui.ConfigFlags.DpiEnableScaleFonts;
            //io.ConfigFlags |= ImGui.ConfigFlags.DpiEnableScaleViewports;

            if (!ImplWin32.Init(window))
                return false;
            if (!ImplDX11.Init(_context, _device, _factory))
                return false;

            SetupStyles(app);
            FontManager.LoadFonts(buildConfig);
            return true;
        }

        private void SetupStyles(App app)
        {
            //Set dark theme colors and style
            ImGui.StyleColorsDark();
            var style = ImGui.GetStyle();
            style.WindowPadding = .(8.0f, 8.0f);
            style.FramePadding = .(5.0f, 5.0f);
            style.ItemSpacing = .(8.0f, 8.0f);
            style.ItemInnerSpacing = .(8.0f, 6.0f);
            style.IndentSpacing = 25.0f;
            style.ScrollbarSize = 18.0f;
            style.GrabMinSize = 12.0f;
            style.WindowBorderSize = 1.0f;
            style.ChildBorderSize = 1.0f;
            style.PopupBorderSize = 1.0f;
            style.FrameBorderSize = 1.0f;
            style.TabBorderSize = 0.0f;
            style.WindowRounding = 4.0f;
            style.ChildRounding = 0.0f;
            style.FrameRounding = 4.0f;
            style.PopupRounding = 4.0f;
            style.ScrollbarRounding = 4.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 0.0f;
            style.Colors[(int)ImGui.Col.Text] = .(0.96f, 0.96f, 0.99f, 1.00f);
            style.Colors[(int)ImGui.Col.TextDisabled] = .(0.50f, 0.50f, 0.50f, 1.00f);
            style.Colors[(int)ImGui.Col.WindowBg] = .(0.1f, 0.1f, 0.1f, 1.0f);
            style.Colors[(int)ImGui.Col.ChildBg] = .(0.1f, 0.1f, 0.1f, 1.0f);
            style.Colors[(int)ImGui.Col.PopupBg] = .(0.04f, 0.04f, 0.05f, 1.00f);
            style.Colors[(int)ImGui.Col.Border] = .(0.216f, 0.216f, 0.216f, 1.0f);
            style.Colors[(int)ImGui.Col.BorderShadow] = .(0.00f, 0.00f, 0.00f, 0.00f);
            style.Colors[(int)ImGui.Col.FrameBg] = .(0.161f, 0.161f, 0.176f, 1.00f);
            style.Colors[(int)ImGui.Col.FrameBgHovered] = .(0.27f, 0.27f, 0.27f, 1.00f);
            style.Colors[(int)ImGui.Col.FrameBgActive] = .(0.255f, 0.255f, 0.275f, 1.00f);
            style.Colors[(int)ImGui.Col.TitleBg] = .(0.18f, 0.18f, 0.18f, 1.0f);
            style.Colors[(int)ImGui.Col.TitleBgActive] = .(0.18f, 0.18f, 0.18f, 1.0f);
            style.Colors[(int)ImGui.Col.TitleBgCollapsed] = .(0.18f, 0.18f, 0.18f, 1.0f);
            style.Colors[(int)ImGui.Col.MenuBarBg] = .(0.18f, 0.18f, 0.18f, 1.0f);
            style.Colors[(int)ImGui.Col.ScrollbarBg] = .(0.074f, 0.074f, 0.074f, 1.0f);
            style.Colors[(int)ImGui.Col.ScrollbarGrab] = .(0.31f, 0.31f, 0.32f, 1.00f);
            style.Colors[(int)ImGui.Col.ScrollbarGrabHovered] = .(0.41f, 0.41f, 0.42f, 1.00f);
            style.Colors[(int)ImGui.Col.ScrollbarGrabActive] = .(0.51f, 0.51f, 0.53f, 1.00f);
            style.Colors[(int)ImGui.Col.CheckMark] = .(0.44f, 0.44f, 0.47f, 1.00f);
            style.Colors[(int)ImGui.Col.SliderGrab] = .(0.44f, 0.44f, 0.47f, 1.00f);
            style.Colors[(int)ImGui.Col.SliderGrabActive] = .(0.59f, 0.59f, 0.61f, 1.00f);
            style.Colors[(int)ImGui.Col.Button] = .(0.20f, 0.20f, 0.22f, 1.00f);
            style.Colors[(int)ImGui.Col.ButtonHovered] = .(0.44f, 0.44f, 0.47f, 1.00f);
            style.Colors[(int)ImGui.Col.ButtonActive] = .(0.59f, 0.59f, 0.61f, 1.00f);
            style.Colors[(int)ImGui.Col.Header] = .(0.20f, 0.20f, 0.22f, 1.00f);
            style.Colors[(int)ImGui.Col.HeaderHovered] = .(0.44f, 0.44f, 0.47f, 1.00f);
            style.Colors[(int)ImGui.Col.HeaderActive] = .(0.59f, 0.59f, 0.61f, 1.00f);
            style.Colors[(int)ImGui.Col.Separator] = .(1.00f, 1.00f, 1.00f, 0.20f);
            style.Colors[(int)ImGui.Col.SeparatorHovered] = .(0.44f, 0.44f, 0.47f, 0.39f);
            style.Colors[(int)ImGui.Col.SeparatorActive] = .(0.44f, 0.44f, 0.47f, 0.59f);
            style.Colors[(int)ImGui.Col.ResizeGrip] = .(0.26f, 0.59f, 0.98f, 0.00f);
            style.Colors[(int)ImGui.Col.ResizeGripHovered] = .(0.26f, 0.59f, 0.98f, 0.00f);
            style.Colors[(int)ImGui.Col.ResizeGripActive] = .(0.26f, 0.59f, 0.98f, 0.00f);
            style.Colors[(int)ImGui.Col.Tab] = .(0.18f, 0.18f, 0.18f, 1.00f);
            style.Colors[(int)ImGui.Col.TabHovered] = .(0.00f, 0.48f, 0.80f, 1.0f);
            style.Colors[(int)ImGui.Col.TabActive] = .(0.00f, 0.48f, 0.80f, 1.0f);
            style.Colors[(int)ImGui.Col.TabUnfocused] = .(0.18f, 0.18f, 0.18f, 1.00f);
            style.Colors[(int)ImGui.Col.TabUnfocusedActive] = .(0.00f, 0.48f, 0.80f, 1.0f);
            style.Colors[(int)ImGui.Col.DockingPreview] = .(0.00f, 0.48f, 0.80f, 0.776f);
            style.Colors[(int)ImGui.Col.DockingEmptyBg] = .(0.114f, 0.114f, 0.125f, 1.0f);
            style.Colors[(int)ImGui.Col.PlotLines] = .(0.96f, 0.96f, 0.99f, 1.00f);
            style.Colors[(int)ImGui.Col.PlotLinesHovered] = .(0.12f, 1.00f, 0.12f, 1.00f);
            style.Colors[(int)ImGui.Col.PlotHistogram] = .(0.23f, 0.51f, 0.86f, 1.00f);
            style.Colors[(int)ImGui.Col.PlotHistogramHovered] = .(0.12f, 1.00f, 0.12f, 1.00f);
            style.Colors[(int)ImGui.Col.TextSelectedBg] = .(0.26f, 0.59f, 0.98f, 0.35f);
            style.Colors[(int)ImGui.Col.DragDropTarget] = .(0.26f, 0.59f, 0.98f, 0.00f);
            style.Colors[(int)ImGui.Col.NavHighlight] = .(0.26f, 0.59f, 0.98f, 1.00f);
            style.Colors[(int)ImGui.Col.NavWindowingHighlight] = .(1.00f, 1.00f, 1.00f, 0.70f);
            style.Colors[(int)ImGui.Col.NavWindowingDimBg] = .(0.80f, 0.80f, 0.80f, 0.20f);
            style.Colors[(int)ImGui.Col.ModalWindowDimBg] = .(0.80f, 0.80f, 0.80f, 0.35f);
            style.Colors[(int)ImGui.Col.TableRowBgAlt] = .(0.12f, 0.12f, 0.14f, 1.00f);
        }
    }
}