#include "TerritoryDocument.h"
#include "render/backend/DX11Renderer.h"
#include "common/filesystem/Path.h"
#include "util/MeshUtil.h"
#include <RfgTools++\formats\zones\properties\primitive\StringProperty.h>
#include <RfgTools++\formats\textures\PegFile10.h>
#include "gui/documents/PegHelpers.h"
#include "RfgTools++/formats/meshes/TerrainLowLod.h"
#include "RfgTools++/formats/meshes/Terrain.h"
#include "Log.h"
#include <span>
#include "util/Profiler.h"

TerritoryDocument::TerritoryDocument(GuiState* state, string territoryName, string territoryShortname)
    : TerritoryName(territoryName), TerritoryShortname(territoryShortname)
{
    state_ = state;

    //Create scene instance and store index
    state->Renderer->CreateScene();
    Scene = state->Renderer->Scenes.back();

    //Init scene camera
    Scene->Cam.Init({ -2573.0f, 200.0f, 963.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->SetShader(LowLodTerrainShaderPath);
    Scene->SetVertexLayout
    ({
        { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    });
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.5f;

    //Create worker thread to load terrain meshes in background
    WorkerFuture = std::async(std::launch::async, &TerritoryDocument::WorkerThread, this, state);
}

TerritoryDocument::~TerritoryDocument()
{
    //Wait for worker thread to exit
    Open = false;
    WorkerFuture.wait();
    WorkerThread_ClearData();

    if (state_->CurrentTerritory == &Territory)
    {
        state_->CurrentTerritory = nullptr;
        state_->SetSelectedZoneObject(nullptr);
    }

    //Free territory data
    Territory.ResetTerritoryData();

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
}

void TerritoryDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();
    if (WorkerDone) //Once worker thread is done clear its temporary data
    {
        if (!WorkerResourcesFreed && !NewTerrainInstanceAdded)
        {
            WorkerThread_ClearData();
            WorkerResourcesFreed = true;
        }
    }
    //Create dx11 resources for terrain meshes as they're loaded
    if (NewTerrainInstanceAdded)
    {
        std::lock_guard<std::mutex> lock(ResourceLock);
        //Create terrain index & vertex buffers
        for (auto& instance : TerrainInstances)
        {
            //Skip already-initialized terrain instances
            if (instance.RenderDataInitialized)
                continue;

            for (u32 i = 0; i < 9; i++)
            {
                auto& renderObject = Scene->Objects.emplace_back();
                Mesh mesh;
                mesh.Create(Scene->d3d11Device_, Scene->d3d11Context_, ToByteSpan(instance.Vertices[i]), ToByteSpan(instance.Indices[i]),
                    instance.Vertices[i].size(), DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                renderObject.Create(mesh, instance.Position);

                if (instance.HasBlendTexture)
                {
                    //Create and setup texture2d
                    Texture2D texture2d;
                    texture2d.Name = Path::GetFileNameNoExtension(instance.Name) + "comb.cvbm_pc";
                    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_BC1_UNORM;
                    D3D11_SUBRESOURCE_DATA textureSubresourceData;
                    textureSubresourceData.pSysMem = instance.BlendTextureBytes.data();
                    textureSubresourceData.SysMemSlicePitch = 0;
                    textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, instance.BlendTextureWidth, instance.BlendTextureHeight);
                    state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
                    texture2d.Create(Scene->d3d11Device_, instance.BlendTextureWidth, instance.BlendTextureHeight, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
                    texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
                    texture2d.CreateSampler(); //Need sampler too
                    state->Renderer->ContextMutex.unlock();

                    renderObject.UseTextures = true;
                    renderObject.DiffuseTexture = texture2d;
                }
                if (instance.HasTexture1)
                {
                    //Create and setup texture2d
                    Texture2D texture2d;
                    texture2d.Name = Path::GetFileNameNoExtension(instance.Name) + "_ovl.cvbm_pc";
                    DXGI_FORMAT dxgiFormat = DXGI_FORMAT_BC1_UNORM;
                    D3D11_SUBRESOURCE_DATA textureSubresourceData;
                    textureSubresourceData.pSysMem = instance.Texture1Bytes.data();
                    textureSubresourceData.SysMemSlicePitch = 0;
                    textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, instance.Texture1Width, instance.Texture1Height);
                    state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
                    texture2d.Create(Scene->d3d11Device_, instance.Texture1Width, instance.Texture1Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
                    texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
                    texture2d.CreateSampler(); //Need sampler too
                    state->Renderer->ContextMutex.unlock();

                    renderObject.UseTextures = true;
                    renderObject.SpecularTexture = texture2d;
                }
            }

            //Set bool so the instance isn't initialized more than once
            instance.RenderDataInitialized = true;
        }
        Scene->NeedsRedraw = true; //Redraw scene if new terrain meshes added
        NewTerrainInstanceAdded = false;
    }

    //Set current territory to most recently focused territory window
    if (ImGui::IsWindowFocused())
    {
        state->SetTerritory(TerritoryName);
        state->CurrentTerritory = &Territory;
        Scene->Cam.InputActive = true;
    }
    else
    {
        Scene->Cam.InputActive = false;
    }

    //Move camera if triggered by another gui panel
    if (state->CurrentTerritoryCamPosNeedsUpdate && &Territory == state->CurrentTerritory)
    {
        Vec3 newPos = state->CurrentTerritoryNewCamPos;
        Scene->Cam.SetPosition(newPos.x, newPos.y, newPos.z);
        Scene->Cam.LookAt({ newPos.x - 250.0f, newPos.y - 500.0f, newPos.z - 250.f });
        Scene->NeedsRedraw = true;
        state->CurrentTerritoryCamPosNeedsUpdate = false;
    }
    //Update debug draw regardless of focus state since we'll never be focused when using the other panels which control debug draw
    state->CurrentTerritoryUpdateDebugDraw = true; //Now set to true permanently to support time based coloring. Quick hack, probably should remove this variable later.
    if (Territory.ZoneFiles.size() != 0 && state->CurrentTerritoryUpdateDebugDraw && TerritoryDataLoaded)
    {
        UpdateDebugDraw(state);
        PrimitivesNeedRedraw = false;
    }

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if (contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
        Scene->HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(Scene->ClearColor.x, Scene->ClearColor.y, Scene->ClearColor.z, Scene->ClearColor.w));
    ImGui::Image(Scene->GetView(), ImVec2(static_cast<f32>(Scene->Width()), static_cast<f32>(Scene->Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    DrawOverlayButtons(state);
}

void TerritoryDocument::Save(GuiState* state)
{

}

void TerritoryDocument::DrawOverlayButtons(GuiState* state)
{
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

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        f32 fov = Scene->Cam.GetFovDegrees();
        f32 nearPlane = Scene->Cam.GetNearPlane();
        f32 farPlane = Scene->Cam.GetFarPlane();
        f32 lookSensitivity = Scene->Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) Scene->Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) Scene->Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) Scene->Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) Scene->Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) Scene->Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) Scene->Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &Scene->Cam.Speed);
        ImGui::InputFloat("Sprint speed", &Scene->Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::SliderFloat("Fov", &fov, 40.0f, 120.0f))
            Scene->Cam.SetFovDegrees(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            Scene->Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&Scene->Cam.camPosition))
        {
            Scene->Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitchDegrees()));
        gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYawDegrees()));

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

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        ImGui::Text("Shading mode: ");
        ImGui::SameLine();
        ImGui::RadioButton("Elevation", &Scene->perFrameStagingBuffer_.ShadeMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Diffuse", &Scene->perFrameStagingBuffer_.ShadeMode, 1);

        if (Scene->perFrameStagingBuffer_.ShadeMode != 0)
        {
            ImGui::Text("Diffuse presets: ");
            ImGui::SameLine();
            if (ImGui::Button("Default"))
            {
                Scene->perFrameStagingBuffer_.DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.2;
                Scene->perFrameStagingBuffer_.ElevationFactorBias = 0.8f;
            }

            ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
            ImGui::SliderFloat("Diffuse intensity", &Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 2.0f);
        }

        ImGui::EndPopup();
    }
}

