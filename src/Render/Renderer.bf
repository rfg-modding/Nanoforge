using Nanoforge.Render.Resources;
using System.Collections;
using System.Diagnostics;
using Nanoforge.Misc;
using Nanoforge.Math;
using Nanoforge.Gui;
using Nanoforge.App;
using Nanoforge;
using Direct3D;
using System;
using ImGui;
using Win32;

//Uncomment to enable more debug features. Disabled by default due to (sometimes major) performance penalties
//#define ENABLE_D3D11_DEBUG_FEATURES

namespace Nanoforge.Render
{
	[System]
	public class Renderer : ISystem
	{
		public Vec4<f32> ClearColor = .(0.0f, 0.0f, 0.0f, 1.0f);
        public ID3D11Device* Device => _device;
        public ID3D11DeviceContext* Context => _context;
        public List<Scene> Scenes = new .() ~delete _;

        private ID3D11Device* _device = null;
        private ID3D11DeviceContext* _context = null;
        private IDXGIFactory* _dxgiFactory = null;
        private IDXGISwapChain* _swapchain = null;
        private ID3D11RenderTargetView* _renderTargetView = null;
        private ID3D11BlendState* _blendState = null;
        private ID3D11RasterizerState* _rasterizerState = null;
        private ID3D11DepthStencilState* _depthStencilState = null;
        private D3D11_VIEWPORT _viewport = .();
        private HWND _hwnd = 0;
		private u32 _drawCount = 0;
		private f32 _totalTime = 0.0f;
        private i32 _windowWidth = 0;
        private i32 _windowHeight = 0;
        private bool _initialized = true;

        public ~this()
        {
            if (!_initialized)
                return;

            //Shutdown dear imgui
            //ImGui_ImplDX11_Shutdown();
            //ImGui_ImplWin32_Shutdown();
            //ImGui::DestroyContext();

            ReleaseCOM!(_swapchain);
            ReleaseCOM!(_device);
            ReleaseCOM!(_context);
            ReleaseCOM!(_dxgiFactory);
        }

		static void ISystem.Build(App app)
		{
			
		}

		[SystemInit]
		public void Init(App app)
		{
            Window window = app.GetResource<Window>();
            _windowWidth = window.Width;
            _windowHeight = window.Height;
            _hwnd = window.Handle;

            if (!InitD3D11())
                Runtime.FatalError("Failed to initialize D3D11");

            Events.Listen<WindowResizeEvent>(new => OnWindowResize);
			InitImGui(app.GetResource<BuildConfig>(), app.GetResource<Window>());
            _initialized = true;
		}

		[SystemStage(.BeginFrame)]
		public void BeginFrame(App app)
		{
			ImGuiBegin();
		}

		[SystemStage(.Update)]
		public void Update(App app)
		{
            
		}

		[SystemStage(.EndFrame)]
		public void EndFrame(App app)
		{
            FrameData frameData = app.GetResource<FrameData>();

            _context.OMSetDepthStencilState(_depthStencilState, 0);
            _context.OMSetBlendState(_blendState, null, 0xFFFFFFFF);
            _context.RSSetState(_rasterizerState);

            for (Scene scene in Scenes)
                if (scene.NeedsRedraw)
	                scene.Draw(frameData.DeltaTime);

            _context.OMSetRenderTargets(1, &_renderTargetView, null);
            _context.ClearRenderTargetView(_renderTargetView, &ClearColor.x);
            _context.RSSetViewports(1, &_viewport);
            //ImGuiDoFrame();

            _swapchain.Present(0, 0);

            ImGuiEnd();
		}

		private void ImGuiBegin()
		{

		}

		private void ImGuiEnd()
		{
			/*ImGui.Render();
			ImGuiImplOpenGL3.RenderDrawData(ImGui.GetDrawData());

			//Clear font texture data after a few frames. Uses a ton of memory.
			//Cleared after 60 frames since that ensures the font atlas was built and sent to the gpu so we can delete the cpu-side copy.
#if !DEBUG //Don't do it in debug builds because it causes a crash
			if (_drawCount < 60)
			{
				if (_drawCount == 59)
				{
					var io = ImGui.GetIO();
					io.Fonts.ClearTexData();
				}

				_drawCount++;
			}
#endif*/
		}

