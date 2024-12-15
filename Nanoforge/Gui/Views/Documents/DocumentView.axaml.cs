using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Interactivity;
using Avalonia.Markup.Xaml;
using Avalonia.OpenGL;
using Avalonia.OpenGL.Controls;
using Avalonia.Rendering.Composition;
using Avalonia.Threading;
using Nanoforge.Render;

namespace Nanoforge.Gui.Views.Documents;

public partial class DocumentView : UserControl
{
    public DocumentView()
    {
        InitializeComponent();
        AvaloniaXamlLoader.Load(this);
    }

    private void InitializeComponent()
    {
        AvaloniaXamlLoader.Load(this);
    }
}

public class OpenGlPageControl : OpenGlControlBase
{
    private Renderer? _renderer;

    private int _frameCounter = 0;
    
    private DateTime _startTime = DateTime.Now;
    private DateTime _lastUpdate = DateTime.Now;
    
    protected override unsafe void OnOpenGlInit(GlInterface GL)
    {
        base.OnOpenGlInit(GL);
        _renderer = new(GL);
    }

    protected override void OnOpenGlDeinit(GlInterface GL)
    {
        _renderer?.Shutdown();
    }

    protected override unsafe void OnOpenGlRender(GlInterface gl, int fb)
    {
        float deltaTime = (float)(DateTime.Now - _lastUpdate).TotalSeconds;
        float totalSeconds = (float)(DateTime.Now - _startTime).TotalSeconds;
        _lastUpdate = DateTime.Now;
        
        _renderer?.RenderFrame(fb, Bounds, deltaTime, totalSeconds);
        Dispatcher.UIThread.Post(InvalidateVisual, DispatcherPriority.Background);
    }
}