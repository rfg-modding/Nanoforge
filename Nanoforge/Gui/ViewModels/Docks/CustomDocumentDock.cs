using System;
using System.Diagnostics;
using CommunityToolkit.Mvvm.Input;
using Dock.Model.Controls;
using Dock.Model.Mvvm.Controls;
using Nanoforge.Gui.ViewModels.Documents;
using Serilog;

namespace Nanoforge.Gui.ViewModels.Docks;

public class CustomDocumentDock : DocumentDock
{
    public CustomDocumentDock()
    {
        CanCreateDocument = true;
    }

    public void AddNewDocument<T>(T document) where T : IDocument
    {
        var index = VisibleDockables?.Count + 1;
        document.Id = $"{document.Title}-{index}";
        
        Factory?.AddDockable(this, document);
        Factory?.SetActiveDockable(document);
        Factory?.SetFocusedDockable(this, document);
    }
}
