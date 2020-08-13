#include "MainGui.h"
#include "common/Typedefs.h"
#include "common/filesystem/Path.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "render/imgui/imgui_ext.h"
#include "render/camera/Camera.h"
#include "gui/panels/FileExplorer.h"
#include "gui/panels/CameraPanel.h"
#include "gui/panels/RenderSettings.h"
#include "gui/panels/ScriptxEditor.h"
#include "gui/panels/StatusBar.h"
#include "gui/panels/ZoneList.h"
#include "gui/panels/ZoneObjectsList.h"
#include "gui/panels/ZoneRender.h"
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include <imgui/imgui.h>
#include <imgui_internal.h>
#include <im3d.h>
#include <im3d_math.h>
#include <filesystem>
#include <iostream>
#include <future>

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, HWND hwnd, ZoneManager* zoneManager)
{
    fontManager_ = fontManager;
    packfileVFS_ = packfileVFS;
    camera_ = camera;
    hwnd_ = hwnd;
    zoneManager_ = zoneManager;

    state_ = GuiState{ fontManager_, packfileVFS_, camera_, zoneManager_ };

    //Create node editor library context
    state_.nodeEditor_ = node::CreateEditor();
    
    //Start worker thread and capture it's future. If future isn't captured it won't actually run async
    static std::future<void> dummy = std::async(std::launch::async, &MainGui::WorkerThread, this);

    //Register all gui panels
    panels_ =
    {
        GuiPanel{&FileExplorer_Update},
        GuiPanel{&CameraPanel_Update},
        GuiPanel{&RenderSettings_Update},
        GuiPanel{&StatusBar_Update},
        GuiPanel{&ZoneObjectsList_Update},
        GuiPanel{&ZoneRender_Update},

        //Todo: Enable in release builds when this is a working feature
#ifdef DEBUG_BUILD
        GuiPanel{&ScriptxEditor_Update},
#endif
        GuiPanel{&ZoneList_Update},
    };
}

MainGui::~MainGui()
{
    node::DestroyEditor(state_.nodeEditor_);
}

void MainGui::Update(f32 deltaTime)
{
    //Draw built in / special gui elements
    DrawMainMenuBar();
    DrawDockspace();
    ImGui::ShowDemoWindow();

    //Draw the rest of the gui code
    for (auto& panel : panels_)
    {
#ifdef DEBUG_BUILD
        //Todo: Add panel name to the error
        if (!panel.Update)
            throw std::exception("Error! Update function pointer for panel was nullptr.");
#endif

        panel.Update(&state_);
    }
}

void MainGui::HandleResize()
{
    RECT usableRect;

    if (GetClientRect(hwnd_, &usableRect))
    {
        windowWidth_ = usableRect.right - usableRect.left;
        windowHeight_ = usableRect.bottom - usableRect.top;
    }
}

void MainGui::WorkerThread()
{
    state_.SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);
    while (!CanStartInit)
    {
        Sleep(100);
    }

    //Scan contents of packfiles
    state_.SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
    packfileVFS_->ScanPackfiles();

    //Todo: Load all zone files in all vpps and str2s. Someone organize them by purpose/area. Maybe by territory
    //Read all zones from zonescript_terr01.vpp_pc
    state_.SetStatus(ICON_FA_SYNC " Loading zones", Working);
    zoneManager_->LoadZoneData();
    state_.SetSelectedZone(0);

    //Load terrain meshes and extract their index + vertex data
    LoadTerrainMeshes();

    state_.ClearStatus();
}

