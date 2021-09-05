#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "rfg/TerrainHelpers.h"
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
    std::future<void> WorkerFuture;
    bool WorkerResourcesFreed = false;
    bool PrimitivesNeedRedraw = true;
    Timer TerrainThreadTimer;

    GuiState* state_ = nullptr;
};