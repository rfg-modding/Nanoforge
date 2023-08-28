using Common;
using System;
using Nanoforge.App;
using ImGui;
using System.Linq;
using NativeFileDialog;
using Nanoforge.Misc;
using System.IO;

namespace Nanoforge.Gui.Dialogs
{
    //Used to create new NF project
	public class NewProjectDialog : Dialog
	{
        private append String _name;
        private append String _path;
        private append String _description;
        private append String _author;
        private bool _createProjectFolder = true;
        private bool _pathNotSetError = false;
        private bool _showPathSelectorError = false;

        public this() : base("New project")
        {

        }

        public void Show()
        {
            Open = true;
            _firstDraw = true;
            Result = .None;
            _name.Set("");
            _description.Set("");
            //Path and author not emptied for convenience
            if (_path.IsEmpty)
            {
                _path.Set(CVar_GeneralSettings->NewProjectDirectory);
            }
            //_author.Set("");
            _showPathSelectorError = false;
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

            if (ImGui.BeginPopupModal(Title, &Open, .AlwaysAutoResize))
            {
                //Input textboxes
                ImGui.PushItemWidth(230.0f);
                ImGui.InputText("Name*", _name);
                ImGui.InputText("Author*", _author);
                ImGui.InputTextMultiline("Description", _description);
                ImGui.InputText("Path*", _path);
                ImGui.SameLine();
                if (ImGui.Button("..."))
                {
                    char8* outPath = null;
                    switch (NativeFileDialog.PickFolder(null, &outPath))
                    {
                        case .Okay:
                            _path.Set(StringView(outPath));
                            _showPathSelectorError = false;
                        case .Cancel:
                            _showPathSelectorError = false;
                        case .Error:
                            char8* error = NativeFileDialog.GetError();
                            Logger.Error("Error opening folder selector in new project dialog: {}", StringView(error));
                            _showPathSelectorError = true;
                    }
                }
                if (_showPathSelectorError)
                {
                    ImGui.SameLine();
                    ImGui.TextColored(.Red, "An error occurred. Check the log");
                }

                ImGui.Checkbox("Create project folder", &_createProjectFolder);

                //Detect if project can be created. Create button gets enabled if so.
                bool canCreateProject = true;
                char8* creationBlock = "";
                if (_name.IsEmpty || _name.IsWhiteSpace)
                {
					canCreateProject = false;
                    creationBlock = "Name must be set";
                }
                else if (_author.IsEmpty || _author.IsWhiteSpace)
                {
					canCreateProject = false;
                    creationBlock = "Author must be set";
                }
                else if (_path.IsEmpty || _path.IsWhiteSpace)
                {
                    canCreateProject = false;
                    creationBlock = "Path must be set";
                }
                else if (!Directory.Exists(_path))
                {
					canCreateProject = false;
                    creationBlock = "Path does not exist";
                }

                let disableCreateButton = !canCreateProject;
                if (disableCreateButton)
                    ImGui.BeginDisabled(disableCreateButton);

                if (ImGui.Button("Create")) //Create project from input
                {
                    //Create project directory if checked
                    String finalPath = _createProjectFolder ?
                        scope $"{_path}\\{_name}\\" :
                        scope $"{_path}\\";
                    if (_createProjectFolder)
                        Directory.CreateDirectory(finalPath);

                    ProjectDB.NewProject(finalPath, _name, _author, _description);
                    Close(.None);

                    CVar_GeneralSettings->NewProjectDirectory.Set(_path);
                    CVar_GeneralSettings.Save();
                }

                if (disableCreateButton)
                    ImGui.EndDisabled();

                if (!canCreateProject)
                {
                    ImGui.SameLine();
                    ImGui.TextColored(.Red, creationBlock);
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
        }
	}
}