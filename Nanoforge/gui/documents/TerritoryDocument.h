#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/Territory.h"
#include "common/timing/Timer.h"
#include "gui/util/Popup.h"
#include "render/imgui/ImGuiFontManager.h"
#include <future>

class Scene;
class Task;
class GuiState;

enum MapExportType
{
    Vpp,
    BinaryPatch
};

class TerritoryDocument : public IDocument
{
public:
    TerritoryDocument(GuiState* state, std::string_view territoryName, std::string_view territoryShortname);
    ~TerritoryDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;
    bool CanClose() override;
    void OnClose(GuiState* state) override;
    void Outliner(GuiState* state) override;
    void Inspector(GuiState* state) override;

    const u32 MAX_OUTLINER_DEPTH = 7; //The outliner won't draw tree nodes deeper than this
    const u32 MAX_DEEP_CLONE_DEPTH = 10; //::DeepCloneObject() won't clone objects in hierarchies any deeper than this. It will still clone objects higher in the hierarchy and log an error.

    string TerritoryName;
    string TerritoryShortname;
    Territory Territory;

private:
    void DrawMenuBar(GuiState* state);
    void Keybinds(GuiState* state);
    void DrawPopups(GuiState* state);
    void UpdateDebugDraw(GuiState* state);

    //Object commands
    void DrawObjectCreationPopup(GuiState* state);
    ObjectHandle SimpleCloneObject(ObjectHandle object, bool useTerritoryZone = false);
    ObjectHandle ShallowCloneObject(ObjectHandle object, bool selectNewObject = true, bool useTerritoryZone = false);
    ObjectHandle DeepCloneObject(ObjectHandle object, bool selectNewObject = true, u32 depth = 0, bool useTerritoryZone = false);
    void DeleteObject(ObjectHandle object);
    void CopyScriptxReference(ObjectHandle object);
    void RemoveWorldAnchors(ObjectHandle object);
    void RemoveDynamicLinks(ObjectHandle object);
    void AddGenericObject(ObjectHandle parent = NullObjectHandle);
    void OrphanObject(ObjectHandle object);
    void OrphanChildren(ObjectHandle parent);
    u32 GetNewObjectHandle(); //Get unused object handle for a new object. Returns 0xFFFFFFFF if it fails.
    u32 GetNewObjectNum(); //Get unused object num for a new object. Returns 0xFFFFFFFF if it fails.

    //Outliner functions
    void Outliner_DrawFilters(GuiState* state);
    void Outliner_DrawObjectNode(GuiState* state, ObjectHandle object, u32 depth); //Draw tree node for zone object and recursively draw child objects
    bool Outliner_ZoneAnyChildObjectsVisible(ObjectHandle zone); //Returns true if any objects in the zone are visible
    bool Outliner_ShowObjectOrChildren(ObjectHandle object); //Returns true if the object or any of it's children are visible
    //If true zones outside the territory viewing distance are hidden. Configurable via the buttons in the top left of the territory viewer.
    bool outliner_OnlyShowNearZones_ = true;
    bool outliner_OnlyShowPersistentObjects_ = false;

    //Inspector functions
    bool Inspector_DrawObjectHandleEditor(PropertyHandle prop);
    bool Inspector_DrawObjectHandleListEditor(PropertyHandle prop);
    bool Inspector_DrawVec3Editor(PropertyHandle prop);
    bool Inspector_DrawMat3Editor(PropertyHandle prop);
    bool Inspector_DrawStringEditor(PropertyHandle prop);
    bool Inspector_DrawBoolEditor(PropertyHandle prop);
    void Inspector_DrawRelativeEditor();

    void ExportTask(GuiState* state, MapExportType exportType);
    bool ExportMap(GuiState* state, const string& exportPath);
    bool ExportBinaryPatch(GuiState* state);
    bool ExportMapSP(GuiState* state, const string& exportPath);

    //Update relevant data when an object is changed
    void ObjectEdited(ObjectHandle object);
    void SetObjectParent(ObjectHandle object, ObjectHandle oldParent, ObjectHandle newParent);

    Handle<Scene> Scene = nullptr;
    Handle<Task> TerritoryLoadTask = nullptr;
    bool WorkerResourcesFreed = false;
    bool PrimitivesNeedRedraw = true;
    Timer TerrainThreadTimer;
    ObjectHandle selectedObject_ = NullObjectHandle;

