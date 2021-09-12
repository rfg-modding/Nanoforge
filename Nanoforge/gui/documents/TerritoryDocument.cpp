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
    Scene->Cam.Init({ 250.0f, 500.0f, 250.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    //Scene->SetShader(LowLodTerrainShaderPath);
    //Scene->SetVertexLayout
    //({
    //    { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //});
    Scene->SetShader(TerrainShaderPath);
    Scene->SetVertexLayout
    ({
        { "POSITION", 0,  DXGI_FORMAT_R16G16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 4, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    });
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.5f;

    //Start territory loading thread
    Territory.Init(state->PackfileVFS, TerritoryName, TerritoryShortname);
    state->CurrentTerritoryUpdateDebugDraw = true;
    TerritoryLoadTask = Territory.LoadAsync(state);
}

TerritoryDocument::~TerritoryDocument()
{
    //Wait for worker thread to exit
    Open = false;
    Territory.StopLoadThread();
    TerritoryLoadTask->Wait();
    if(!WorkerResourcesFreed)
        Territory.ClearLoadThreadData();

    if (state_->CurrentTerritory == &Territory)
    {
        state_->CurrentTerritory = nullptr;
        state_->SetSelectedZoneObject(nullptr);
    }

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
}

void TerritoryDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    //Create dx11 resources for terrain meshes as they're loaded
    for (auto& instance : Territory.TerrainInstances)
    {
        //Skip already-initialized terrain instances
        if (!instance.NeedsRenderInit)
            continue;

        //Initialize low lod terrain
        //for (u32 i = 0; i < 9; i++)
        //{
        //    auto& renderObject = Scene->Objects.emplace_back();
        //    Mesh mesh;
        //    mesh.Create(Scene->d3d11Device_, Scene->d3d11Context_, instance.LowLodMeshes[i]);
        //    renderObject.Create(mesh, instance.Position);

        //    if (instance.HasBlendTexture)
        //    {
        //        //Create and setup texture2d
        //        Texture2D texture2d;
        //        texture2d.Name = Path::GetFileNameNoExtension(instance.Name) + "comb.cvbm_pc";
        //        DXGI_FORMAT dxgiFormat = DXGI_FORMAT_BC1_UNORM;
        //        D3D11_SUBRESOURCE_DATA textureSubresourceData;
        //        textureSubresourceData.pSysMem = instance.BlendTextureBytes.data();
        //        textureSubresourceData.SysMemSlicePitch = 0;
        //        textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, instance.BlendTextureWidth, instance.BlendTextureHeight);
        //        state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
        //        texture2d.Create(Scene->d3d11Device_, instance.BlendTextureWidth, instance.BlendTextureHeight, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
        //        texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
        //        texture2d.CreateSampler(); //Need sampler too
        //        state->Renderer->ContextMutex.unlock();

        //        renderObject.UseTextures = true;
        //        renderObject.DiffuseTexture = texture2d;
        //    }
        //    if (instance.HasTexture1)
        //    {
        //        //Create and setup texture2d
        //        Texture2D texture2d;
        //        texture2d.Name = Path::GetFileNameNoExtension(instance.Name) + "_ovl.cvbm_pc";
        //        DXGI_FORMAT dxgiFormat = DXGI_FORMAT_BC1_UNORM;
        //        D3D11_SUBRESOURCE_DATA textureSubresourceData;
        //        textureSubresourceData.pSysMem = instance.Texture1Bytes.data();
        //        textureSubresourceData.SysMemSlicePitch = 0;
        //        textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, instance.Texture1Width, instance.Texture1Height);
        //        state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
        //        texture2d.Create(Scene->d3d11Device_, instance.Texture1Width, instance.Texture1Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
        //        texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
        //        texture2d.CreateSampler(); //Need sampler too
        //        state->Renderer->ContextMutex.unlock();

        //        renderObject.UseTextures = true;
        //        renderObject.SpecularTexture = texture2d;
        //    }
        //}

        //Initialize high lod terrain
        for (u32 i = 0; i < 9; i++)
        {
            auto& subzone = instance.Subzones[i];
            auto& renderObject = Scene->Objects.emplace_back();
            Mesh mesh;
            mesh.Create(Scene->d3d11Device_, subzone.InstanceData, 1);
            renderObject.Create(mesh, subzone.Data.Subzone.Position);

            //Todo: ***********REMOVE BEFORE COMMIT. THIS SHOULD BE IN THE LOW LOD TERRAIN SECTION. ONLY HERE FOR TESTING*************************
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
                texture2d.Create(Scene->d3d11Device_, instance.BlendTextureWidth, instance.BlendTextureHeight, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
                texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
                texture2d.CreateSampler(); //Need sampler too

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
                texture2d.Create(Scene->d3d11Device_, instance.Texture1Width, instance.Texture1Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
                texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
                texture2d.CreateSampler(); //Need sampler too

                renderObject.UseTextures = true;
                renderObject.SpecularTexture = texture2d;
            }
        }

        //Set bool so the instance isn't initialized more than once
        instance.NeedsRenderInit = false;
        Scene->NeedsRedraw = true; //Redraw scene if new terrain meshes added
    }

    //Once worker thread is done clear its temporary data
    if (!Territory.LoadThreadRunning() && !WorkerResourcesFreed)
    {
        Territory.ClearLoadThreadData();
        WorkerResourcesFreed = true;
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
    if (Territory.ZoneFiles.size() != 0 && state->CurrentTerritoryUpdateDebugDraw && Territory.Ready())
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