void TerritoryDocument::UpdateDebugDraw(GuiState* state)
{
    //Reset primitives first to ensure old primitives get cleared
    Scene->ResetPrimitives();

    //Draw bounding boxes
    for (const auto& zone : Territory.ZoneFiles)
    {
        if (!zone.RenderBoundingBoxes)
            continue;

        for (const auto& object : zone.Zone.Objects)
        {
            auto objectClass = Territory.GetObjectClass(object.ClassnameHash);
            if (!objectClass.Show)
                continue;

            //If object is selected in zone object list panel use different drawing method for visibilty
            bool selectedInZoneObjectList = &object == state->ZoneObjectList_SelectedObject;
            if (selectedInZoneObjectList)
            {
                //Calculate color that changes with time
                Vec3 color = objectClass.Color;
                f32 colorMagnitude = objectClass.Color.Magnitude();
                //Negative values used for brighter colors so they get darkened instead of lightened//Otherwise doesn't work on objects with white debug color
                f32 multiplier = colorMagnitude > 0.85f ? -1.0f : 1.0f;
                color.x = objectClass.Color.x + powf(sin(Scene->TotalTime * 2.0f), 2.0f) * multiplier;
                color.y = objectClass.Color.y + powf(sin(Scene->TotalTime), 2.0f) * multiplier;
                color.z = objectClass.Color.z + powf(sin(Scene->TotalTime), 2.0f) * multiplier;

                //Keep color in a certain range so it stays visible against the terrain
                f32 magnitudeMin = 0.20f;
                f32 colorMin = 0.20f;
                if (color.Magnitude() < magnitudeMin)
                {
                    color.x = std::max(color.x, colorMin);
                    color.y = std::max(color.y, colorMin);
                    color.z = std::max(color.z, colorMin);
                }

                //Calculate bottom center of box so we can draw a line from the bottom of the box into the sky
                Vec3 lineStart;
                lineStart.x = (object.Bmin.x + object.Bmax.x) / 2.0f;
                lineStart.y = object.Bmin.y;
                lineStart.z = (object.Bmin.z + object.Bmax.z) / 2.0f;
                Vec3 lineEnd = lineStart;
                lineEnd.y += 300.0f;

                //Draw object bounding box and line from it's bottom into the sky
                Scene->DrawBox(object.Bmin, object.Bmax, color);
                Scene->DrawLine(lineStart, lineEnd, color);
            }
            else //If not selected just draw bounding box with static color
            {
                Scene->DrawBox(object.Bmin, object.Bmax, objectClass.Color);
            }
        }
    }

    Scene->NeedsRedraw = true;
    state->CurrentTerritoryUpdateDebugDraw = false;
}

