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
    void WorkerThread(GuiState* state);
    //Clear temporary data created by the worker thread. Called once the worker thread is done working and the renderer is done using the worker data
    void WorkerThread_ClearData();
    //Loads vertex and index data of each zones terrain mesh
    void WorkerThread_LoadTerrainMeshes(GuiState* state);
    void WorkerThread_LoadTerrainMesh(FileHandle terrainMesh, Vec3 position, GuiState* state);
    bool WorkerThread_FindTexture(PackfileVFS* vfs, const string& textureName, PegFile10& peg, std::span<u8>& textureBytes, u32& textureWidth, u32& textureHeight);

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
    Timer TerrainThreadTimer;

    GuiState* state_ = nullptr;
};