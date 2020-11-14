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
    std::shared_ptr<Scene> Scene = nullptr;
    std::vector<TerrainInstance> TerrainInstances = {};
    std::future<void> WorkerFuture;
    std::mutex ResourceLock;
    bool WorkerDone = false;
    bool WorkerResourcesFreed = false;
    bool NewTerrainInstanceAdded = false;
};


void TerritoryDocument_Init(GuiState* state, std::shared_ptr<Document> doc);
void TerritoryDocument_Update(GuiState* state, std::shared_ptr<Document> doc);
void TerritoryDocument_OnClose(GuiState* state, std::shared_ptr<Document> doc);