void TerritoryDocument::WorkerThread(GuiState* state)
{
    PROFILER_FUNCTION();
    //Wait for packfile thread to finish.
    while (!state->PackfileVFS || !state->PackfileVFS->Ready())
    {
        Sleep(50);
    }

    TerrainThreadTimer.Reset();

    //Read all zones from zonescript_terr01.vpp_pc
    state->SetStatus(ICON_FA_SYNC " Loading zones for " + Title, Working);
    Territory.Init(state->PackfileVFS, TerritoryName, TerritoryShortname);
    Territory.LoadZoneData();
    state->CurrentTerritoryUpdateDebugDraw = true;
    TerritoryDataLoaded = true;

    std::vector<ZoneData>& zoneFiles = Territory.ZoneFiles;
    Log->info("Loaded {} zones for {} in {} seconds", zoneFiles.size(), Title, TerrainThreadTimer.ElapsedSecondsPrecise());

    //Move camera close to zone with the most objects by default. Convenient as some territories have origins distant from each other
    if (zoneFiles.size() > 0 && zoneFiles[0].Zone.Objects.size() > 0)
    {
        //Tell camera to move to near the first object in the zone
        state->CurrentTerritoryNewCamPos = zoneFiles[0].Zone.Objects[0].Bmin + Vec3(250.0f, 500.0f, 250.0f);
        state->CurrentTerritoryCamPosNeedsUpdate = true;
    }

    //End worker thread early if document is closed
    if (!Open)
    {
        state->ClearStatus();
        return;
    }

    //Load terrain meshes and extract their index + vertex data
    WorkerThread_LoadTerrainMeshes(state);
    state->ClearStatus();
    WorkerDone = true;
}

