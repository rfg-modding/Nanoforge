#include "TerritoryDocument.h"
#include "render/backend/DX11Renderer.h"
#include "common/filesystem/Path.h"
#include "util/MeshUtil.h"
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include "Log.h"
#include <span>

void TerritoryDocument_WorkerThread(GuiState* state, std::shared_ptr<Document> doc);
void WorkerThread_ClearData(std::shared_ptr<Document> doc);
void LoadTerrainMeshes(GuiState* state, std::shared_ptr<Document> doc);
void LoadTerrainMesh(FileHandle terrainMesh, Vec3 position, GuiState* state, std::shared_ptr<Document> doc);
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

void TerritoryDocument_Init(GuiState* state, std::shared_ptr<Document> doc)
{
    //Get parent packfile
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

    //Create scene instance and store index
    state->Renderer->CreateScene();
    data->Scene = state->Renderer->Scenes.back();

    //Init scene camera
    data->Scene->Cam.Init({ -2573.0f, 200.0f, 963.0f }, 80.0f, { (f32)data->Scene->Width(), (f32)data->Scene->Height() }, 1.0f, 10000.0f);
    data->Scene->SetShader(terrainShaderPath_);
    data->Scene->SetVertexLayout
    ({
        { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 } 
    });

    //Create worker thread to load terrain meshes in background
    data->WorkerFuture = std::async(std::launch::async, &TerritoryDocument_WorkerThread, state, doc);
}

void TerritoryDocument_DrawOverlayButtons(GuiState* state, std::shared_ptr<Document> doc);

void TerritoryDocument_Update(GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;
    if (!ImGui::Begin(doc->Title.c_str(), &doc->Open))
    {
        ImGui::End();
        return;
    }

    if (data->WorkerDone) //Once worker thread is done clear its temporary data
    {
        if (!data->WorkerResourcesFreed && !data->NewTerrainInstanceAdded)
        {
            WorkerThread_ClearData(doc);
            data->WorkerResourcesFreed = true;
        }
    }
    //Create dx11 resources for terrain meshes as they're loaded
    if (data->NewTerrainInstanceAdded)
    {
        std::lock_guard<std::mutex> lock(data->ResourceLock);
        //Create terrain index & vertex buffers
        for (auto& instance : data->TerrainInstances)
        {
            //Skip already-initialized terrain instances
            if (instance.RenderDataInitialized)
                continue;

            for (u32 i = 0; i < 9; i++)
            {
                auto& renderObject = data->Scene->Objects.emplace_back();
                Mesh mesh;
                mesh.Create(data->Scene->d3d11Device_, data->Scene->d3d11Context_, ToByteSpan(instance.Vertices[i]), ToByteSpan(instance.Indices[i]),
                    instance.Vertices[i].size(), DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                renderObject.Create(mesh, instance.Position);
            }

            //Set bool so the instance isn't initialized more than once
            instance.RenderDataInitialized = true;
        }
        data->NewTerrainInstanceAdded = false;
    }

    //Set current territory to most recently focused territory window
    if (ImGui::IsWindowFocused())
    {
        state->SetTerritory(data->TerritoryName);
        state->CurrentTerritory = &data->Territory;
        data->Scene->Cam.InputActive = true;

        //Move camera if triggered by another gui panel
        if (state->CurrentTerritoryCamPosNeedsUpdate)
        {
            Vec3 newPos = state->CurrentTerritoryNewCamPos;
            data->Scene->Cam.SetPosition(newPos.x, newPos.y, newPos.z);
            data->Scene->Cam.LookAt({ newPos.x - 250.0f, newPos.y - 500.0f, newPos.z - 250.f });
            state->CurrentTerritoryCamPosNeedsUpdate = false;
        }
    }
    else
    {
        data->Scene->Cam.InputActive = false;
    }

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;

    data->Scene->HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(data->Scene->ClearColor.x, data->Scene->ClearColor.y, data->Scene->ClearColor.z, data->Scene->ClearColor.w));
    ImGui::Image(data->Scene->GetView(), ImVec2(static_cast<f32>(data->Scene->Width()), static_cast<f32>(data->Scene->Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    TerritoryDocument_DrawOverlayButtons(state, doc);

    ImGui::End();
}

void TerritoryDocument_DrawOverlayButtons(GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_CAMERA))
        ImGui::OpenPopup("##CameraSettingsPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##CameraSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Camera");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        f32 fov = data->Scene->Cam.GetFov();
        f32 nearPlane = data->Scene->Cam.GetNearPlane();
        f32 farPlane = data->Scene->Cam.GetFarPlane();
        f32 lookSensitivity = data->Scene->Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) data->Scene->Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) data->Scene->Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) data->Scene->Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) data->Scene->Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) data->Scene->Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) data->Scene->Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &data->Scene->Cam.Speed);
        ImGui::InputFloat("Sprint speed", &data->Scene->Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::InputFloat("Fov", &fov))
            data->Scene->Cam.SetFov(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            data->Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            data->Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            data->Scene->Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&data->Scene->Cam.camPosition))
        {
            data->Scene->Cam.UpdateViewMatrix();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_SUN))
        ImGui::OpenPopup("##SceneSettingsPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##SceneSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Render settings");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        ImGui::Text("Shading mode: ");
        ImGui::SameLine();
        ImGui::RadioButton("Elevation", &data->Scene->perFrameStagingBuffer_.ShadeMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Diffuse", &data->Scene->perFrameStagingBuffer_.ShadeMode, 1);

        if (data->Scene->perFrameStagingBuffer_.ShadeMode != 0)
        {
            ImGui::Text("Diffuse presets: ");
            ImGui::SameLine();
            if (ImGui::Button("Default"))
            {
                data->Scene->perFrameStagingBuffer_.DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                data->Scene->perFrameStagingBuffer_.DiffuseIntensity = 0.65f;
                data->Scene->perFrameStagingBuffer_.ElevationFactorBias = 0.8f;
            }
            ImGui::SameLine();
            if (ImGui::Button("False color"))
            {
                data->Scene->perFrameStagingBuffer_.DiffuseColor = { 1.15f, 0.67f, 0.02f, 1.0f };
                data->Scene->perFrameStagingBuffer_.DiffuseIntensity = 0.55f;
                data->Scene->perFrameStagingBuffer_.ElevationFactorBias = -0.8f;
            }

            ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&data->Scene->perFrameStagingBuffer_.DiffuseColor));
            ImGui::SliderFloat("Diffuse intensity", &data->Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 1.0f);
            ImGui::SliderFloat("Elevation color bias", &data->Scene->perFrameStagingBuffer_.ElevationFactorBias, -1.0f, 1.0f);
        }

        ImGui::EndPopup();
    }
}

