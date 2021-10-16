#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "gui/GuiState.h"
#include "render/resources/Scene.h"
#include "util/TaskScheduler.h"
#include "RfgTools++/formats/meshes/StaticMesh.h"
#include <future>
#include <vector>

class StaticMeshDocument : public IDocument
{
public:
    StaticMeshDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer);
    ~StaticMeshDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

private:
    //Worker thread that loads a mesh and locates its textures in the background
    void WorkerThread(Handle<Task> task, GuiState* state);
    void DrawOverlayButtons(GuiState* state);

    string Filename;
    string ParentName;
    string VppName;
    bool InContainer;
    string CpuFilePath;
    string GpuFilePath;
    StaticMesh StaticMesh;
    Handle<Scene> Scene = nullptr;

    Handle<Task> meshLoadTask_ = nullptr;
    string WorkerStatusString;
    f32 WorkerProgressFraction = 0.0f;

    string DiffuseTextureName;
    string SpecularTextureName;
    string NormalTextureName;

    GuiState* state_ = nullptr;
    bool meshExportEnabled_ = false;
};