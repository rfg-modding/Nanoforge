using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Data.Core.Plugins;
using Avalonia.Markup.Xaml;
using Microsoft.Extensions.DependencyInjection;
using Nanoforge.Gui.Themes;
using Nanoforge.Gui.Views;
using Nanoforge.Render;
using Nanoforge.Services;
using MainWindowViewModel = Nanoforge.Gui.ViewModels.MainWindowViewModel;

namespace Nanoforge;

public partial class App : Application
{
    public readonly Renderer? Renderer;
    
    public static IThemeManager? ThemeManager;

    public new static App Current => (App)Application.Current!;

    public ServiceProvider Services { get; }

    private static IEnumerable<Window> Windows => (Application.Current?.ApplicationLifetime as IClassicDesktopStyleApplicationLifetime)?.Windows ?? Array.Empty<Window>();
    
    public App()
    {
        //Don't try starting the renderer if we're in the designer. Breaks the preview.
        if (!Design.IsDesignMode)
        {
            Renderer = new Renderer(1920, 1080);
        }
        Services = ConfigureServices();
    }
    
    public override void Initialize()
    {
        ThemeManager = new FluentThemeManager();
        ThemeManager.Initialize(this);

        AvaloniaXamlLoader.Load(this);
        
        //TODO: Make this user configurable
        //Default to dark theme for now
        ThemeManager.Switch(1);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        // DockManager.s_enableSplitToWindow = true;

        var mainWindowViewModel = new MainWindowViewModel();

        switch (ApplicationLifetime)
        {
            case IClassicDesktopStyleApplicationLifetime desktopLifetime:
            {
                // Line below is needed to remove Avalonia data validation.
                // Without this line you will get duplicate validations from both Avalonia and CT
                BindingPlugins.DataValidators.RemoveAt(0);
                var mainWindow = new MainWindow
                {
                    DataContext = mainWindowViewModel
                };

                mainWindow.Closing += (_, _) =>
                {
                    mainWindowViewModel.CloseLayout();
                };

                desktopLifetime.MainWindow = mainWindow;

                desktopLifetime.Exit += (_, _) =>
                {
                    mainWindowViewModel.CloseLayout();
                };
                    
                break;
            }
            case ISingleViewApplicationLifetime singleViewLifetime:
            {
                var mainView = new MainView()
                {
                    DataContext = mainWindowViewModel
                };

                singleViewLifetime.MainView = mainView;

                break;
            }
        }

        base.OnFrameworkInitializationCompleted();
#if DEBUG
        this.AttachDevTools();
#endif
    }

    private ServiceProvider ConfigureServices()
    {
        var services = new ServiceCollection();
        services.AddSingleton<IFileDialogService, FileDialogService>();
        return services.BuildServiceProvider();
    }

    public Window? FindWindowByViewModel(INotifyPropertyChanged viewModel)
    {
        return Windows.FirstOrDefault(x => ReferenceEquals(viewModel, x.DataContext));
    }
}