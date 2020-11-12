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
    u32 SceneIndex = 0;
    std::vector<u32> RenderObjectIndices;

    //std::future<void> WorkerFuture;
    //std::mutex ResourceLock;
    //bool WorkerDone = false;
    //bool WorkerResourcesFreed = false;
};


void StaticMeshDocument_Init(GuiState* state, Document& doc);
void StaticMeshDocument_Update(GuiState* state, Document& doc);
void StaticMeshDocument_OnClose(GuiState* state, Document& doc);