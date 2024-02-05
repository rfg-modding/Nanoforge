using Common;
using System;
using Nanoforge.App;
using ImGui;
using System.Linq;

namespace Nanoforge.Gui.Dialogs
{
    //Confirms that the user wants to close documents with unsaved changes
	public class SaveConfirmationDialog : Dialog
	{
        public bool UserChoseDontSave = false;

        public this() : base("Save?")
        {
            DisableKeybindsWhileOpen = true;
        }

        public void Show()
        {
            Open = true;
            _firstDraw = true;
            Result = .None;
        }

        public override void Draw(App app, Gui gui)
        {
            if (Open)
                ImGui.OpenPopup(Title);
            else
                return;

            int numUnsavedDocs = gui.NumUnsavedDocuments;
            if (gui.[Friend]_closeNanoforgeRequested && numUnsavedDocs == 0)
            {
                app.Exit = true; //Signal to App it's ok to close the window (any unsaved changes were saved or cancelled)
            }

            if (numUnsavedDocs == 0)
            {
                Close();
                return;
            }

            defer { _firstDraw = false; }

            //Manually set padding since the parent window might be a document with padding disabled
            ImGui.PushStyleVar(.WindowPadding, .(8.0f, 8.0f));

            //Auto center
            ImGui.IO* io = ImGui.GetIO();
            ImGui.SetNextWindowPos(.(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), .Always, .(0.5f, 0.5f));

            if (ImGui.BeginPopupModal(Title, null, .AlwaysAutoResize))
            {
                //List of files with unsaved changes
                ImGui.Text("Save changes to the following file(s)?");
                ImGui.PushStyleColor(.Text, ImGui.Red);
                ImGui.TextWrapped("WARNING! Nanoforge can't save or discard changes per map at the moment. So if you click 'Save' or 'Don't save' it'll apply that to ALL MAPS you have open with unsaved changes.");
                ImGui.TextWrapped("Per map saving & discarding will be added in a future release.");
                ImGui.PopStyleColor();
                f32 itemHeight = ImGui.GetTextLineHeightWithSpacing();
                if (ImGui.BeginChildFrame(ImGui.GetID("FilesWithUnsavedChanges"), .(-f32.Epsilon, 6.25f * itemHeight)))
                {
                    for (GuiDocumentBase doc in gui.Documents)
                    {
                        if (!doc.Open && doc.UnsavedChanges)
                        {
                            ImGui.Text(doc.Title);
                        }
                    }
                    ImGui.EndChildFrame();
                }

                ImGui.Vec2 buttonSize = .(ImGui.GetFontSize() * 7.0f, 0.0f);
                if (ImGui.Button("Save", buttonSize))
                {
                    gui.SaveAll(app);
                    Close();
                }

                ImGui.SameLine();
                if (ImGui.Button("Don't save", buttonSize))
                {
                    for (GuiDocumentBase doc in gui.Documents)
                    {
                        if (!doc.Open && doc.UnsavedChanges)
                        {
                            doc.UnsavedChanges = false;
                        }
                    }
                    Close();

                    UserChoseDontSave = true;
                }

                ImGui.SameLine();
                if (ImGui.Button("Cancel", buttonSize))
                {
                    for (GuiDocumentBase doc in gui.Documents)
                    {
                        if (!doc.Open && doc.UnsavedChanges)
                        {
                            doc.Open = true;
                        }
                    }

                    //Cancel any pending actions waiting that're waiting for unsaved changes to be handled
                    gui.[Friend]_closeNanoforgeRequested = false;
                    gui.[Friend]_createProjectRequested = false;
                    gui.[Friend]_openProjectRequested = false;
                    gui.[Friend]_closeProjectRequested = false;

                    //Cancel app close if thats in progress
                    if (app.Exit)
                    {
                        app.Exit = false;
                    }

                    Close();
                }

                ImGui.EndPopup();
            }
            ImGui.PopStyleVar();
        }

        public override void Close(DialogResult result = .None)
        {
            Open = false;
            Result = result;
            _firstDraw = true;
            ImGui.CloseCurrentPopup();
        }
	}
}