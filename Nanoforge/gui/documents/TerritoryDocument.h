#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "rfg/TerrainHelpers.h"
#include "render/resources/Scene.h"
#include <future>

//Used internally by TerritoryDocument
struct ShortVec4
{
    i16 x = 0;
    i16 y = 0;
    i16 z = 0;
    i16 w = 0;

    ShortVec4 operator-(const ShortVec4& B)
    {
        return ShortVec4{ x - B.x, y - B.y, z - B.z, w - B.w };
    }
};

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
    std::span<LowLodTerrainVertex> WorkerThread_GenerateTerrainNormals(std::span<ShortVec4> vertices, std::span<u16> indices);

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

    GuiState* state_ = nullptr;
};