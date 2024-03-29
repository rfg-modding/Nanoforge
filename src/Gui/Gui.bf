using System.Collections;
using Nanoforge.Gui.Panels;
using Common.Math;
using System.Threading;
using Nanoforge.App;
using System.Linq;
using Common;
using System;
using ImGui;
using Nanoforge.Misc;
using Nanoforge.Gui.Dialogs;
using NativeFileDialog;
using Nanoforge.FileSystem;
using System.IO;

namespace Nanoforge.Gui
{
	[System, ReflectAll]
	public class Gui : ISystem
	{
        public List<GuiPanelBase> Panels = new .() ~DeleteContainerAndItems!(_);
        public List<GuiDocumentBase> Documents = new .() ~DeleteContainerAndItems!(_);
        private append Dictionary<Object, List<Dialog>> _guiDialogs ~ClearDictionaryAndDeleteValues!(_); //Dialogs in panels and documents automatically get drawn as long as they have [RegisterPopup]
        public append Monitor DocumentLock;
        public MainMenuBar MainMenuBar = new MainMenuBar();
        public bool OutlinerOpen = true;
        public bool InspectorOpen = true;
        public String OutlinerIdentifier = new .(Icons.ICON_FA_LIST)..Append(" Outliner") ~delete _;
        public String InspectorIdentifier = new .(Icons.ICON_FA_WRENCH)..Append(" Inspector") ~delete _;
        public GuiDocumentBase FocusedDocument = null;

        public int NumUnsavedDocuments => Documents.Select((doc) => doc).Where((doc) => !doc.Open && doc.UnsavedChanges).Count();
        public bool CloseInProgress => _closeNanoforgeRequested;
        private bool _closeNanoforgeRequested = false;
        private bool _createProjectRequested = false;
        private bool _openProjectRequested = false;
        private bool _closeProjectRequested = false;
        private append String _pathOfProjectToOpen;

        [RegisterDialog]
        public append SaveConfirmationDialog SaveConfirmationDialog;
        [RegisterDialog]
        private append NewProjectDialog _newProjectDialog;
        [RegisterDialog]
        public append DataFolderSelectDialog DataFolderSelector;

        public bool ReadyForExit { get; private set; } = false;

		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
            AddPanel("", true, MainMenuBar);
#if DEBUG
            AddPanel("View/State viewer", true, new StateViewer());
#endif
            RegisterDialogs(this);

            //Hardcoded static instance of TaskDialog for global use
            _guiDialogs[this].Add(gTaskDialog);
		}

