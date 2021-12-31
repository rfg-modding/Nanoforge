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

private:
    void DrawOverlayButtons(GuiState* state);
    void UpdateDebugDraw(GuiState* state);

    string TerritoryName;
    string TerritoryShortname;
    Territory Territory;
    Handle<Scene> Scene = nullptr;
    Handle<Task> TerritoryLoadTask = nullptr;
    bool WorkerResourcesFreed = false;
    bool PrimitivesNeedRedraw = true;
    Timer TerrainThreadTimer;

    //High/low terrain visibility settings
    f32 highLodTerrainDistance_ = 1200.0f; //Low lod terrain will be used past this distance
    bool highLodTerrainEnabled_ = true;
    bool terrainVisiblityUpdateNeeded_ = true;
    u32 numTerrainInstances_ = 0;
    bool useHighLodTerrain_ = true;

    //Zone object visibility range
    f32 zoneObjDistance_ = 1200.0f;

    GuiState* state_ = nullptr;
};