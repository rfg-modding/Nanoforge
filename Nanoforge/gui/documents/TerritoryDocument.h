#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "rfg/TerrainHelpers.h"
#include "util/TaskScheduler.h"
#include "render/resources/Scene.h"
#include "common/timing/Timer.h"
#include <future>

class TerritoryDocument : public IDocument
{
public:
    TerritoryDocument(GuiState* state, string territoryName, string territoryShortname);
    ~TerritoryDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

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

    GuiState* state_ = nullptr;
};