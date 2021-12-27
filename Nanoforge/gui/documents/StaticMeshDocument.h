#pragma once
#include "common/Typedefs.h"
#include "common/Handle.h"
#include "IDocument.h"
#include "RfgTools++/formats/meshes/StaticMesh.h"
#include <future>
#include <vector>

class Scene;
class Task;
class GuiState;

class StaticMeshDocument : public IDocument
{
public:
    StaticMeshDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer);
    ~StaticMeshDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;
    bool CanClose() override;
    void OnClose(GuiState* state) override;

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

    //Names of subtextures and the path of the peg they're stored in. Discovered on load and used during export.
    std::optional<string> DiffusePegPath;
    std::optional<string> DiffuseName;
    std::optional<string> SpecularPegPath;
    std::optional<string> SpecularName;
    std::optional<string> NormalPegPath;
    std::optional<string> NormalName;

    GuiState* state_ = nullptr;
    bool meshExportEnabled_ = false;
};