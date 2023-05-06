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
        public bool Open { get; private set; } = false;
        public DialogResult Result { get; private set; } = .None;
        protected bool _firstDraw = true;

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