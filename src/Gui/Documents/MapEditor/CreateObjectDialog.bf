using System;
using ImGui;
using Nanoforge.App;
using Nanoforge.Rfg;
using System.Collections;
using Nanoforge.Misc;
namespace Nanoforge.Gui.Documents.MapEditor;

//Used by MapEditorDocument to create new zone objects
public class CreateObjectDialog : Dialog
{
    private Territory _map = null;
    private delegate void(DialogResult result) _onClose = null ~DeleteIfSet!(_);

    private append String _nameBuffer;
    private ObjectTypeDefinition _selectedType = null;
    private append List<ObjectTypeDefinition> _objectTypeDefinitions ~ClearAndDeleteItems(_);

    private class ObjectTypeDefinition
    {
        public Type Type;
        public StringView Classname;
        public bool Disabled;

        public this(Type type, StringView classname, bool disabled)
        {
            Type = type;
            Classname = classname;
            Disabled = disabled;
        }
    }

    //Object that gets constructed by the dialog, but not added to NanoDB yet. MapEditorDialog adds it to NanoDB after a successful creation via the _onClose callback.
    public ZoneObject PendingObject = null ~DeleteIfSet!(_);

    public ZoneObject Parent = null;

    //Used when something happens that breaks the dialog. Draw() will disable it and show an error.
    private bool _fatalError = false;

    public this() : base("Create object")
    {
        DisableKeybindsWhileOpen = true;

        DefineObjectType<ObjZone>("obj_zone");
		DefineObjectType<ObjectBoundingBox>("object_bounding_box");
		DefineObjectType<ObjectDummy>("object_dummy");
		DefineObjectType<PlayerStart>("player_start");
        DefineObjectType<TriggerRegion>("trigger_region");
		DefineObjectType<ObjectMover>("object_mover", disabled: true);
		DefineObjectType<GeneralMover>("general_mover", disabled: true);
		DefineObjectType<RfgMover>("rfg_mover", disabled: true);
        DefineObjectType<ShapeCutter>("shape_cutter");
		DefineObjectType<ObjectEffect>("object_effect");
		DefineObjectType<Item>("item");
		DefineObjectType<Weapon>("weapon");
        DefineObjectType<Ladder>("ladder");
		DefineObjectType<ObjLight>("obj_light");
		DefineObjectType<MultiMarker>("multi_object_marker");
		DefineObjectType<MultiFlag>("multi_object_flag");

        //These types are defined because of a few MP/WC maps that mistakenly have them. You're not able to create them yet because they should only be used in SP.
        //DefineObjectType<NavPoint>("navpoint");
		//DefineObjectType<CoverNode>("cover_node");
		//DefineObjectType<RfgConstraint>("constraint");
		//DefineObjectType<ActionNode>("object_action_node"); 
    }

    private void DefineObjectType<T>(StringView classname, bool disabled = false) where T : ZoneObject
    {
        _objectTypeDefinitions.Add(new ObjectTypeDefinition(typeof(T), classname, disabled));
    }

    public void Show(Territory map, delegate void(DialogResult result) onClose = null, StringView initialObjectType = "object_dummy", ZoneObject parent = null)
    {
        _fatalError = false;
        Open = true;
        _firstDraw = true;
        Result = .None;

        _map = map;
        if (_onClose != null)
        {
            delete _onClose; //Delete delegates from previous runs
        }
        _onClose = onClose;

        //Create initial object
        _selectedType = null;
        for (var objectType in _objectTypeDefinitions)
        {
            if (objectType.Classname.Equals(initialObjectType, ignoreCase: true))
            {
                _selectedType = objectType;
                break;
            }
        }
        if (_selectedType == null)
        {
            _selectedType = _objectTypeDefinitions[0];
        }

        Parent = parent;

        CreateNewPendingObject(_selectedType.Type);
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

        if (ImGui.BeginPopupModal(Title, null, .Popup))
        {
            if (_fatalError)
            {
                ImGui.PushStyleColor(.Text, .Red);
                ImGui.TextWrapped("Fatal error occurred. Please close this dialog and try again. Check the log for more details.");
                ImGui.PopStyleColor();
            }
            else
            {
                if (Parent != null)
                {
                    String parentDisplayName = scope .();
                    if (!Parent.Name.IsEmpty)
                    {
                        parentDisplayName.Set(Parent.Name);
                    }
                    else
                    {
                        parentDisplayName.Set(Parent.Classname);
                    }
                    ImGui.LabelAndValue("Parent:", scope $"{parentDisplayName} - [{Parent.Handle}, {Parent.Num}]"..EnsureNullTerminator(), .SecondaryTextColor);
                }

                //TODO: Stop hardcoding _objectTypeDefinitions. Make attribute to enable objects in this dialog and create the list of options in the constructor via reflection
                //TODO: Change to define display name with attribute on the object classes
                if (ImGui.BeginCombo("Type", _selectedType.Classname.Ptr))
                {
                    for (ObjectTypeDefinition objType in _objectTypeDefinitions)
                    {
                        bool selected = (objType.Type == _selectedType.Type);
                        ImGui.BeginDisabled(objType.Disabled);
                        if (ImGui.Selectable(objType.Classname, selected) && objType.Type != _selectedType.Type)
                        {
                            _selectedType = objType;

                            //Type changed. Delete current PendingObject and create a new one with the new type.
                            CreateNewPendingObject(objType.Type);
                        }
                        ImGui.EndDisabled();
                        if (objType.Disabled)
                        {
                            ImGui.TooltipOnPrevious(scope $"Nanoforge can't create these from scratch yet. Copy an existing {objType.Classname} instead and modify it."..EnsureNullTerminator());
                        }
                    }
                    ImGui.EndCombo();
                }

                ImGui.InputText("Name", _nameBuffer);

                if (ImGui.Button("Create") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
                {
                    Close(.Yes);
                }
                ImGui.SameLine();

            }

            //Auto focus on default option on first frame so you don't need to switch to the mouse
            if (_firstDraw)
                ImGui.SetKeyboardFocusHere();

            if (ImGui.Button("Cancel") || (ImGui.IsItemFocused() && ImGui.IsKeyPressed(.Enter)))
            {
                Close(.Cancel);
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
        if (_onClose != null)
            _onClose(Result);
    }

    //Destroy PendingObject and make a new one with the provided type
    private void CreateNewPendingObject(Type type)
    {
        if (!type.HasBaseType(typeof(ZoneObject)))
        {
            String error = scope $"CreateObjectDialog.CreatePendingObject() failed to create a new object. The provided type '{type.GetFullName(.. scope .())}' does not inherit ZoneObject";
            Logger.Error(error);
        }

        DeleteIfSet!(PendingObject);
        switch (type.CreateObject())
        {
            case .Ok(Object newPendingObject):
                PendingObject = (ZoneObject)newPendingObject;
            case .Err:
                Logger.Error("CreateObjectDialog.CreatePendingObject() failed to create a new object of type '{}'", type.GetFullName(.. scope .()));
                return;
        }
    }
}