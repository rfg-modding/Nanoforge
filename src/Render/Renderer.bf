using Nanoforge.Render.Resources;
using Nanoforge.Render.ImGui;
using System.Collections;
using System.Diagnostics;
using Nanoforge.Misc;
using Nanoforge.Gui;
using Nanoforge.App;
using Common.Math;
using Direct3D;
using Common;
using System;
using ImGui;
using Win32;

namespace Nanoforge.Render
{
	[System]
	public class Renderer : ISystem
	{
		public Vec4 ClearColor = .(0.0f, 0.0f, 0.0f, 1.0f);
        public ID3D11Device* Device => _device;
        public ID3D11DeviceContext* Context => _context;
        public List<Scene> Scenes = new .() ~DeleteContainerAndItems!(_);

        private append ImGuiRenderer _imguiRenderer;
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

            _imguiRenderer.Shutdown();
            ReleaseCOM(&_swapchain);
            ReleaseCOM(&_device);
            ReleaseCOM(&_context);
            ReleaseCOM(&_dxgiFactory);
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
            if (!_imguiRenderer.Init(app, _context, _device, _dxgiFactory))
                Runtime.FatalError("Failed to initialize dear imgui backend.");

            window.Maximize();
            RenderMaterials.Init(_device, _context);
            _initialized = true;
		}

		[SystemStage(.BeginFrame)]
		public void BeginFrame(App app)
		{
            _imguiRenderer.BeginFrame(app);
		}

		[SystemStage(.Update)]
		public void Update(App app)
		{
            for (Scene scene in Scenes)
                scene.Camera.Update(app);
		}

		[SystemStage(.EndFrame)]
		public void EndFrame(App app)
		{
            FrameData frameData = app.GetResource<FrameData>();

            _context.OMSetDepthStencilState(_depthStencilState, 0);
            _context.OMSetBlendState(_blendState, null, 0xFFFFFFFF);
            _context.RSSetState(_rasterizerState);

            for (Scene scene in Scenes)
                if (scene.Active)
	                scene.Draw(frameData.DeltaTime);

            _context.OMSetRenderTargets(1, &_renderTargetView, null);
            _context.ClearRenderTargetView(_renderTargetView, &ClearColor.x);
            _context.RSSetViewports(1, &_viewport);
            _imguiRenderer.Render(app);

            _swapchain.Present(0, 0);
            _imguiRenderer.EndFrame(app);
		}

        private void OnWindowResize(ref WindowResizeEvent event)
        {
            if (_swapchain == null || _renderTargetView == null)
                return;

            _windowWidth = event.Width;
            _windowHeight = event.Height;

            //Recreate swapchain and render target view
            ReleaseCOM(&_swapchain);
            if (!InitSwapchainAndResources())
                Runtime.FatalError("Failed to create swapchain following window resize");
        }

        public Scene CreateScene(bool active = true)
        {
            Scene scene = new .(_device, _context);
            scene.Active = active;
            Scenes.Add(scene);
            return scene;
        }

        public void DestroyScene(Scene scene)
        {
            Scenes.Remove(scene);
            delete scene;
        }

        private bool InitD3D11()
        {
            return CreateDevice() && InitSwapchainAndResources();
        }

        private bool CreateDevice()
        {
            D3D11_CREATE_DEVICE_FLAG createDeviceFlags = 0;
            if (BuildConfig.EnableD3D11DebugFeatures)
            {
                createDeviceFlags |= .DEBUG;
            }

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
            blend.RenderTarget[0].BlendEnable = 0;
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
            DxRequired!(_device.CreateRasterizerState(&rasterizer, &_rasterizerState), "Failed to create ID3D11RasterizerState");

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
            DxRequired!(_device.CreateRenderTargetView(backbuffer, null, &_renderTargetView), "Failed to create ID3D11RenderTargetView");

            //Release backbuffer reference
            ReleaseCOM(&backbuffer);
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
	        ReleaseCOM(&dxgiDevice);
	        ReleaseCOM(&dxgiAdapter);
            return true;
        }
	}
}