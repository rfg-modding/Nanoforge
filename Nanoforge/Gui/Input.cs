using System.Collections.Generic;
using Avalonia.Input;

namespace Nanoforge.Gui;

//TODO: Add code IsKeyPressed() code to see if the key was pressed this frame. For actions that you only want to happen one time each time a key is pressed
public class Input
{
    private readonly HashSet<Key> _downKeys = new();

    public void SetKeyDown(Key k)
    {
        _downKeys.Add(k);
    }

    public void SetKeyUp(Key k)
    {
        _downKeys.Remove(k);
    }

    public bool IsKeyDown(Key k)
    {
        return _downKeys.Contains(k);    
    }
}