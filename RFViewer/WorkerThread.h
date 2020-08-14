#pragma once
#include "gui/GuiState.h"
#include "rfg/TerrainHelpers.h"
#include "common/filesystem/Path.h"
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include "Log.h"
#include <future>

std::vector<TerrainInstance> TerrainInstances = {};

//Lock used to make sure the worker thread and main thread aren't using resources at the same time
std::mutex ResourceLock;
//Tells the main thread that the worker thread pushed a new terrain instance that needs to be uploaded to the gpu
bool NewTerrainInstanceAdded = false;
//Set this to false to test the init sequence. It will wait for this to be set to true to start init. F1 sets this to true
bool CanStartInit = true;

void LoadTerrainMeshes(GuiState* state);
void LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position, GuiState* state);

void WorkerThread(GuiState* state)
{
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);
    while (!CanStartInit)
    {
        Sleep(100);
    }

    //Scan contents of packfiles
    state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
    state->PackfileVFS->ScanPackfiles();
    Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());

    //Todo: Load all zone files in all vpps and str2s. Someone organize them by purpose/area. Maybe by territory
    //Read all zones from zonescript_terr01.vpp_pc
    state->SetStatus(ICON_FA_SYNC " Loading zones", Working);
    state->ZoneManager->LoadZoneData();
    state->SetSelectedZone(0);
    Log->info("Loaded {} zones", state->ZoneManager->ZoneFiles.size());

    //Load terrain meshes and extract their index + vertex data
    LoadTerrainMeshes(state);

    state->ClearStatus();
}

//Loads vertex and index data of each zones terrain mesh
//Note: This function is run by the worker thread
void LoadTerrainMeshes(GuiState* state)
{
    state->SetStatus(ICON_FA_SYNC " Locating terrain files", Working);

    //Todo: Split this into it's own function(s)
    //Get terrain mesh files for loaded zones and load them
    std::vector<string> terrainCpuFileNames = {};
    std::vector<string> terrainGpuFileNames = {};
    std::vector<Vec3> terrainPositions = {};

    //Create list of terrain meshes we need from obj_zone objects
    for (auto& zone : state->ZoneManager->ZoneFiles)
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
    auto terrainMeshHandlesCpu = state->PackfileVFS->GetFiles(terrainCpuFileNames, true, true);
    Log->info("Found {} terrain meshes. Loading...", terrainMeshHandlesCpu.size());
    for (auto& name : terrainCpuFileNames)
    {
        Log->info("Terrain cpu file found: {}", name);
    }

    state->SetStatus(ICON_FA_SYNC " Loading terrain meshes", Working);

    //Todo: Make multithreaded loading optional
    std::vector<std::future<void>> futures;
    u32 terrainMeshIndex = 0;
    for (auto& terrainMesh : terrainMeshHandlesCpu)
    {
        futures.push_back(std::async(std::launch::async, &LoadTerrainMesh, terrainMesh, terrainPositions[terrainMeshIndex], state));
        terrainMeshIndex++;
    }
    for (auto& future : futures)
    {
        future.wait();
    }
    Log->info("Done loading terrain meshes");
}

void LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position, GuiState* state)
{
    //Get packfile that holds terrain meshes
    auto* container = terrainMesh.GetContainer();
    if (!container)
        THROW_EXCEPTION("Error! Failed to get container ptr for a terrain mesh.");

    //Todo: This does a full extract twice on the container due to the way single file extracts work. Fix this
    //Get mesh file byte arrays
    auto cpuFileBytes = container->ExtractSingleFile(terrainMesh.Filename(), true);
    auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc", true);

    //Ensure the mesh files were extracted
    if (!cpuFileBytes)
        THROW_EXCEPTION("Error! Failed to get terrain mesh cpu file byte array!");
    if (!gpuFileBytes)
        THROW_EXCEPTION("Error! Failed to get terrain mesh gpu file byte array!");

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
            THROW_EXCEPTION("Error! Failed to read next mesh data block hash in terrain gpu file.");

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
            THROW_EXCEPTION("Error, verification hash at start of gpu file mesh data doesn't match hash end of gpu file mesh data!");
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