#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "rfg/TerrainHelpers.h"
#include <future>

//Todo: Make separate document for general scenes (any meshes, effects, etc)
struct TerritoryDocumentData
{
    string TerritoryName;
    string TerritoryShortname;
    Territory Territory;
    u32 SceneIndex;
    std::vector<TerrainInstance> TerrainInstances = {};
    std::future<void> WorkerFuture;
    std::mutex ResourceLock;
    bool WorkerDone = false;
    bool WorkerResourcesFreed = false;
    bool NewTerrainInstanceAdded = false;
};


void TerritoryDocument_Init(GuiState* state, Document& doc);
void TerritoryDocument_Update(GuiState* state, Document& doc);
void TerritoryDocument_OnClose(GuiState* state, Document& doc);