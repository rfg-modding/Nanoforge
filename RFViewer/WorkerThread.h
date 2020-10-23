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
//Signals that the worker thread is done so any worker resources can be cleared once the rest of the program is done with them
bool WorkerDone = false;

void LoadTerrainMeshes(GuiState* state);
void LoadTerrainMesh(FileHandle& terrainMesh, Vec3& position, GuiState* state);
struct ShortVec4
{
    i16 x = 0;
    i16 y = 0;
    i16 z = 0;
    i16 w = 0;

    ShortVec4 operator-(const ShortVec4& B)
    {
        return ShortVec4{ x - B.x, y - B.y, z - B.z, w - B.w };
    }
};
std::span<LowLodTerrainVertex> GenerateTerrainNormals(std::span<ShortVec4> vertices, std::span<u16> indices);

void WorkerThread(GuiState* state, bool reload)
{
    WorkerDone = false;
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);
    while (!CanStartInit)
    {
        Sleep(100);
    }

    //We only need to scan the packfiles once. Packfile data is independent from our current territory
    if (!reload)
    {
        //Scan contents of packfiles
        state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
        state->PackfileVFS->ScanPackfiles();
        Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());
    }

    //Todo: Load all zone files in all vpps and str2s. Someone organize them by purpose/area. Maybe by territory
    //Read all zones from zonescript_terr01.vpp_pc
    state->SetStatus(ICON_FA_SYNC " Loading zones", Working);
    state->ZoneManager->LoadZoneData();
    state->SetSelectedZone(0);
    Log->info("Loaded {} zones", state->ZoneManager->ZoneFiles.size());

    //Move camera close to zone with the most objects by default. Convenient as some territories have origins distant from each other
    if (state->ZoneManager->ZoneFiles.size() > 0)
    {
        ZoneFile& zone = state->ZoneManager->ZoneFiles[0];
        if (zone.Zone.Objects.size() > 0)
        {
            auto& firstObj = zone.Zone.Objects[0];
            Vec3 newCamPos = firstObj.Bmin;
            newCamPos.x += 100.0f;
            newCamPos.y += 500.0f;
            newCamPos.z += 100.0f;
            state->Camera->SetPosition(newCamPos.x, newCamPos.y, newCamPos.z);
        }
    }

    //Load terrain meshes and extract their index + vertex data
    LoadTerrainMeshes(state);

    state->ClearStatus();
    WorkerDone = true;
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
        cpuFileIndex += static_cast<u32>(cpuFile.Position() - meshDataBlockStart) / 4;

        terrain.Meshes.push_back(meshData);

        //Read index data
        gpuFile.Align(16); //Indices always start here
        u32 indicesSize = meshData.NumIndices * meshData.IndexSize;
        u8* indexBuffer = new u8[indicesSize];
        gpuFile.ReadToMemory(indexBuffer, indicesSize);
        terrain.Indices.push_back(std::span<u16>{ (u16*)indexBuffer, indicesSize / meshData.IndexSize });

        //Read vertex data
        gpuFile.Align(16);
        u32 verticesSize = meshData.NumVertices * meshData.VertexStride0;
        u8* vertexBuffer = new u8[verticesSize];
        gpuFile.ReadToMemory(vertexBuffer, verticesSize);

        std::span<LowLodTerrainVertex> verticesWithNormals = GenerateTerrainNormals
        (
            std::span<ShortVec4>{ (ShortVec4*)vertexBuffer, verticesSize / meshData.VertexStride0},
            std::span<u16>{ (u16*)indexBuffer, indicesSize / meshData.IndexSize }
        );
        terrain.Vertices.push_back(verticesWithNormals);

        u32 endMeshCrc = gpuFile.ReadUint32();
        if (meshCrc != endMeshCrc)
            THROW_EXCEPTION("Error, verification hash at start of gpu file mesh data doesn't match hash end of gpu file mesh data!");
    }

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

std::span<LowLodTerrainVertex> GenerateTerrainNormals(std::span<ShortVec4> vertices, std::span<u16> indices)
{
    struct Face
    {
        u32 verts[3];
    };

    //Generate list of faces and face normals
    std::vector<Face> faces = {};
    for (u32 i = 0; i < indices.size() - 3; i++)
    {
        u32 index0 = indices[i];
        u32 index1 = indices[i + 1];
        u32 index2 = indices[i + 2];

        faces.emplace_back(Face{ .verts = {index0, index1, index2} });
    }

    //Generate list of vertices with position and normal data
    u8* vertBuffer = new u8[vertices.size() * sizeof(LowLodTerrainVertex)];
    std::span<LowLodTerrainVertex> outVerts((LowLodTerrainVertex*)vertBuffer, vertices.size());
    for (u32 i = 0; i < vertices.size(); i++)
    {
        outVerts[i].x = vertices[i].x;
        outVerts[i].y = vertices[i].y;
        outVerts[i].z = vertices[i].z;
        outVerts[i].w = vertices[i].w;
        outVerts[i].normal = { 0.0f, 0.0f, 0.0f };
    }
    for (auto& face : faces)
    {
        const u32 ia = face.verts[0];
        const u32 ib = face.verts[1];
        const u32 ic = face.verts[2];

        Vec3 vert0 = { (f32)vertices[ia].x, (f32)vertices[ia].y, (f32)vertices[ia].z };
        Vec3 vert1 = { (f32)vertices[ib].x, (f32)vertices[ib].y, (f32)vertices[ib].z };
        Vec3 vert2 = { (f32)vertices[ic].x, (f32)vertices[ic].y, (f32)vertices[ic].z };

        Vec3 e1 = vert1 - vert0;
        Vec3 e2 = vert2 - vert1;
        Vec3 normal = e2.Cross(e1);
        //Todo: Fix normal vector calculation. This code below is a temporary fix but I'm pretty certain this normals aren't perfectly correct.
        //Fudge the normals. They don't get calculated otherwise. This isn't correct but creates decent results
        if (normal.y < 0.0f)
        {
            normal.x *= -1.0f;
            normal.y *= -1.0f;
            normal.z *= -1.0f;
        }
        //normal = normal.Normalize();
        outVerts[ia].normal += normal;
        outVerts[ib].normal += normal;
        outVerts[ic].normal += normal;
    }
    for (u32 i = 0; i < vertices.size(); i++)
    {
        outVerts[i].normal = outVerts[i].normal.Normalize();
    }
    return outVerts;
}

//Clear temporary data created by the worker thread. Called by Application once the worker thread is done working and the renderer is done using the worker data
void WorkerThread_ClearData()
{
    Log->info("Worker thread temporary data cleared.");
    for (auto& instance : TerrainInstances)
    {
        //Free vertex and index buffer memory
        //Note: Assumes same amount of vertex and index buffers
        for (u32 i = 0; i < instance.Indices.size(); i++)
        {
            delete instance.Indices[i].data();
            delete instance.Vertices[i].data();
        }
        //Clear vectors
        instance.Indices.clear();
        instance.Vertices.clear();
        instance.Meshes.clear();
    }
    //Clear instance list
    TerrainInstances.clear();
}