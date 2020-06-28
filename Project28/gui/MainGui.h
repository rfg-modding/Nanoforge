#pragma once
#include "common/Typedefs.h"
#include <ext/WindowsWrapper.h>
#include <im3d/im3d.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\types\Vec4.h>
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include <vector>

class ImGuiFontManager;
class PackfileVFS;
class Camera;

struct ZoneFile
{
    string Name;
    ZonePc36 Zone;
    bool RenderBoundingBoxes = true;
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
//Used to render a zones terrain
struct TerrainInstance
{
    string Name;
    std::vector<MeshDataBlock> Meshes = {}; //Low lod terrain files have 9 meshes (not technically submeshes)
    std::vector<std::span<u16>> Indices = {{}};
    std::vector<std::span<Vec3>> Vertices = {{}};
    bool Visible = true;
};

constexpr u32 InvalidZoneIndex = 0xFFFFFFFF;

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

private:
    void DrawMainMenuBar();
    void DrawDockspace();
    void DrawFileExplorer();
    void DrawZoneWindow();
    void DrawZoneObjectsWindow();
    void DrawRenderSettingsWindow();
    void DrawZonePrimitives();
    void DrawCameraWindow();
    void DrawNodeEditor();

    void LoadTerrainMeshes();

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
};