        private void OnWindowResize(ref WindowResizeEvent event)
        {
            if (_swapchain == null || _renderTargetView == null)
                return;

            _windowWidth = event.Width;
            _windowHeight = event.Height;

            //Recreate swapchain and render target view
            ReleaseCOM!(_swapchain);
            if (!InitSwapchainAndResources())
                Runtime.FatalError("Failed to create swapchain following window resize");

            //TODO: **************************************************************************************** RE-ENABLE when imgui is added *******************************************************************
            //ImGui.GetIO().DisplaySize = .(_windowWidth, _windowHeight);
        }

        private bool InitD3D11()
        {
            return CreateDevice() && InitSwapchainAndResources();
        }

        private bool CreateDevice()
        {
            D3D11_CREATE_DEVICE_FLAG createDeviceFlags = 0;
#if ENABLE_D3D11_DEBUG_FEATURES
            createDeviceFlags |= .DEBUG;
#endif
            D3D_FEATURE_LEVEL featureLevel = 0;
            DxRequired!(D3D11.D3D11CreateDevice(null, .HARDWARE, 0, createDeviceFlags, null, 0, D3D11.D3D11_SDK_VERSION, &_device, &featureLevel, &_context), "Failed to create ID3D11Device");
            if (featureLevel != ._11_0) //Note: To use versions greater than 11.0 we'll need to specify it in an array and pass that to create device. See pFeatureLevels here: https://docs.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-d3d11createdevice#parameters
                Runtime.FatalError("Minimum Direct3D 11 feature level (11.0) is not supported by this device");

            //Get DXGI factory for later use
            return AcquireDxgiFactoryInstance();
        }

        private bool InitSwapchainAndResources()
        {
            bool swapchainCreateResult = CreateSwapchain() && CreateRenderTargetView();
            if (!swapchainCreateResult)
                return false;

            //Viewport
            _viewport.TopLeftX = 0.0f;
            _viewport.TopLeftY = 0.0f;
            _viewport.Width = _windowWidth;
            _viewport.Height = _windowHeight;
            _viewport.MinDepth = 0.0f;
            _viewport.MaxDepth = 1.0f;

            //Blending
            D3D11_BLEND_DESC blend = .();
            ZeroMemory(&blend);
            blend.RenderTarget[0].BlendEnable = 1;
            blend.RenderTarget[0].SrcBlend = .SRC_COLOR;
            blend.RenderTarget[0].DestBlend = .BLEND_FACTOR;
            blend.RenderTarget[0].BlendOp = .ADD;
            blend.RenderTarget[0].SrcBlendAlpha = .ONE;
            blend.RenderTarget[0].DestBlendAlpha = .ZERO;
            blend.RenderTarget[0].BlendOpAlpha = .ADD;
            blend.RenderTarget[0].RenderTargetWriteMask = (u8)D3D11_COLOR_WRITE_ENABLE.ALL;
            DxRequired!(_device.CreateBlendState(blend, &_blendState), "Failed to create ID3D11BlendState");

            //Rasterizer
            D3D11_RASTERIZER_DESC rasterizer = .();
            ZeroMemory(&rasterizer);
            rasterizer.FillMode = .SOLID;
            rasterizer.CullMode = .BACK;
            rasterizer.FrontCounterClockwise = 0;
            rasterizer.DepthBias = 0;
            rasterizer.DepthBiasClamp = 0;
            rasterizer.SlopeScaledDepthBias = 0;
            rasterizer.DepthClipEnable = 1;
            rasterizer.ScissorEnable = 0;
            rasterizer.MultisampleEnable = 0;
            rasterizer.AntialiasedLineEnable = 0;
            DxRequired!(_device.CreateRasterizerState(rasterizer, &_rasterizerState), "Failed to create ID3D11RasterizerState");

            return true;
        }

