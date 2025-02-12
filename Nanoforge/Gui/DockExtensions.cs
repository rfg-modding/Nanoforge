using System.Collections.Generic;
using Dock.Model.Core;
using Dock.Model.Mvvm.Core;

namespace Nanoforge.Gui;

public static class DockExtensions
{
    public static List<IDockable> GetChildrenRecursive(this DockBase parent)
    {
        List<IDockable> children = new List<IDockable>();
        DockBase currentParent = parent;
        GetDockChildrenRecursive(parent, children);
        return children;
    }

    private static void GetDockChildrenRecursive(DockBase parent, List<IDockable> children)
    {
        if (parent.VisibleDockables == null)
            return;
        
        foreach (IDockable child in parent.VisibleDockables)
        {
            children.Add(child);
            if (child is DockBase childDock)
            {
                GetDockChildrenRecursive(childDock, children);
            }
        }
    }
}