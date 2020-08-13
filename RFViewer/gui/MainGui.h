#pragma once
#include "common/Typedefs.h"
#include "TerrainHelpers.h"
#include <ext/WindowsWrapper.h>
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\types\Vec4.h>
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include "rfg/FileHandle.h"
#include "rfg/ZoneManager.h"
#include "GuiState.h"
#include "GuiBase.h"
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, HWND hwnd, ZoneManager* zoneManager);
    ~MainGui();

    void Update(f32 deltaTime);
    void HandleResize();

    std::vector<TerrainInstance> TerrainInstances = {};

    //Lock used to make sure the worker thread and main thread aren't using resources at the same time
    std::mutex ResourceLock;
    //Tells the main thread that the worker thread pushed a new terrain instance that needs to be uploaded to the gpu
    bool NewTerrainInstanceAdded = false;
    //Set this to false to test the init sequence. It will wait for this to be set to true to start init. F1 sets this to true
    bool CanStartInit = true;

    GuiState state_;

private:
    //Todo: Move the worker thread into different threads / files / etc
    void WorkerThread();
    void LoadTerrainMeshes();
    void LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position);
    
    void DrawMainMenuBar();
    void DrawDockspace();

    f32 ScaleTextSizeToDistance(f32 minSize, f32 maxSize, const Vec3& textPos);

    std::vector<GuiPanel> panels_;

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
    Camera* camera_ = nullptr;
    ZoneManager* zoneManager_ = nullptr;
    HWND hwnd_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;
};