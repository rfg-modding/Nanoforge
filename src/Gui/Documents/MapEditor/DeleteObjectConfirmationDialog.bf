using System;
using ImGui;
using Nanoforge.App;
using Nanoforge.Rfg;
namespace Nanoforge.Gui.Documents.MapEditor;

//Used by MapEditorDocument to confirm that the user wants to delete a zone object
public class DeleteObjectConfirmationDialog : Dialog
{
    private ZoneObject _objectToDelete = null;
    private delegate void(DialogResult result) _onClose = null ~DeleteIfSet!(_);

    public bool ParentAdoptsChildren { get; private set; } = false;

    public this() : base("Delete object?")
    {
        DisableKeybindsWhileOpen = true;
    }

    public void Show(ZoneObject objectToDelete, delegate void(DialogResult result) onClose = null)
    {
        Open = true;
        _objectToDelete = objectToDelete;
        _firstDraw = true;
        Result = .None;

        if (_onClose != null)
        {
            delete _onClose; //Delete delegates from previous runs
        }
        _onClose = onClose;
    }

    public override void Draw(App app, Gui gui)
    {
        if (Open)
            ImGui.OpenPopup(Title);
        else
            return;

        defer { _firstDraw = false; }

        //Manually set padding since the parent window might be a document with padding disabled
        ImGui.PushStyleVar(.WindowPadding, .(12.0f, 12.0f));

        //Auto center
        ImGui.IO* io = ImGui.GetIO();
        ImGui.SetNextWindowPos(.(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), .Always, .(0.5f, 0.5f));

        //Reasonable default size
        ImGui.SetNextWindowSize(.(500.0f, 0.0f));

        if (ImGui.BeginPopupModal(Title, null))
        {
            defer ImGui.EndPopup();

            String objectName = scope .();
            if (!_objectToDelete.Name.IsEmpty)
            {
                objectName.Set(_objectToDelete.Name);
            }
            else
            {
                objectName.Set(_objectToDelete.Classname);
            }
            ImGui.TextWrapped(scope $"Are you sure you want to delete the selected object ({objectName})?");

            ImGui.PushStyleColor(.Text, .Red);
            ImGui.TextWrapped("WARNING! There is currently no undo/redo. If you delete an object and want it back you either need to close and reopen the project without saving, or re-import a copy of the map that still has the object.");
            ImGui.TextWrapped("Undo/redo will be added in a future release.");
            ImGui.PopStyleColor();

            bool targetHasParent = (_objectToDelete.Parent != null);
            bool targetHasChildren = (_objectToDelete.Children.Count > 0);
            bool parentAdoptsChildrenUseable = targetHasParent && targetHasChildren;
            if (!parentAdoptsChildrenUseable)
                ParentAdoptsChildren = false;

            ImGui.BeginDisabled(!parentAdoptsChildrenUseable);
            ImGui.Checkbox("Parent adopts children", &ParentAdoptsChildren);
            ImGui.TooltipOnPrevious("If checked the children of the deleted object will be adopted by its parent. Otherwise they'll be orphaned (no parent, they'll sit at the root of the outliner)");
            ImGui.EndDisabled();

            if (ImGui.Button("Yes") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
            {
                Close(.Yes);
            }
            ImGui.SameLine();

            //Auto focus on default option on first frame so you don't need to switch to the mouse
            if (_firstDraw)
                ImGui.SetKeyboardFocusHere();

            if (ImGui.Button("Cancel") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
            {
                Close(.Cancel);
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