        private bool CreateSwapchain()
        {
            //Describe swapchain
            DXGI_SWAP_CHAIN_DESC desc = .();
            desc.BufferDesc.Width = (u32)_windowWidth;
            desc.BufferDesc.Height = (u32)_windowHeight;
            desc.BufferDesc.RefreshRate.Numerator = 60; //TODO: Make this configurable
            desc.BufferDesc.RefreshRate.Denominator = 1;
            desc.BufferDesc.Format = .R8G8B8A8_UNORM;
            desc.BufferDesc.ScanlineOrdering = .UNSPECIFIED;
            desc.BufferDesc.Scaling = .UNSPECIFIED;
            desc.BufferUsage = DXGI.DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 1;
            desc.OutputWindow = _hwnd;
            desc.Windowed = 1;
            desc.SwapEffect = .DISCARD;
            desc.Flags = 0;
            desc.SampleDesc.Count = 1; //Don't use MSAA
            desc.SampleDesc.Quality = 0;

            //Create swapchain
            DxRequired!(_dxgiFactory.CreateSwapChain(ref *_device, ref desc, out _swapchain), "Failed to create IDXGISwapChain");

            return true;
        }

        private bool CreateRenderTargetView()
        {
            //We want a view on the swapchains backbuffer
            ID3D11Texture2D* backbuffer = null;
            _swapchain.GetBuffer(0, ID3D11Texture2D.IID, (void**)&backbuffer);
            if (backbuffer == null)
                Runtime.FatalError("Failed to get swapchain backbuffer for ID3D11RenderTargetView creation");

            //Create primary render target view
            DxRequired!(_device.CreateRenderTargetView(ref *backbuffer, null, &_renderTargetView), "Failed to create ID3D11RenderTargetView");

            //Release backbuffer reference
            ReleaseCOM!(backbuffer);
            return true;
        }

        //Get IDXGIFactory instance from device. Used to create other objects like the swapchain
        private bool AcquireDxgiFactoryInstance()
        {
            //Temporary references needed to get factory
            IDXGIDevice* dxgiDevice = null;
            IDXGIAdapter* dxgiAdapter = null;

            //Get factory
            DxRequired!(_device.QueryInterface(IDXGIDevice.IID, (void**)&dxgiDevice), "Failed to create IDXGIDevice");
            DxRequired!(dxgiDevice.GetParent(IDXGIAdapter.IID, (void**)&dxgiAdapter), "Failed to create IDXGIAdapter");
            DxRequired!(dxgiAdapter.GetParent(IDXGIFactory.IID, (void**)&_dxgiFactory), "Failed to create IDXGIFactory");

            //Release references
	        ReleaseCOM!(dxgiDevice);
	        ReleaseCOM!(dxgiAdapter);
            return true;
        }

