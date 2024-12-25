using System;
using System.Linq;
using System.Numerics;
using System.Windows.Input;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Media.Imaging;
using Avalonia.OpenGL;
using Avalonia.OpenGL.Controls;
using Avalonia.Platform;
using Avalonia.Rendering.Composition;
using Avalonia.Threading;
using CommunityToolkit.Mvvm.ComponentModel;
using Nanoforge.Render;
using Nanoforge.Render.Resources;
using Serilog;
using Vector = Avalonia.Vector;

namespace Nanoforge.Gui.Views.Controls;

public partial class Viewport3D : UserControl
{
    private Renderer _renderer;
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
    
    public Viewport3D()
    {
        InitializeComponent();

        _renderer = (Application.Current as App)!.Renderer;
        _rendererOutput = new WriteableBitmap(new PixelSize(1920, 1080), new Vector(96, 96), PixelFormat.Bgra8888);
        
        var mainWindow = MainWindow.Instance;
        var selfVisual = ElementComposition.GetElementVisual(mainWindow)!;
        _compositor = selfVisual.Compositor;
        
        ViewportImage.Source = _rendererOutput;
        _lastUpdate = DateTime.Now;
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
        TimeSpan deltaTime = DateTime.Now - _lastUpdate;
        TimeSpan totalTime = DateTime.Now - _startTime;

        //Give the ViewModel an opportunity to update the scene
        FrameUpdateParams updateParams = new((float)deltaTime.TotalSeconds, (float)totalTime.TotalSeconds);
        UpdateCommand.Execute(updateParams);
        
        _renderer.RenderFrame(Scene!);

        using var buffer = _rendererOutput!.Lock();
        _renderer.GetRenderImage(buffer.Address);

        _lastUpdate = DateTime.Now;
    }
}

public struct FrameUpdateParams(float deltaTime, float totalTime)
{
    public readonly float DeltaTime = deltaTime;
    public readonly float TotalTime = totalTime;
}