using Common;
using System;
using Nanoforge.App;
using ImGui;

namespace Nanoforge.Gui.Dialogs
{
    public enum MessageBoxType
    {
        Ok,
        YesCancel
    }

    //Basic yes/no/ok message box
    public class MessageBox : Dialog
    {
        private append String _content;
        private MessageBoxType _type = .Ok;
        private delegate void(DialogResult result) _onClose = null ~DeleteIfSet!(_);

        public this(StringView title) : base(title)
        {
            DisableKeybindsWhileOpen = true;
        }

        public void Show(StringView content, MessageBoxType type, delegate void(DialogResult result) onClose = null)
        {
            Open = true;
            _content.Set(content);
            _type = type;
            _onClose = onClose;
            _firstDraw = true;
            Result = .None;
        }

        public override void Draw(App app, Gui gui)
        {
            if (Open)
                ImGui.OpenPopup(Title);
            else
                return;

            defer { _firstDraw = false; }

            //Manually set padding since the parent window might be a document with padding disabled
            ImGui.PushStyleVar(.WindowPadding, .(8.0f, 8.0f));

            //Auto center
            ImGui.IO* io = ImGui.GetIO();
            ImGui.SetNextWindowPos(.(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), .Always, .(0.5f, 0.5f));

            if (ImGui.BeginPopupModal(Title, null))
            {
                defer ImGui.EndPopup();

                ImGui.TextWrapped(_content);

                //Auto focus on default option on first frame so you don't need to switch to the mouse
                if (_firstDraw)
                    ImGui.SetKeyboardFocusHere();

                if (_type == .Ok)
                {
                    if (ImGui.Button("Ok") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
                    {
                        Close(.Ok);
                    }
                }
                else if (_type == .YesCancel)
                {
                    if (ImGui.Button("Yes") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
                    {
                        Close(.Yes);
                    }
                    ImGui.SameLine();
                    if (ImGui.Button("Cancel") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
                    {
                        Close(.Cancel);
                    }
                }
                else
                {
                    Close(.None);
                }
            }
            ImGui.PopStyleVar();
        }

        public override void Close(DialogResult result = .None)
        {
            Open = false;
            Result = result;
            _firstDraw = true;
            ImGui.CloseCurrentPopup();
            if (_onClose != null)
                _onClose(Result);
        }
    }
}