		private void InitImGui(BuildConfig buildConfig, Window window)
		{
			/*//Init and configure imgui
			ImGui.CHECKVERSION();
			ImGui.CreateContext();

            ImGui_ImplWin32_EnableDpiAwareness();
            //TODO: Implement win32 & dx11 imgui backends in beef by looking at C++ example versions of them

			var io = ImGui.GetIO();
			io.DisplaySize = .(window.Width, window.Height);
			io.DisplayFramebufferScale = .(1.0f, 1.0f);
			io.ConfigFlags |= ImGui.ConfigFlags.NavEnableKeyboard;
			io.ConfigFlags |= ImGui.ConfigFlags.DockingEnable;

			/*//TODO: Have Mirror and imgui use the same version of GLFW
			ImGuiImplGlfw.InitForOpenGL(window.Base, true);
			ImGuiImplOpenGL3.Init("#version 130");*/

			//Set dark theme colors and style
			ImGui.StyleColorsDark();
			var style = ImGui.GetStyle();

			style.WindowPadding = .(8.0f, 8.0f);
			style.WindowRounding = 0.0f;
			style.FramePadding = .(5.0f, 5.0f);
			style.FrameRounding = 0.0f;
			style.ItemSpacing = .(8.0f, 8.0f);
			style.ItemInnerSpacing = .(8.0f, 6.0f);
			style.IndentSpacing = 25.0f;
			style.ScrollbarSize = 18.0f;
			style.ScrollbarRounding = 0.0f;
			style.GrabMinSize = 12.0f;
			style.GrabRounding = 0.0f;
			style.TabRounding = 0.0f;
			style.ChildRounding = 0.0f;
			style.PopupRounding = 0.0f;
			style.WindowBorderSize = 1.0f;
			style.FrameBorderSize = 1.0f;
			style.PopupBorderSize = 1.0f;

			style.Colors[(int)ImGui.Col.Text] = .(0.96f, 0.96f, 0.99f, 1.00f);
			style.Colors[(int)ImGui.Col.TextDisabled] = .(0.50f, 0.50f, 0.50f, 1.00f);
			style.Colors[(int)ImGui.Col.WindowBg] = .(0.114f, 0.114f, 0.125f, 1.0f);
			style.Colors[(int)ImGui.Col.ChildBg] = .(0.106f, 0.106f, 0.118f, 1.0f);
			style.Colors[(int)ImGui.Col.PopupBg] = .(0.06f, 0.06f, 0.07f, 1.00f);
			style.Colors[(int)ImGui.Col.Border] = .(0.216f, 0.216f, 0.216f, 1.0f);
			style.Colors[(int)ImGui.Col.BorderShadow] = .(0.00f, 0.00f, 0.00f, 0.00f);
			style.Colors[(int)ImGui.Col.FrameBg] = .(0.161f, 0.161f, 0.176f, 1.00f);
			style.Colors[(int)ImGui.Col.FrameBgHovered] = .(0.216f, 0.216f, 0.235f, 1.00f);
			style.Colors[(int)ImGui.Col.FrameBgActive] = .(0.255f, 0.255f, 0.275f, 1.00f);
			style.Colors[(int)ImGui.Col.TitleBg] = .(0.157f, 0.157f, 0.157f, 1.0f);
			style.Colors[(int)ImGui.Col.TitleBgActive] = .(0.216f, 0.216f, 0.216f, 1.0f);
			style.Colors[(int)ImGui.Col.TitleBgCollapsed] = .(0.157f, 0.157f, 0.157f, 1.0f);
			style.Colors[(int)ImGui.Col.MenuBarBg] = .(0.157f, 0.157f, 0.157f, 1.0f);
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
			style.Colors[(int)ImGui.Col.Tab] = .(0.21f, 0.21f, 0.24f, 1.00f);
			style.Colors[(int)ImGui.Col.TabHovered] = .(0.23f, 0.514f, 0.863f, 1.0f);
			style.Colors[(int)ImGui.Col.TabActive] = .(0.23f, 0.514f, 0.863f, 1.0f);
			style.Colors[(int)ImGui.Col.TabUnfocused] = .(0.21f, 0.21f, 0.24f, 1.00f);
			style.Colors[(int)ImGui.Col.TabUnfocusedActive] = .(0.23f, 0.514f, 0.863f, 1.0f);
			style.Colors[(int)ImGui.Col.DockingPreview] = .(0.23f, 0.514f, 0.863f, 0.776f);
			style.Colors[(int)ImGui.Col.DockingEmptyBg] = .(0.114f, 0.114f, 0.125f, 1.0f);
			style.Colors[(int)ImGui.Col.PlotLines] = .(0.96f, 0.96f, 0.99f, 1.00f);
			style.Colors[(int)ImGui.Col.PlotLinesHovered] = .(0.12f, 1.00f, 0.12f, 1.00f);
			style.Colors[(int)ImGui.Col.PlotHistogram] = .(0.23f, 0.51f, 0.86f, 1.00f);
			style.Colors[(int)ImGui.Col.PlotHistogramHovered] = .(0.12f, 1.00f, 0.12f, 1.00f);
			style.Colors[(int)ImGui.Col.TextSelectedBg] = .(0.26f, 0.59f, 0.98f, 0.35f);
			style.Colors[(int)ImGui.Col.DragDropTarget] = .(0.91f, 0.62f, 0.00f, 1.00f);
			style.Colors[(int)ImGui.Col.NavHighlight] = .(0.26f, 0.59f, 0.98f, 1.00f);
			style.Colors[(int)ImGui.Col.NavWindowingHighlight] = .(1.00f, 1.00f, 1.00f, 0.70f);
			style.Colors[(int)ImGui.Col.NavWindowingDimBg] = .(0.80f, 0.80f, 0.80f, 0.20f);
			style.Colors[(int)ImGui.Col.ModalWindowDimBg] = .(0.80f, 0.80f, 0.80f, 0.35f);

			FontManager.RegisterFonts(buildConfig);*/
		}
	}
}