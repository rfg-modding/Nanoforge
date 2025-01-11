using System;
using System.Numerics;
using System.Threading.Tasks;
using System.Windows.Input;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Rendering.Composition;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.Input;
using Nanoforge.FileSystem;
using Nanoforge.Render;
using Serilog;
using Vector = Avalonia.Vector;

namespace Nanoforge.Gui.Views.Controls;

public partial class Viewport3D : UserControl
{
    private Renderer? _renderer;
    private WriteableBitmap _rendererOutput;
    private Compositor? _compositor;
    
    private readonly DateTime _startTime = DateTime.Now;
    private DateTime _lastUpdate;
    
    public static readonly StyledProperty<Scene?> SceneProperty = AvaloniaProperty.Register<Viewport3D, Scene?>(nameof(Scene));

    public Scene? Scene
    {
        get => GetValue(SceneProperty);
        set => SetValue(SceneProperty, value);
    }

    public static readonly StyledProperty<ICommand> UpdateCommandProperty = AvaloniaProperty.Register<Viewport3D, ICommand>(nameof(UpdateCommand));

    public ICommand UpdateCommand
    {
        get => GetValue(UpdateCommandProperty);
        set => SetValue(UpdateCommandProperty, value);
    }

    public static readonly StyledProperty<IAsyncRelayCommand> SceneInitCommandProperty = AvaloniaProperty.Register<Viewport3D, IAsyncRelayCommand>(nameof(SceneInitCommand));

    public IAsyncRelayCommand SceneInitCommand
    {
        get => GetValue(SceneInitCommandProperty);
        set => SetValue(SceneInitCommandProperty, value);
    }

    public static readonly StyledProperty<bool> SceneInitializedProperty = AvaloniaProperty.Register<Viewport3D, bool>(nameof(SceneInitialized));

    public bool SceneInitialized
    {
        get => GetValue(SceneInitializedProperty);
        set => SetValue(SceneInitializedProperty, value);
    }

    public static readonly StyledProperty<string> LoadingStatusProperty = AvaloniaProperty.Register<Viewport3D, string>(nameof(LoadingStatus));

    public string LoadingStatus
    {
        get => GetValue(LoadingStatusProperty);
        set => SetValue(LoadingStatusProperty, value);
    }

    private bool _leftMouseButtonDown = false;
    private bool _rightMouseButtonDown = false;
    private Vector2 _lastMousePosition = Vector2.Zero;
    private Vector2 _mousePosition = Vector2.Zero;
    private Vector2 _mousePositionDelta = Vector2.Zero;
    private bool _mouseOverViewport = false;
    private bool _mouseMovedThisFrame = false;
    private bool _sceneInitStarted = false;
    private Task? _sceneInitTask;

    public Viewport3D()
    {
        InitializeComponent();

        if (Design.IsDesignMode)
        {
            LoadingStatus = "Loading scene...";
        }

        _renderer = (Application.Current as App)!.Renderer;
        _rendererOutput = new WriteableBitmap(new PixelSize(1920, 1080), new Vector(96, 96), PixelFormat.Bgra8888);

        if (_renderer != null)
        {
            var mainWindow = MainWindow.Instance;
            var selfVisual = ElementComposition.GetElementVisual(mainWindow)!;
            _compositor = selfVisual.Compositor;
        
            ViewportImage.Source = _rendererOutput;
            _lastUpdate = DateTime.Now;
        }
        else
        {
            Log.Error("Renderer is null. If you're not in the xaml designer and you see this then there's a big problem.");
        }
    }
    
    private void Control_OnLoaded(object? sender, RoutedEventArgs e)
    {
        Update();
    }

    private void Update()
    {
        RenderFrame();
        
        _compositor?.RequestCompositionUpdate(Update);
        Dispatcher.UIThread.Post(ViewportImage.InvalidateVisual, DispatcherPriority.Render);
    }

    private void RenderFrame()
    {
        if (_renderer == null)
            return;
        if (!PackfileVFS.Ready)
        {
            LoadingStatus = "Waiting for data folder to be mounted...";
            return;
        }

        if (!_sceneInitStarted)
        {
            LoadingStatus = "Loading scene...";
            _sceneInitTask = SceneInitCommand.ExecuteAsync(null);
            _sceneInitStarted = true;
            return;
        }
        if (_sceneInitStarted)
        {
            if (_sceneInitTask is { IsCompleted: true })
            {
                SceneInitialized = true;
                LoadingStatus = "Done loading.";
            }
            else
            {
                return;
            }
        }

        if (_mouseMovedThisFrame)
        {
            _mouseMovedThisFrame = false;
        }
        else
        {
            _lastMousePosition = _mousePosition;
            _mousePositionDelta = Vector2.Zero;
        }
        
        TimeSpan deltaTime = DateTime.Now - _lastUpdate;
        TimeSpan totalTime = DateTime.Now - _startTime;
        
        //Give the ViewModel an opportunity to update the scene
        SceneFrameUpdateParams updateParams = new((float)deltaTime.TotalSeconds, (float)totalTime.TotalSeconds, _leftMouseButtonDown, _rightMouseButtonDown, _mousePosition, _mousePositionDelta, _mouseOverViewport);
        UpdateCommand.Execute(updateParams);
        
        _renderer.RenderFrame(Scene!);

        using var buffer = _rendererOutput!.Lock();
        _renderer.GetRenderImage(buffer.Address);

        _lastUpdate = DateTime.Now;
    }
    
    private void InputElement_OnPointerMoved(object? sender, PointerEventArgs e)
    {
        Point position = e.GetPosition(sender as Control);
        _lastMousePosition = _mousePosition;
        _mousePosition = new Vector2((float)position.X, (float)position.Y);
        _mousePositionDelta = _mousePosition - _lastMousePosition;
        _mouseMovedThisFrame = true;
    }

    private void InputElement_OnPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        PointerPoint point = e.GetCurrentPoint(sender as Control);
        _leftMouseButtonDown = point.Properties.IsLeftButtonPressed;
        _rightMouseButtonDown = point.Properties.IsRightButtonPressed;
    }

    private void InputElement_OnPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        PointerPoint point = e.GetCurrentPoint(sender as Control);
        _leftMouseButtonDown = point.Properties.IsLeftButtonPressed;
        _rightMouseButtonDown = point.Properties.IsRightButtonPressed;
    }

    private void InputElement_OnPointerEntered(object? sender, PointerEventArgs e)
    {
        _mouseOverViewport = true;
    }

    private void InputElement_OnPointerExited(object? sender, PointerEventArgs e)
    {
        _mouseOverViewport = false;
    }
}