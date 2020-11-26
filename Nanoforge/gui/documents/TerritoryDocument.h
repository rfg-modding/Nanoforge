#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "rfg/TerrainHelpers.h"
#include "render/resources/Scene.h"
#include <future>

struct TerritoryDocumentData
{
    string TerritoryName;
    string TerritoryShortname;
    Territory Territory;
    Handle<Scene> Scene = nullptr;
    std::vector<TerrainInstance> TerrainInstances = {};
    std::future<void> WorkerFuture;
    std::mutex ResourceLock;
    bool WorkerDone = false;
    bool WorkerResourcesFreed = false;
    bool NewTerrainInstanceAdded = false;
    bool PrimitivesNeedRedraw = true;
    bool TerritoryDataLoaded = false;
};


void TerritoryDocument_Init(GuiState* state, Handle<Document> doc);
void TerritoryDocument_Update(GuiState* state, Handle<Document> doc);
void TerritoryDocument_OnClose(GuiState* state, Handle<Document> doc);