void TerritoryDocument::WorkerThread_ClearData()
{
    Log->info("Temporary data cleared for {} terrain worker threads", Title);
    for (auto& instance : TerrainInstances)
    {
        //Free vertex and index buffer memory
        //Note: Assumes same amount of vertex and index buffers
        for (u32 i = 0; i < instance.Indices.size(); i++)
        {
            if(instance.Indices[i].data())
                delete instance.Indices[i].data();
            if(instance.Vertices[i].data())
                delete instance.Vertices[i].data();
        }
        //Clear vectors
        instance.Indices.clear();
        instance.Vertices.clear();

        //Clear blend texture data
        if (instance.HasBlendTexture)
        {
            //Cleanup peg texture data
            instance.BlendTextureBytes = std::span<u8>((u8*)nullptr, 0);
            instance.BlendPeg.Cleanup(); //Clears data the span pointed to
            instance.Texture1Bytes = std::span<u8>((u8*)nullptr, 0);
            instance.Texture1.Cleanup(); //Clears data the span pointed to
        }
    }
    //Clear instance list
    TerrainInstances.clear();
}

void TerritoryDocument::WorkerThread_LoadTerrainMeshes(GuiState* state)
{
    PROFILER_FUNCTION();
    state->SetStatus(ICON_FA_SYNC " Loading terrain meshes for " + Title, Working);

    //Must store futures for std::async to run functions asynchronously
    std::vector<std::future<void>> futures;

    //Find terrain meshes and load them
    for (auto& zone : Territory.ZoneFiles)
    {
        //Exit early if document closes
        if (!Open)
            break;

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

        //Exit early if document closes
        if (!Open)
            break;

        filename += ".cterrain_pc";
        Vec3 position = objZoneObject->Bmin + ((objZoneObject->Bmax - objZoneObject->Bmin) / 2.0f);
        auto terrainMeshHandleCpu = state->PackfileVFS->GetFiles(filename, true, true);
        if (terrainMeshHandleCpu.size() > 0)
            futures.push_back(std::async(std::launch::async, &TerritoryDocument::WorkerThread_LoadTerrainMesh, this, terrainMeshHandleCpu[0], position, state));
    }

    //Wait for all threads to exit
    for (auto& future : futures)
        future.wait();

    Log->info("Done loading terrain meshes for {}. Took {} seconds", Title, TerrainThreadTimer.ElapsedSecondsPrecise());
}

