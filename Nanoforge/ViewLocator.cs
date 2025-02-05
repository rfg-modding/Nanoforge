using System;
using Avalonia.Controls;
using Avalonia.Controls.Templates;
using CommunityToolkit.Mvvm.ComponentModel;
using Dock.Model.Core;
using Nanoforge.Gui.ViewModels;

namespace Nanoforge;

public class ViewLocator : IDataTemplate
{
    public Control? Build(object? data)
    {
        if (data is null)
            return null;
        
        //TODO: Change this so it goes off the class name and ignores the namespace. I don't want to be forced to have the same folder structure for the View and ViewModel
        var name = data.GetType().FullName?.Replace("ViewModel", "View", StringComparison.Ordinal);
        if (name is null)
        {
            return new TextBlock { Text = "Invalid Data Type" };
        }
        var type = Type.GetType(name);

        if (type != null)
        {
            var control = (Control)Activator.CreateInstance(type)!;
            control.DataContext = data;
            return control;
        }

        return new TextBlock { Text = "Not Found: " + name };
    }

    public bool Match(object? data)
    {
        return data is ObservableObject || data is IDockable;
    }
}