void TerritoryDocument_OnClose(GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

    if (state->CurrentTerritory == &data->Territory)
    {
        state->CurrentTerritory = nullptr;
        state->SetSelectedZone(0);
        state->SetSelectedZoneObject(nullptr);
    }

    //Free territory data
    data->Territory.ResetTerritoryData();

    //Delete scene and free its resources
    state->Renderer->DeleteScene(data->Scene);
    
    //Free document data
    delete data;
}

void TerritoryDocument_WorkerThread(GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

    //Read all zones from zonescript_terr01.vpp_pc
    state->SetStatus(ICON_FA_SYNC " Loading zones for " + doc->Title, Working);
    data->Territory.Init(state->PackfileVFS, data->TerritoryName, data->TerritoryShortname);
    data->Territory.LoadZoneData();
    state->CurrentTerritory = &data->Territory;
    state->SetSelectedZone(0);

    std::vector<ZoneData>& zoneFiles = data->Territory.ZoneFiles;
    Log->info("Loaded {} zones for {}", zoneFiles.size(), doc->Title);

    //Move camera close to zone with the most objects by default. Convenient as some territories have origins distant from each other
    if (zoneFiles.size() > 0 && zoneFiles[0].Zone.Objects.size() > 0)
    {
        //Tell camera to move to near the first object in the zone
        state->CurrentTerritoryNewCamPos = zoneFiles[0].Zone.Objects[0].Bmin + Vec3{ .x = 250.0f, .y = 500.0f, .z = 250.0f };
        state->CurrentTerritoryCamPosNeedsUpdate = true;
    }

    //Load terrain meshes and extract their index + vertex data
    LoadTerrainMeshes(state, doc);
    state->ClearStatus();
    data->WorkerDone = true;
}

//Loads vertex and index data of each zones terrain mesh
//Note: This function is run by the worker thread
void LoadTerrainMeshes(GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;
    state->SetStatus(ICON_FA_SYNC " Loading terrain meshes for " + doc->Title, Working);

    //Must store futures for std::async to run functions asynchronously
    std::vector<std::future<void>> futures;

    //Find terrain meshes and load them
    for (auto& zone : data->Territory.ZoneFiles)
    {
        //Get obj_zone object with a terrain_file_name property
        auto* objZoneObject = zone.Zone.GetSingleObject("obj_zone");
        if (!objZoneObject)
            continue;
        auto* terrainFilenameProperty = objZoneObject->GetProperty<StringProperty>("terrain_file_name");
        if (!terrainFilenameProperty)
            continue;

        //Remove extra null terminators that RFG so loves to have in it's files
        string filename = terrainFilenameProperty->Data;
        if (filename.ends_with('\0'))
            filename.pop_back();

        filename += ".cterrain_pc";
        Vec3 position = objZoneObject->Bmin + ((objZoneObject->Bmax - objZoneObject->Bmin) / 2.0f);
        auto terrainMeshHandleCpu = state->PackfileVFS->GetFiles(filename, true, true);
        if (terrainMeshHandleCpu.size() > 0)
            futures.push_back(std::async(std::launch::async, &LoadTerrainMesh, terrainMeshHandleCpu[0], position, state, std::ref(doc)));
    }
    
    //Wait for all threads to exit
    for (auto& future : futures)
        future.wait();

    Log->info("Done loading terrain meshes for {}", doc->Title);
}

void LoadTerrainMesh(FileHandle terrainMesh, Vec3 position, GuiState* state, std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

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

        //Free vertex buffer, no longer need this copy. verticesWithNormals copied the data it needed from this one
        delete[] vertexBuffer;

        u32 endMeshCrc = gpuFile.ReadUint32();
        if (meshCrc != endMeshCrc)
            THROW_EXCEPTION("Error, verification hash at start of gpu file mesh data doesn't match hash end of gpu file mesh data!");
    }

    //Clear resources
    delete container;
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();

    //Acquire resource lock before writing terrain instance data to the instance list
    std::lock_guard<std::mutex> lock(data->ResourceLock);
    data->TerrainInstances.push_back(terrain);
    data->NewTerrainInstanceAdded = true;
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
        Vec3 normal = e1.Cross(e2);

        //Todo: Make sure this isn't subtly wrong
        //Attempt to flip normal if it's pointing in wrong direction. Seems to result in correct normals
        if (normal.y < 0.0f)
        {
            normal.x *= -1.0f;
            normal.y *= -1.0f;
            normal.z *= -1.0f;
        }
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
void WorkerThread_ClearData(std::shared_ptr<Document> doc)
{
    TerritoryDocumentData* data = (TerritoryDocumentData*)doc->Data;

    Log->info("Temporary data cleared for {} terrain worker threads", doc->Title);
    for (auto& instance : data->TerrainInstances)
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
    data->TerrainInstances.clear();
}