    //High/low terrain visibility settings
    f32 highLodTerrainDistance_ = 1200.0f; //Low lod terrain will be used past this distance
    bool highLodTerrainEnabled_ = true;
    bool terrainVisiblityUpdateNeeded_ = true;
    u32 numLoadedTerrainInstances_ = 0;
    bool useHighLodTerrain_ = true;
    f32 chunkDistance_ = 500.0f;

    //Zone object visibility range
    f32 zoneObjDistance_ = 1200.0f;

    //Object creation popup data
    bool showObjectCreationPopup_ = false;
    string objectCreationType_ = "";
    
    bool updateDebugDraw_ = true;
    GuiState* state_ = nullptr;

    //Debug draw & editor settings
    bool drawSecondaryBbox_ = false;

    //Export thread data
    Handle<Task> exportTask_ = nullptr;
    string exportStatus_ = "";
    f32 exportPercentage_ = 0.0f;
    string originalVppPath_ = ""; //Path of original vpp_pc file. For binary patch generation

    //If true a large solid box will be drawn in the position of the currently hovered zone in the outliner
    bool _highlightHoveredZone = false;
    //Short name of hovered zone. E.g. 04_06 
    ObjectHandle _hoveredZone = NullObjectHandle;
    f32 _zoneBoxHeight = 150.0f; //For far a highlight zones bbox goes above and below y=0.0. The terrain can go from -512 to 512 but 256 is the default in the editor for visibility purposes

    //If true draw a solid box around the object being hovered in the outliner
    bool _highlightHoveredObject = false;
    ObjectHandle _hoveredObject = NullObjectHandle;

    //Used by "Add dummy child" right click menu in outliner. Adds the dummy at the end of ::Outliner() so we're not modifying objects Children list while iterating.
    ObjectHandle _contextMenuAddDummyChildParent = NullObjectHandle;
    ObjectHandle _contextMenuShallowCloneObject = NullObjectHandle;
    ObjectHandle _contextMenuDeepCloneObject = NullObjectHandle;
    //Open this node in the outliner if it's set. Currently only works if the parent node of this node is also open. Fine for now since it only gets triggered by the "add dummy child" right click menu option
    ObjectHandle _openNodeNextFrame = NullObjectHandle;
    //Scrolls to bottom of outliner once next time its drawn
    bool _scrollToBottomOfOutliner = false;
    //Scroll to this object next time the outliner is drawn. Only works if that objects outliner node is visible. It won't open that object node.
    ObjectHandle _scrollToObjectInOutliner = NullObjectHandle;

    bool _handledImport = false;
    bool _zoneInitialized = false;
    string _exportResultString = "";
    bool _inspectorEditingName = false;
    string _inspectorNameEditBuffer = "";
    string _inspectorObjectClassSelectorSearch = "";
    u16 _inspectorManualFlagSetter = 0;
    string _inspectorNewPropName = "";
    string _inspectorNewPropType = "uint";
    string _inspectorParentSelectorSearch = "";

    //Confirmation popups + relevant object for several operations. Kinda gross but Dear ImGui popups operate in a strange order
    Popup deleteObjectPopup_ = { "Delete object?", "Are you sure you want to delete the object? Any children will be orphaned " ICON_FA_HEART_BROKEN, PopupType::YesCancel, true };
    ObjectHandle deleteObjectPopupHandle_ = NullObjectHandle;
    Popup removeWorldAnchorPopup_ = { "Remove world anchors?", "Are you sure you'd like to remove the world anchors from this object? You can't undo this.", PopupType::YesCancel, true };
    ObjectHandle removeWorldAnchorPopupHandle_ = NullObjectHandle;
    Popup removeDynamicLinkPopup_ = { "Remove dynamic links?", "Are you sure you'd like to remove the dynamic links from this object? You can't undo this.", PopupType::YesCancel, true };
    ObjectHandle removeDynamicLinkPopupHandle_ = NullObjectHandle;
    Popup orphanObjectPopup_ = { "Orphan object?", "Are you sure you'd like to make this object an orphan?", PopupType::YesCancel, true };
    ObjectHandle orphanObjectPopupHandle_ = NullObjectHandle;
    Popup orphanChildrenPopup_ = { "Orphan children?", "Are you sure you'd like to orphan all the children of this object? You'll need to manually add the children to parent if you want to undo this.", PopupType::YesCancel, true };
    ObjectHandle orphanChildrenPopupHandle_ = NullObjectHandle;
};