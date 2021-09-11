#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "gui/GuiState.h"
#include "render/resources/Scene.h"
#include "util/TaskScheduler.h"
#include "RfgTools++/formats/meshes/StaticMesh.h"
#include <future>
#include <vector>

//Extension of Texture2D used by StaticMeshDocument for texture file searches
struct Texture2D_Ext
{
    //The texture
    Texture2D Texture;
    //Cpu file path in the global cache
    string CpuFilePath;
};

class StaticMeshDocument : public IDocument
{
public:
    StaticMeshDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer);
    ~StaticMeshDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

private:
    //Worker thread that loads a mesh and locates its textures in the background
    void WorkerThread(Handle<Task> task, GuiState* state);
    void DrawOverlayButtons(GuiState* state);
    std::optional<Texture2D_Ext> FindTexture(GuiState* state, const string& name, bool lookForHighResVariant);
    std::optional<Texture2D_Ext> GetTexture(GuiState* state, const string& textureName, bool useLastResortSearches = false);
    std::optional<Texture2D_Ext> GetTextureFromPackfile(GuiState* state, Packfile3* packfile, const string& textureName);
    std::optional<Texture2D_Ext> GetTextureFromPeg(GuiState* state, const string& vppName, const string& parentName, const string& pegName, const string& textureName, bool inContainer);

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

    string DiffuseMapPegPath = "";
    string SpecularMapPegPath = "";
    string NormalMapPegPath = "";
    string DiffuseTextureName;
    string SpecularTextureName;
    string NormalTextureName;

    GuiState* state_ = nullptr;
    bool meshExportEnabled_ = false;
};