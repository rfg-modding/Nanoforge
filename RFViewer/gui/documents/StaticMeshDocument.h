#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "render/resources/Scene.h"
#include "RfgTools++/formats/meshes/StaticMesh.h"
#include <future>
#include <vector>

struct StaticMeshDocumentData
{
    string Filename;
    string ParentName;
    string VppName;
    bool InContainer;
    string CpuFilePath;
    string GpuFilePath;
    StaticMesh StaticMesh;
    std::shared_ptr<Scene> Scene = nullptr;
    std::vector<u32> RenderObjectIndices;

    std::future<void> WorkerFuture;
    string WorkerStatusString;
    f32 WorkerProgressFraction = 0.0f;
    bool WorkerDone = false;

    string DiffuseMapPegPath = "";
    string SpecularMapPegPath = "";
    string NormalMapPegPath = "";
    string DiffuseTextureName;
    string SpecularTextureName;
    string NormalTextureName;
};


void StaticMeshDocument_Init(GuiState* state, std::shared_ptr<Document> doc);
void StaticMeshDocument_Update(GuiState* state, std::shared_ptr<Document> doc);
void StaticMeshDocument_OnClose(GuiState* state, std::shared_ptr<Document> doc);