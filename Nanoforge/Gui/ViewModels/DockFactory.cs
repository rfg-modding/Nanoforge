using System;
using System.Collections.Generic;
using Dock.Avalonia.Controls;
using Dock.Model.Controls;
using Dock.Model.Core;
using Dock.Model.Mvvm;
using Dock.Model.Mvvm.Controls;
using Nanoforge.Gui.Models.Documents;
using Nanoforge.Gui.Models.Tools;
using Nanoforge.Gui.ViewModels.Docks;
using Nanoforge.Gui.ViewModels.Documents;
using Nanoforge.Gui.ViewModels.Pages;
using Nanoforge.Gui.ViewModels.Tools;
using Nanoforge.Gui.ViewModels.Tools.FileExplorer;

namespace Nanoforge.Gui.ViewModels;

public class DockFactory : Factory
{
    private IRootDock? _rootDock;
    public CustomDocumentDock? DocumentDock;
    public OutlinerViewModel? Outliner;
    public InspectorViewModel? Inspector;
    
    public override IDocumentDock CreateDocumentDock() => new CustomDocumentDock();

    public override IRootDock CreateLayout()
    {
        var fileExplorer = new FileExplorerViewModel { Id = "File explorer", Title = "File explorer" };
        Outliner = new OutlinerViewModel { Id = "Outliner", Title = "Outliner" };
        Inspector = new InspectorViewModel { Id = "Inspector", Title = "Inspector" };

        var leftDock = new ProportionalDock
        {
            Proportion = 0.20,
            Orientation = Orientation.Vertical,
            ActiveDockable = null,
            VisibleDockables = CreateList<IDockable>
            (
                new ToolDock
                {
                    ActiveDockable = fileExplorer,
                    VisibleDockables = CreateList<IDockable>(fileExplorer),
                    Alignment = Alignment.Left
                }
            )
        };
        
        var rightDock = new ProportionalDock
        {
            Proportion = 0.20,
            Orientation = Orientation.Vertical,
            ActiveDockable = null,
            VisibleDockables = CreateList<IDockable>
            (
                new ToolDock
                {
                    ActiveDockable = Outliner,
                    VisibleDockables = CreateList<IDockable>(Outliner),
                    Alignment = Alignment.Right,
                },
                new ProportionalDockSplitter(),
                new ToolDock
                {
                    ActiveDockable = Inspector,
                    VisibleDockables = CreateList<IDockable>(Inspector),
                    Alignment = Alignment.Right,
                }
            )
        };

        var documentDock = new CustomDocumentDock()
        {
            IsCollapsable = false,
            ActiveDockable = null,
            VisibleDockables = CreateList<IDockable>(),
            CanCreateDocument = false
        };

        var mainLayout = new ProportionalDock
        {
            Orientation = Orientation.Horizontal,
            VisibleDockables = CreateList<IDockable>
            (
                leftDock,
                new ProportionalDockSplitter(),
                documentDock,
                new ProportionalDockSplitter(),
                rightDock
            ),
        };

        //TODO: Make a separate home/start page view using this. It'll look nicer than having it be a document.
        //NOTE: This is something from avalonia dock library example. It had the ability to switch between different pages. Currently we just have the editor view.
        var editorView = new EditorViewModel
        {
            Id = "Editor",
            Title = "Editor",
            ActiveDockable = mainLayout,
            VisibleDockables = CreateList<IDockable>(mainLayout)
        };

        var rootDock = CreateRootDock();

        rootDock.IsCollapsable = false;
        rootDock.ActiveDockable = editorView;
        rootDock.DefaultDockable = editorView;
        rootDock.VisibleDockables = CreateList<IDockable>(editorView);

        DocumentDock = documentDock;
        _rootDock = rootDock;
        
        return rootDock;
    }

    public override IDockWindow? CreateWindowFrom(IDockable dockable)
    {
        var window = base.CreateWindowFrom(dockable);

        if (window != null)
        {
            window.Title = "Nanoforge";
        }

        return window;
    }

    public override void InitLayout(IDockable layout)
    {
        ContextLocator = new Dictionary<string, Func<object?>>
        {
            ["File explorer"] = () => new FileExplorer(),
            ["Outliner"] = () => new Outliner(),
            ["Inspector"] = () => new Inspector(),
            ["Editor"] = () => layout,
        };

        DockableLocator = new Dictionary<string, Func<IDockable?>>()
        {
            ["Root"] = () => _rootDock,
            ["Documents"] = () => DocumentDock
        };

        HostWindowLocator = new Dictionary<string, Func<IHostWindow?>>
        {
            [nameof(IDockWindow)] = () => new HostWindow()
        };

        base.InitLayout(layout);
    }

    public override void OnFocusedDockableChanged(IDockable? dockable)
    {
        base.OnFocusedDockableChanged(dockable);
        
        if (Inspector is null || Outliner is null)
            return;
        
        //Inspector tracks currently selected NF document so it knows what data to bind to its view
        if (dockable is NanoforgeDocument nfDoc)
        {
            Outliner.FocusedDocument = nfDoc;
            Inspector.FocusedDocument = nfDoc;
        }
        else
        {
            Outliner.FocusedDocument = null;
            Inspector.FocusedDocument = null;
        }
    }
}