//Loads vertex and index data of each zones terrain mesh
//Note: This function is run by the worker thread
void MainGui::LoadTerrainMeshes()
{
    state_.SetStatus(ICON_FA_SYNC " Locating terrain files", Working);

    //Todo: Split this into it's own function(s)
    //Get terrain mesh files for loaded zones and load them
    std::vector<string> terrainCpuFileNames = {};
    std::vector<string> terrainGpuFileNames = {};
    std::vector<Vec3> terrainPositions = {};

    //Create list of terrain meshes we need from obj_zone objects
    for (auto& zone : zoneManager_->ZoneFiles)
    {
        //Get obj zone object from zone
        auto* objZoneObject = zone.Zone.GetSingleObject("obj_zone");
        if (!objZoneObject)
            continue;

        //Attempt to get terrain mesh name from it's properties
        auto* terrainFilenameProperty = objZoneObject->GetProperty<StringProperty>("terrain_file_name");
        if (!terrainFilenameProperty)
            continue;

        bool found = false;
        for (auto& filename : terrainCpuFileNames)
        {
            if (filename == terrainFilenameProperty->Data)
                found = true;
        }
        if (!found)
        {
            terrainCpuFileNames.push_back(terrainFilenameProperty->Data);
            terrainPositions.push_back(objZoneObject->Bmin + ((objZoneObject->Bmax - objZoneObject->Bmin) / 2.0f));
        }
        else
            std::cout << "Found another zone file using the terrain mesh \"" << terrainFilenameProperty->Data << "\"\n";
    }

    //Remove extra null terminators that RFG so loves to have in it's files
    for (auto& filename : terrainCpuFileNames)
    {
        //Todo: Strip this in the StringProperty. Causes many issues
        if (filename.ends_with('\0'))
            filename.pop_back();
    }

    //Generate gpu file names from cpu file names
    for (u32 i = 0; i < terrainCpuFileNames.size(); i++)
    {
        string& filename = terrainCpuFileNames[i];
        terrainGpuFileNames.push_back(filename + ".gterrain_pc");
        filename += ".cterrain_pc";
    }

    //Get handles to cpu files
    auto terrainMeshHandlesCpu = packfileVFS_->GetFiles(terrainCpuFileNames, true, true);

    state_.SetStatus(ICON_FA_SYNC " Loading terrain meshes", Working);

    //Todo: Make multithreaded loading optional
    std::vector<std::future<void>> futures;
    u32 terrainMeshIndex = 0;
    for (auto& terrainMesh : terrainMeshHandlesCpu)
    {
        futures.push_back(std::async(std::launch::async, &MainGui::LoadTerrainMesh, this, terrainMesh, terrainPositions[terrainMeshIndex]));
        //LoadTerrainMesh(terrainMesh, terrainPositions[terrainMeshIndex]);
        terrainMeshIndex++;
    }
    for (auto& future : futures)
    {
        future.wait();
    }
}

