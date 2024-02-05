using Common;
using System;
using Nanoforge.App;
using ImGui;

namespace Nanoforge.Gui
{
    //Wrapper around Dear ImGui popups. They're a pain to use manually since you have to manually create state variables and add draw code somewhere that'll get drawn every frame.
    //They can be made much easier using this class + some plumbing in Gui.bf to automatically call their drawn functions.
    public class Dialog
    {
        public append String Title;
        public bool Open = false;
        public DialogResult Result { get; private set; } = .None;
        protected bool _firstDraw = true;

        //Lets dialogs disable keybinds while open. For example, you don't want people hitting keybinds to copy/delete/etc objects while you're in the unsaved changes confirmation dialog or while a blocking task is running.
        //This only disable keybinds for code that uses the Input class. Which all NF code should be using with the exception of Dear ImGui code which gets window events directly from Window. We want that so our UI still works.
        public bool DisableKeybindsWhileOpen { get; private set; } = true;

        public this(StringView title)
        {
            Title.Set(title);
        }

        public virtual void Draw(App app, Gui gui)
        {

        }

        public virtual void Close(DialogResult result = .None)
        {

        }
    }

    public enum DialogResult
    {
        None,
        Ok,
        Cancel,
        Yes,
        No
    }

    public struct RegisterDialogAttribute : Attribute
    {

    }
}