void TerritoryDocument::WorkerThread_LoadTerrainMesh(FileHandle terrainMesh, Vec3 position, GuiState* state)
{
    PROFILER_FUNCTION();
    PROFILER_SET_THREAD_NAME((terrainMesh.Filename() + " terrain worker").c_str());

    //Get packfile that holds terrain meshes
    auto* container = terrainMesh.GetContainer();
    if (!container)
        THROW_EXCEPTION("Failed to get container pointer for a terrain mesh.");

    //Todo: This does a full extract twice on the container due to the way single file extracts work. Fix this
    //Get mesh file byte arrays
    auto cpuFileBytes = container->ExtractSingleFile(terrainMesh.Filename(), true);
    auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(terrainMesh.Filename()) + ".gterrain_pc", true);

    //Ensure the mesh files were extracted
    if (!cpuFileBytes)
        THROW_EXCEPTION("Failed to extract terrain mesh cpu file.");
    if (!gpuFileBytes)
        THROW_EXCEPTION("Failed to extract terrain mesh gpu file.");

    BinaryReader cpuFile(cpuFileBytes.value());
    BinaryReader gpuFile(gpuFileBytes.value());

    //Create new instance
    TerrainInstance terrain;
    terrain.Position = position;
    terrain.DataLowLod.Read(cpuFile, terrainMesh.Filename());

    //Get vertex data. Each terrain file is made up of 9 meshes which are stitched together
    u32 cpuFileIndex = 0;
    u32* cpuFileAsUintArray = (u32*)cpuFileBytes.value().data();
    for (u32 i = 0; i < 9; i++)
    {
        //Exit early if document closes
        if (!Open)
            break;

        std::optional<MeshInstanceData> meshData = terrain.DataLowLod.ReadSubmeshData(gpuFile, i);
        if (!meshData.has_value())
            THROW_EXCEPTION("Failed to read submesh {} from {}.", i, terrainMesh.Filename());

        terrain.Indices.push_back(ConvertSpan<u8, u16>(meshData.value().IndexBuffer));
        terrain.Vertices.push_back(ConvertSpan<u8, LowLodTerrainVertex>(meshData.value().VertexBuffer));
    }

    //Clear resources
    delete container;
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();

    //Exit early if document closes
    if (!Open)
        return;

    //Get comb texture. Used to color low lod terrain
    string blendTextureName = Path::GetFileNameNoExtension(terrainMesh.Filename()) + "comb.cvbm_pc";
    terrain.HasBlendTexture = WorkerThread_FindTexture(state->PackfileVFS, blendTextureName, terrain.BlendPeg, terrain.BlendTextureBytes, terrain.BlendTextureWidth, terrain.BlendTextureHeight);

    //Get ovl texture. Used to light low lod terrain
    string texture1Name = Path::GetFileNameNoExtension(terrainMesh.Filename()) + "_ovl.cvbm_pc";
    terrain.HasTexture1 = WorkerThread_FindTexture(state->PackfileVFS, texture1Name, terrain.Texture1, terrain.Texture1Bytes, terrain.Texture1Width, terrain.Texture1Height);

    //Acquire resource lock before writing terrain instance data to the instance list
    std::lock_guard<std::mutex> lock(ResourceLock);
    TerrainInstances.push_back(terrain);
    NewTerrainInstanceAdded = true;
}

bool TerritoryDocument::WorkerThread_FindTexture(PackfileVFS* vfs, const string& textureName, PegFile10& peg, std::span<u8>& textureBytes, u32& textureWidth, u32& textureHeight)
{
    //Search for texture
    std::vector<FileHandle> search = vfs->GetFiles(textureName, true, true);
    if (search.size() == 0)
    {
        Log->warn("Couldn't find {} for {}.", textureName, TerritoryName);
        return false;
    }

    FileHandle& handle = search[0];
    auto* container = handle.GetContainer();
    if (!container)
        THROW_EXCEPTION("Failed to get container pointer for a terrain mesh.");

    //Get mesh file byte arrays
    auto cpuFileBytes = container->ExtractSingleFile(textureName, true);
    auto gpuFileBytes = container->ExtractSingleFile(Path::GetFileNameNoExtension(textureName) + ".gvbm_pc", true);

    //Ensure the texture files were extracted
    if (!cpuFileBytes)
        THROW_EXCEPTION("Failed to extract texture cpu file {}", textureName);
    if (!gpuFileBytes)
        THROW_EXCEPTION("Failed to extract texture gpu file {}", textureName);

    BinaryReader gpuFile(cpuFileBytes.value());
    BinaryReader cpuFile(gpuFileBytes.value());

    peg.Read(gpuFile, cpuFile);
    peg.ReadTextureData(cpuFile, peg.Entries[0]);
    auto maybeTexturePixelData = peg.GetTextureData(0);
    if (maybeTexturePixelData)
    {
        textureBytes = maybeTexturePixelData.value();
        textureWidth = peg.Entries[0].Width;
        textureHeight = peg.Entries[0].Height;
    }
    else
    {
        Log->warn("Failed to extract pixel data for terrain blend texture {}", textureName);
    }

    delete container;
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();
    return true;
}
