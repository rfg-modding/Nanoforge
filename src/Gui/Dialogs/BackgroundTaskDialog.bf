using Common;
using System;
using Nanoforge.App;
using ImGui;
using System.Collections;
using Common.Math;
using Nanoforge.Misc;
using System.Threading;

static
{
    public static Nanoforge.Gui.Dialogs.BackgroundTaskDialog gTaskDialog = new .() ~delete _;
}

namespace Nanoforge.Gui.Dialogs
{
    //Modal dialog that's meant to be shown whenever a threaded background task is running. Intended to block all user input and keybinds while running to prevent any accidental data races.
    //This is part of the new approach to threading being taken for the rewrite initially. Try to use threading cautiously and sparingly.
	//Don't want users to click the map editor or hit keybinds mid load/save/import/export and cause data to get touched in the middle of the process. 
	public class BackgroundTaskDialog : Dialog 
	{
        public bool CanClose = false;
        private append String _status;
        private int _numSteps = 1;
        private int _step = 0;
        private append List<String> _statusLog ~ClearAndDeleteItems(_);
        private append Monitor _statusLogLock;
        private bool _scrollLogToBottom = false;

        public this() : base("Task in progress...")
        {

        }

        public void Step()
        {
            _step++;
        }

        public void SetStatus(StringView status)
        {
            _status.Set(status);
            _status.EnsureNullTerminator();

            if (!_status.IsEmpty)
	        {
                ScopedLock!(_statusLogLock);
				_statusLog.Add(new String(_status));
                _scrollLogToBottom = true;
			}
        }

        //Log additional info like errors or warnings
        public void Log(StringView message)
        {
            ScopedLock!(_statusLogLock);
            _statusLog.Add(new String(message));
            _scrollLogToBottom = true;
        }

        public void Show(int numSteps)
        {
            if (Open)
                return;

            CanClose = false;
            Open = true;
            _firstDraw = true;
            Result = .None;
            _status.Set("");
            _status.EnsureNullTerminator();
            _statusLog.ClearAndDeleteItems();
            _numSteps = numSteps;
            _step = 0;
            Input.KeysEnabled = false;
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

            f32 percentageCompleted = ((f32)_step / (f32)_numSteps);
            if (ImGui.BeginPopupModal(Title, null, .AlwaysAutoResize))
            {
                ImGui.TextWrapped(_status);
                ImGui.ProgressBar(percentageCompleted, .(400.0f, 0.0f));

                ScopedLock!(_statusLogLock);
                if (_statusLog.Count > 0)
                {
                    f32 itemHeight = ImGui.GetTextLineHeightWithSpacing();
                    if (ImGui.BeginChildFrame(ImGui.GetID("StatusLog"), .(-f32.Epsilon, 4.25f * itemHeight)))
                    {
                        //Set custom highlight colors for the table
                        Vec4 selectedColor = .(0.157f, 0.350f, 0.588f, 1.0f);
                        Vec4 highlightedColor = selectedColor * 1.1f;
                        ImGui.PushStyleColor(.Header, selectedColor);
                        ImGui.PushStyleColor(.HeaderHovered, highlightedColor);
                        ImGui.PushStyleColor(.HeaderActive, highlightedColor);
                        for (String path in _statusLog)
                        {
                            if (ImGui.Selectable(path))
                            {

                            }
                        }
                        if (_scrollLogToBottom)
                        {
                            ImGui.SetScrollHereY(1.0f);
                            _scrollLogToBottom = false;
                        }
                        ImGui.PopStyleColor(3);
                        ImGui.EndChildFrame();
                    }
                }

                using (ImGui.DisabledBlock(!CanClose))
                {
                    if (ImGui.Button("Close"))
                    {
                        Close();
                    }
                }

                ImGui.EndPopup();
            }
            ImGui.PopStyleVar();
        }

        public override void Close(Nanoforge.Gui.DialogResult result = .None)
        {
            Open = false;
            Result = result;
            _firstDraw = true;
            ImGui.CloseCurrentPopup();
            Input.KeysEnabled = true;
        }
	}
}