		[SystemStage(.Update)]
		void Update(App app)
		{
			for (GuiPanelBase panel in Panels)
            {
                if (!panel.Open)
                    continue;

                panel.Update(app, this);
                panel.FirstDraw = false;
            }
            DrawOutliner(app);
            DrawInspector(app);

            for (GuiDocumentBase document in Documents.ToList(.. scope .())) //Iterate temporary list to we can delete documents from main list while iterating
            {
                if (document.Open)
                {
                    //Optionally disable window padding for documents that need to flush with the window (map/mesh viewer viewports)
                    if (document.NoWindowPadding)
                        ImGui.PushStyleVar(.WindowPadding, .(0.0f, 0.0f));

                    ImGui.WindowFlags flags = .None;
                    if (document.UnsavedChanges)
                        flags |= .UnsavedDocument;
                    if (document.HasMenuBar)
                        flags |= .MenuBar;

                    //Draw document
                    ImGui.SetNextWindowDockID(MainMenuBar.DockspaceCentralNodeId, .Appearing);
                    ImGui.Begin(document.Title, &document.Open, flags);
                    if (ImGui.IsWindowFocused())
                        FocusedDocument = document;

                    document.Update(app, this);
                    document.FirstDraw = false;
                    ImGui.End();

                    if (document.NoWindowPadding)
                        ImGui.PopStyleVar();

                    //Call OnClose when the user clicks the close button
                    if (!document.Open)
                        document.OnClose(app, this);
                }
                else if (!document.Open && !document.UnsavedChanges && document.CanClose(app, this))
                {
                    //Remove references to dialogs owned by this document
                    List<Dialog> dialogs = _guiDialogs[document];
                    delete dialogs;
                    _guiDialogs.Remove(document);

                    //Erase the document if it was closed, has no unsaved changes, and is ready to close (not waiting for worker threads to exit)
                    Documents.Remove(document);
                    if (FocusedDocument == document)
                        FocusedDocument = null;

                    delete document;
                }
            }

            //Open save confirmation dialog if there's any docs closed with unsaved changes
            if (NumUnsavedDocuments > 0)
                SaveConfirmationDialog.Show();

            //Handle open/close/new project logic once any unsaved changes are handled. If canceled SaveConfirmationDialog will set all of these to false.
            if (!SaveConfirmationDialog.Open)
            {
                if (_createProjectRequested)
                {
                    _newProjectDialog.Show();
                    _createProjectRequested = false;
                }
                else if (_openProjectRequested)
                {
                    if (_pathOfProjectToOpen.IsEmpty)
                    {
                        TryOpenProject(); //Path of project to open wasn't specified. Ask the user to pick a project file
                        SaveConfirmationDialog.UserChoseDontSave = false; //Disable since we're already loading a different project
                    }
                    else
                    {
                        NanoDB.LoadAsync(_pathOfProjectToOpen);
                        SaveConfirmationDialog.UserChoseDontSave = false;
                    }
                    _pathOfProjectToOpen.Set("");
                    _openProjectRequested = false;
                }
                else if (_closeProjectRequested)
                {
                    NanoDB.Reset();
                    _closeProjectRequested = false;
                }
                else if (_closeNanoforgeRequested && !NanoDB.Saving)
                {
                    app.Exit = true;
                    ReadyForExit = true;
                }
                else if (SaveConfirmationDialog.UserChoseDontSave)
                {
                    for (var document in Documents)
                    {
                        document.UnsavedChanges = false;
                        document.Open = false;
                    }

                    //We're not creating a new project or opening another. So we must've just closed a map document and clicked "Don't Save". Force a project reload to discard the changes.
                    //Temporary code until we get proper undo/redo support. We're forced to reload the last saved copy of the project to discard changes made since the last save.
                    //Currently NF has no way to rollback changes for a single map or object, so if multiple maps have been edited we either need to save them all or discard them all.
                    //Two possible ways to fix this in future:
                    //    - Fix when undo/redo is added. It'll need a way to track and rollback changes per object. So we could use that to fix this.
                    //    - Fix when maps are split into separate .nanodata files. I want to do this in the future to reduce load times and avoid loading objects for maps that aren't opened in the editor. Would make it easy to discard per map.
                    NanoDB.LoadAsync(NanoDB.CurrentProject.FilePath);
                }
                SaveConfirmationDialog.UserChoseDontSave = false;
            }

            //Draw dialogs
            bool anyKeyDisableDialogsOpen = false;
            for (var kv in _guiDialogs)
            {
                for (Dialog dialog in kv.value)
                {
                    if (dialog.Open)
                    {
                        dialog.Draw(app, this);

                        if (dialog.DisableKeybindsWhileOpen)
                            anyKeyDisableDialogsOpen = true;
                    }
                }
            }

            //Disable keybinds if there are currently any dialogs open that request that
            Input.KeysEnabled = !anyKeyDisableDialogsOpen;

            //Auto open data folder selector if its not set
            if (!PackfileVFS.Ready && !PackfileVFS.Loading)
            {
                if (!DataFolderSelector.Open)
                {
                    String dataPath = CVar_GeneralSettings->DataPath;
                    if (dataPath.IsEmpty || !Directory.Exists(dataPath))
                    {
                        //Data path not set. Make the user choose one.
                        DataFolderSelector.Show();
                    }
                    else
                    {
                        //Data folder already set in config. Use it
                        PackfileVFS.MountDataFolderAsync("//data/", CVar_GeneralSettings->DataPath);
                    }
                }
            }

            Keybinds(app);
		}

        void DrawOutliner(App app)
        {
            if (!ImGui.Begin(OutlinerIdentifier, &OutlinerOpen))
            {
                ImGui.End();
                return;
            }

            if (FocusedDocument != null && FocusedDocument.HasCustomOutlinerAndInspector)
            {
            	FocusedDocument.Outliner(app, this);
            }
            else
            {
                ImGui.TextWrapped("Click on the viewport of map to see a list of its objects here.");
            }

            ImGui.End();
        }

        void DrawInspector(App app)
        {
            if (!ImGui.Begin(InspectorIdentifier, &InspectorOpen))
            {
                ImGui.End();
                return;
            }

            if (FocusedDocument != null && FocusedDocument.HasCustomOutlinerAndInspector)
            {
            	FocusedDocument.Inspector(app, this);
            }

            ImGui.End();
        }

