#pragma once
#include "common/Typedefs.h"
#include "TerrainHelpers.h"
#include <ext/WindowsWrapper.h>
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\types\Vec4.h>
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include "rfg/FileHandle.h"
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;

struct ZoneFile
{
    string Name;
    ZonePc36 Zone;
    bool RenderBoundingBoxes = false;
};
//Used to filter objects list by class type
struct ZoneObjectClass
{
    string Name;
    u32 Hash = 0;
    u32 NumInstances = 0;
    Vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool Show = true;
    bool ShowLabel = false;
    const char* LabelIcon = "";
};

constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

//Used to color status bar
enum GuiStatus
{
    Ready, //Used when the app isn't doing any background work and the gui / overlays are completely ready for use
    Working, //Used when the app is doing background work. User may have to wait for the work to complete to use the entire gui
    Error, //Used when a serious error that requires user attention is encountered
    None //Used for default function arguments
};

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    MainGui(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, HWND hwnd);
    ~MainGui();

    void Update(f32 deltaTime);
    void HandleResize();

    bool Visible = true;

    std::vector<TerrainInstance> TerrainInstances = {};

    //Lock used to make sure the worker thread and main thread aren't using resources at the same time
    std::mutex ResourceLock;
    //Tells the main thread that the worker thread pushed a new terrain instance that needs to be uploaded to the gpu
    bool NewTerrainInstanceAdded = false;
    //Set this to false to test the init sequence. It will wait for this to be set to true to start init. F1 sets this to true
    bool CanStartInit = true;

private:
    //Todo: Move the worker thread into different threads / files / etc
    void WorkerThread();
    void LoadTerrainMeshes();
    void LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position);
    
    void DrawMainMenuBar();
    void DrawDockspace();
    void DrawStatusBar();
    void DrawFileExplorer();
    void DrawZoneWindow();
    void DrawZoneObjectsWindow();
    void DrawRenderSettingsWindow();
    void DrawZonePrimitives();
    void DrawCameraWindow();
    void DrawNodeEditor();


    //Whether or not this object should be shown based on filtering settings
    bool ShouldShowObjectClass(u32 classnameHash);
    //Checks if a object class is in the selected zones class list
    bool ObjectClassRegistered(u32 classnameHash, u32& outIndex);
    //Set selected zone and update any cached data about it's objects
    void SetSelectedZone(u32 index);
    //Update number of instances of each object class for selected zone
    void UpdateObjectClassInstanceCounts();
    //Scans all zone objects for any object class types that aren't known. Used for filtering and coloring purposes
    void InitObjectClassData();
    ZoneObjectClass& GetObjectClass(u32 classnameHash);

    f32 ScaleTextSizeToDistance(f32 minSize, f32 maxSize, const Vec3& textPos);

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
    Camera* camera_ = nullptr;
    HWND hwnd_ = nullptr;
    int windowWidth_ = 800;
    int windowHeight_ = 800;

    std::vector<ZoneFile> zoneFiles_;
    Im3d::Vec4 boundingBoxColor_ = Im3d::Vec4(0.778f, 0.414f, 0.0f, 1.0f);
    f32 boundingBoxThickness_ = 1.0f;
    
    Im3d::Vec3 gridOrigin_ = { 0.0f, 0.0f, 0.0f };
    bool gridFollowCamera_ = true;
    bool drawGrid_ = false;
    int gridSpacing_ = 10;
    int gridSize_ = 100;
    
    u32 selectedZone = InvalidZoneIndex;
    std::vector<ZoneObjectClass> zoneObjectClasses_ = {};

    f32 labelTextSize_ = 1.0f;
    Vec4 labelTextColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

    bool drawParentConnections_ = false;

    f32 statusBarHeight_ = 25.0f;
    GuiStatus status_ = Ready;
    string customStatusMessage_ = "";

    //Set status message and enum
    void SetStatus(const string& message, GuiStatus status = None)
    {
        //Set status enum too if it's not the default argument value
        if (status != None)
            status_ = status;

        customStatusMessage_ = message;
    }
    //Clear status message and set status enum to 'Ready'
    void ClearStatus()
    {
        status_ = Ready;
        customStatusMessage_ = "";
    }
};