void MainGui::LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position)
{
    //Get packfile that holds terrain meshes
    auto* container = terrainMesh.GetContainer();
    if (!container)
        throw std::runtime_error("Error! Failed to get container ptr for a terrain mesh.");

    //Todo: This does a full extract twice on the container due to the way single file extracts work. Fix this
    //Get mesh file byte arrays
    auto cpuFileBytes = container->ExtractSingleFile(terrainMesh.Filename(), true);
    auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc", true);

    //Ensure the mesh files were extracted
    if (!cpuFileBytes)
        throw std::runtime_error("Error! Failed to get terrain mesh cpu file byte array!");
    if (!gpuFileBytes)
        throw std::runtime_error("Error! Failed to get terrain mesh gpu file byte array!");

    BinaryReader cpuFile(cpuFileBytes.value());
    BinaryReader gpuFile(gpuFileBytes.value());

    //Create new instance
    TerrainInstance terrain;
    terrain.Position = position;

    //Get vertex data. Each terrain file is made up of 9 meshes which are stitched together
    u32 cpuFileIndex = 0;
    u32* cpuFileAsUintArray = (u32*)cpuFileBytes.value().data();
    for (u32 i = 0; i < 9; i++)
    {
        //Get mesh crc from gpu file. Will use this to find the mesh description data section of the cpu file which starts and ends with this value
        //In while loop since a mesh file pair can have multiple meshes inside
        u32 meshCrc = gpuFile.ReadUint32();
        if (meshCrc == 0)
            throw std::runtime_error("Error! Failed to read next mesh data block hash in terrain gpu file.");

        //Find next mesh data block in cpu file
        while (true)
        {
            //This is done instead of using BinaryReader::ReadUint32() because that method was incredibly slow (+ several minutes slow)
            if (cpuFileAsUintArray[cpuFileIndex] == meshCrc)
                break;

            cpuFileIndex++;
        }
        u64 meshDataBlockStart = (cpuFileIndex * 4) - 4;
        cpuFile.SeekBeg(meshDataBlockStart);


        //Read mesh data block. Contains info on vertex + index layout + size + format
        MeshDataBlock meshData;
        meshData.Read(cpuFile);
        cpuFileIndex += (cpuFile.Position() - meshDataBlockStart) / 4;

        terrain.Meshes.push_back(meshData);

        //Read index data
        gpuFile.Align(16); //Indices always start here
        u32 indicesSize = meshData.NumIndices * meshData.IndexSize;
        u8* indexBuffer = new u8[indicesSize];
        gpuFile.ReadToMemory(indexBuffer, indicesSize);
        terrain.Indices.push_back(std::span<u16>{ (u16*)indexBuffer, indicesSize / 2 });

        //Read vertex data
        gpuFile.Align(16);
        u32 verticesSize = meshData.NumVertices * meshData.VertexStride0;
        u8* vertexBuffer = new u8[verticesSize];
        gpuFile.ReadToMemory(vertexBuffer, verticesSize);
        terrain.Vertices.push_back(std::span<LowLodTerrainVertex>{ (LowLodTerrainVertex*)vertexBuffer, verticesSize / sizeof(LowLodTerrainVertex)});

        u32 endMeshCrc = gpuFile.ReadUint32();
        if (meshCrc != endMeshCrc)
            throw std::runtime_error("Error, verification hash at start of gpu file mesh data doesn't match hash end of gpu file mesh data!");
    }


    //Todo: Create D3D11 vertex buffers / shaders / whatever else is needed to render the terrain
    //Todo: Tell the renderer to render each terrain mesh each frame

    //Todo: Clear index + vertex buffers in RAM after they've been uploaded to the gpu

    //Clear resources
    container->Cleanup();
    delete container;
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();

    //Acquire resource lock before writing terrain instance data to the instance list
    std::lock_guard<std::mutex> lock(ResourceLock);
    TerrainInstances.push_back(terrain);
    NewTerrainInstanceAdded = true;
}

void MainGui::DrawMainMenuBar()
{
    //Todo: Make this actually work
    if (ImGui::BeginMainMenuBar())
    {
        ImGuiMenu("File",
            ImGuiMenuItemShort("Open file", )
            ImGuiMenuItemShort("Save file", )
            ImGuiMenuItemShort("Exit", )
        )
        ImGuiMenu("Help",
            ImGuiMenuItemShort("Welcome", )
            ImGuiMenuItemShort("Metrics", )
            ImGuiMenuItemShort("About", )
        )

        //Note: Not the preferred way of doing this with dear imgui but necessary for custom UI elements
        auto* drawList = ImGui::GetWindowDrawList();
        string framerate = std::to_string(ImGui::GetIO().Framerate);
        u64 decimal = framerate.find('.');
        const char* labelAndSeparator = "|    FPS: ";
        drawList->AddText(ImVec2(ImGui::GetCursorPosX(), 3.0f), 0xF2F5FAFF, labelAndSeparator, labelAndSeparator + strlen(labelAndSeparator));
        drawList->AddText(ImVec2(ImGui::GetCursorPosX() + 49.0f, 3.0f), ImGui::ColorConvertFloat4ToU32(gui::SecondaryTextColor), framerate.c_str(), framerate.c_str() + decimal + 3);

        ImGui::EndMainMenuBar();
    }
}

void MainGui::DrawDockspace()
{
    //Dockspace flags
    static ImGuiDockNodeFlags dockspace_flags = 0;
    
    //Parent window flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                                    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImVec2 dockspaceSize = viewport->GetWorkSize();
    dockspaceSize.y -= state_.statusBarHeight_;
    ImGui::SetNextWindowSize(dockspaceSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace parent window", &state_.Visible, window_flags);
    ImGui::PopStyleVar(3);
    
    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("Editor dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    
    ImGui::End();
}