        //Open a new document
        public bool OpenDocument(StringView title, StringView id, GuiDocumentBase newDoc)
        {
            //Make sure only one thread can access Documents at once
            DocumentLock.Enter();
            defer DocumentLock.Exit();

            //Make sure document ID is unique
            for (GuiDocumentBase doc in Documents)
                if (StringView.Equals(doc.UID, id, true))
                {
                    delete newDoc;
					return false;
				}

            newDoc.Title.Set(title);
            newDoc.UID.Set(id);
            Documents.Add(newDoc);
            RegisterDialogs(newDoc);
            return true;
        }

        //Add gui panel and validate its path to make sure its not a duplicate. Takes ownership of panel.
        void AddPanel(StringView menuPos, bool open, GuiPanelBase panel)
        {
            //Make sure there isn't already a panel with the same menuPos
            for (GuiPanelBase existingPanel in Panels)
            {
                if (StringView.Equals(menuPos, existingPanel.MenuPos, true))
                {
                    //Just crash when a duplicate is found. These are set at compile time currently so it'll always happen if theres a duplicate
                    Runtime.FatalError(scope $"Duplicate GuiPanel menu path. Fix this before release. Type = {panel.GetType().ToString(.. scope .())}. Path = '{menuPos}'");
                }
            }

            panel.MenuPos.Set(menuPos);
            panel.Open = open;
            Panels.Add(panel);
            RegisterDialogs(panel);
        }

        //Register any dialogs owned by this gui element which have the [RegisterDialog] attribute
        void RegisterDialogs(Object guiElement)
        {
            //Track dialogs attached to this gui element
            if (!_guiDialogs.ContainsKey(guiElement))
	        {
				_guiDialogs[guiElement] = new List<Dialog>();
			}

            Type elementType  = guiElement.GetType();
            for (var field in elementType.GetFields(.Public | .NonPublic | .Instance))
            {
                if (!field.FieldType.HasBaseType(typeof(Dialog)))
                    continue;

                switch (field.GetValue(guiElement))
                {
                    case .Ok(Variant val):
                        Dialog dialog = val.Get<Dialog>();
                        _guiDialogs[guiElement].Add(dialog);
                    case .Err:
                        Logger.Error("Failed to get dialog reference for {} in {}. Make sure the document or panel trying to use dialogs has [ReflectAll]", field.Name, elementType.GetName(.. scope .()));
                        continue;
                }
            }
        }

        private void TryOpenProject()
        {
            char8* outPath = null;
            switch (NativeFileDialog.OpenDialog("nanoproj", null, &outPath))
            {
                case .Okay:
                    NanoDB.LoadAsync(StringView(outPath));
                case .Cancel:
                    return;
                case .Error:
                    char8* error = NativeFileDialog.GetError();
                    Logger.Error("Error opening file selector in TryOpenProject(): {}", StringView(error));
            }
        }

        public void SaveAll(App app)
        {
            for (GuiDocumentBase doc in Documents)
            {
                if (!doc.Open && doc.UnsavedChanges)
                {
                    doc.Save(app, this);
                }

                doc.UnsavedChanges = false;
            }
            NanoDB.SaveAsync();
        }

        public void CloseNanoforge()
        {
            _closeNanoforgeRequested = true;
            ReadyForExit = false;
            for (GuiDocumentBase doc in Documents)
	            doc.Open = false; //Close all documents so unsaved changes popup lists them

            SaveConfirmationDialog.Show();
        }

        public void CreateProject()
        {
            _createProjectRequested = true;
            for (GuiDocumentBase doc in Documents)
	            doc.Open = false;

            SaveConfirmationDialog.Show();
        }

        public void OpenProject(StringView pathOfProjectToOpen = "")
        {
            _openProjectRequested = true;
            _pathOfProjectToOpen.Set(pathOfProjectToOpen);
            for (GuiDocumentBase doc in Documents)
	            doc.Open = false;

            SaveConfirmationDialog.Show();
        }

        public void CloseProject()
        {
            _closeProjectRequested = true;
            for (GuiDocumentBase doc in Documents)
	            doc.Open = false;

            SaveConfirmationDialog.Show();
        }

        private void Keybinds(App app)
        {
            Input input = app.GetResource<Input>();
            if (input.ControlDown && input.KeyPressed(.S))
            {
                SaveAll(app);
            }
        }
	}
}