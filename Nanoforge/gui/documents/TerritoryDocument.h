#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/Territory.h"
#include "common/timing/Timer.h"
#include <future>

class Scene;
class Task;
class GuiState;

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

private:
    void DrawMenuBar(GuiState* state);
    void Keybinds(GuiState* state);
    void UpdateDebugDraw(GuiState* state);

    //Object commands
    void DrawObjectCreationPopup(GuiState* state);
    void CloneObject(ObjectHandle object);
    void DeleteObject(ObjectHandle object);
    void CopyScriptxReference(ObjectHandle object);
    void RemoveWorldAnchors(ObjectHandle object);
    void RemoveDynamicLinks(ObjectHandle object);
    u32 GetNewObjectHandle(); //Get unused object handle for a new object

    //Outliner functions
    void Outliner_DrawFilters(GuiState* state);
    void Outliner_DrawObjectNode(GuiState* state, ObjectHandle object); //Draw tree node for zone object and recursively draw child objects
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

    void ExportTask(GuiState* state, bool exportPatch);
    bool ExportMap(GuiState* state);
    bool ExportPatch();

    string TerritoryName;
    string TerritoryShortname;
    Territory Territory;
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

    //Zone object visibility range
    f32 zoneObjDistance_ = 1200.0f;

    //Object creation popup data
    bool showObjectCreationPopup_ = false;
    string objectCreationType_ = "";
    
    bool updateDebugDraw_ = true;
    GuiState* state_ = nullptr;

    //Export thread data
    Handle<Task> exportTask_ = nullptr;
    string exportStatus_ = "";
    f32 exportPercentage_ = 0.0f;
};