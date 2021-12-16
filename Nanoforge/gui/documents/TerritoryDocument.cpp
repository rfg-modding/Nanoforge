#include "TerritoryDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/Profiler.h"
#include "render/Render.h"
#include "Log.h"
#include "gui/GuiState.h"
#include "rfg/TerrainHelpers.h"
#include "util/TaskScheduler.h"
#include "render/resources/Scene.h"
#include "application/Config.h"
#include "render/imgui/ImGuiFontManager.h"
#include <imgui.h>

CVar CVar_DisableHighQualityTerrain("Disable high quality terrain", ConfigType::Bool,
    "If true high lod terrain won't be used in the territory viewer. You must close and re-open any viewers after changing this for it to go into effect.",
    ConfigValue(false),
    true,  //ShowInSettings
    false, //IsFolderPath
    false //IsFilePath
);

TerritoryDocument::TerritoryDocument(GuiState* state, std::string_view territoryName, std::string_view territoryShortname)
    : TerritoryName(territoryName), TerritoryShortname(territoryShortname)
{
    state_ = state;
    NoWindowPadding = true;

    //Determine if high lod terrain should be used
    useHighLodTerrain_ = !CVar_DisableHighQualityTerrain.Get<bool>();

    //Create scene
    Scene = state->Renderer->CreateScene();

    //Init scene camera
    Scene->Cam.Init({ 250.0f, 500.0f, 250.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.2f;

    //Start territory loading thread
    Territory.Init(state->PackfileVFS, TerritoryName, TerritoryShortname, useHighLodTerrain_);
    TerritoryLoadTask = Territory.LoadAsync(Scene, state);
    state->CurrentTerritoryUpdateDebugDraw = true;
}

TerritoryDocument::~TerritoryDocument()
{
    if (state_->CurrentTerritory == &Territory)
    {
        state_->CurrentTerritory = nullptr;
        state_->SetSelectedZoneObject(nullptr);
    }

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
}

bool TerritoryDocument::CanClose()
{
    //Can't close until the worker threads are finished. The doc will still disappear if the user closes it. It just won't get deleted until all the threads finish.
    return !TerritoryLoadTask->Running();
}

void TerritoryDocument::OnClose(GuiState* state)
{
    Territory.StopLoadThread();
}

void TerritoryDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

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

    //Force visibility update if new terrain instances are loaded
    if (numTerrainInstances_ != Territory.TerrainInstances.size())
    {
        numTerrainInstances_ = (u32)Territory.TerrainInstances.size();
        Scene->NeedsRedraw = true;
        terrainVisiblityUpdateNeeded_ = true;
    }

    //Update high/low lod mesh visibility based on camera distance
    if ((ImGui::IsWindowFocused() || terrainVisiblityUpdateNeeded_) && useHighLodTerrain_)
    {
        std::lock_guard<std::mutex> lock(Territory.TerrainLock);
        f32 highLodTerrainDistance = highLodTerrainEnabled_ ? highLodTerrainDistance_ : -1.0f;
        for (auto& terrain : Territory.TerrainInstances)
        {
            u32 numMeshes = (u32)std::min({ terrain.LowLodMeshes.size(), terrain.HighLodMeshes.size(), terrain.Subzones.size()});
            for (u32 i = 0; i < numMeshes; i++)
            {
                //Calc distance from camera ignoring Y so elevation doesn't matter
                auto& subzone = terrain.Subzones[i];
                Vec2 subzonePos = subzone.Subzone.Position.XZ();
                Vec2 cameraPos = Scene->Cam.PositionVec3().XZ();
                f32 distanceFromCamera = subzonePos.Distance(cameraPos);

                //Determine if low or high lod meshes should be used
                bool lowLodVisible = distanceFromCamera > highLodTerrainDistance;
                bool highLodVisible = !lowLodVisible;

                //Set mesh visibility
                terrain.LowLodMeshes[i]->Visible = lowLodVisible;
                terrain.HighLodMeshes[i]->Visible = highLodVisible;
                for (auto& stitchMesh : terrain.StitchMeshes)
                    if (stitchMesh.SubzoneIndex == i)
                        stitchMesh.Mesh->Visible = highLodVisible;
                for (auto& roadMesh : terrain.RoadMeshes)
                    if (roadMesh.SubzoneIndex == i)
                        roadMesh.Mesh->Visible = highLodVisible;
            }

        }
        terrainVisiblityUpdateNeeded_ = false;
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
        Scene->HandleResize((u32)contentAreaSize.x, (u32)contentAreaSize.y);

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

#pragma warning(disable:4100)
void TerritoryDocument::Save(GuiState* state)
{

}
#pragma warning(default:4100)

void TerritoryDocument::DrawOverlayButtons(GuiState* state)
{
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_CAMERA))
        ImGui::OpenPopup("##CameraSettingsPopup");
    state->FontManager->FontL.Pop();

    //Must manually set padding here since the parent window has padding disabled to get the viewport flush with the window border.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 8.0f });
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
                Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.2f;
                Scene->perFrameStagingBuffer_.ElevationFactorBias = 0.8f;
            }

            ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
            ImGui::SliderFloat("Diffuse intensity", &Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 2.0f);
        }

        if (useHighLodTerrain_)
        {
            if (ImGui::Checkbox("High lod terrain enabled", &highLodTerrainEnabled_))
            {
                terrainVisiblityUpdateNeeded_ = true;
            }
            if (highLodTerrainEnabled_)
            {
                ImGui::SliderFloat("High lod distance", &highLodTerrainDistance_, 0.0f, 10000.0f);
                ImGui::SameLine();
                gui::HelpMarker("Beyond this distance from the camera low lod terrain is used.", ImGui::GetIO().FontDefault);
                terrainVisiblityUpdateNeeded_ = true;
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_INFO_CIRCLE))
        ImGui::OpenPopup("##InfoPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##InfoPopup"))
    {
        //Header / general data
        state->FontManager->FontL.Push();
        ImGui::Text("Mesh info");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        if (ImGui::TreeNode("Instances"))
        {
            for (TerrainInstance& instance : Territory.TerrainInstances)
            {
                if (ImGui::TreeNode(fmt::format("{}##{}", instance.Name, (u64)&instance).c_str()))
                {
                    if (ImGui::TreeNode("Low lod meshes"))
                    {
                        TerrainLowLod& lowLod = instance.DataLowLod;
                        if (ImGui::TreeNode("Materials"))
                        {
                            for (RfgMaterial& material : lowLod.Materials)
                            {
                                std::optional<string> nameGuess = HashGuesser::GuessHashOriginString(material.NameChecksum);
                                string name = nameGuess.value_or(std::to_string(material.NameChecksum)) + "##" + std::to_string((u64)&material);
                                if (ImGui::TreeNode(name.c_str()))
                                {
                                    gui::LabelAndValue("Shader handle:", std::to_string(material.ShaderHandle));
                                    gui::LabelAndValue("Name checksum:", std::to_string(material.NameChecksum));
                                    gui::LabelAndValue("Material flags:", std::to_string(material.MaterialFlags));
                                    gui::LabelAndValue("Num textures:", std::to_string(material.NumTextures));
                                    gui::LabelAndValue("Num constants:", std::to_string(material.NumConstants));
                                    gui::LabelAndValue("Max constants:", std::to_string(material.MaxConstants));
                                    gui::LabelAndValue("Texture offset:", std::to_string(material.TextureOffset));
                                    gui::LabelAndValue("Constant name checksums offset:", std::to_string(material.ConstantNameChecksumsOffset));
                                    gui::LabelAndValue("Constant block offset:", std::to_string(material.ConstantBlockOffset));

                                    if (ImGui::TreeNode("Constants"))
                                    {
                                        for (u32 i = 0; i < material.NumConstants; i++)
                                        {
                                            MaterialConstant& constant = material.Constants[i];
                                            const u32 constantNameChecksum = material.ConstantNameChecksums[i];
                                            std::optional<string> constantNameGuess = HashGuesser::GuessHashOriginString(constantNameChecksum);
                                            string constantName = constantNameGuess.value_or(std::to_string(constantNameChecksum));
                                            ImGui::InputFloat4(constantName.c_str(), constant.Constants);
                                        }
                                        ImGui::TreePop();
                                    }

                                    //Material texture descriptions
                                    std::vector<TextureDesc> TextureDescs = {};

                                    //Constant name checksums
                                    std::vector<u32> ConstantNameChecksums = {};

                                    //Constant values
                                    std::vector<MaterialConstant> Constants = {};

                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }
                        if (ImGui::TreeNode("Minimap material"))
                        {
                            RfgMaterial& material = lowLod.MinimapMaterials;
                            std::optional<string> nameGuess = HashGuesser::GuessHashOriginString(material.NameChecksum);
                            string name = nameGuess.value_or(std::to_string(material.NameChecksum)) + "##" + std::to_string((u64)&material);
                            if (ImGui::TreeNode(name.c_str()))
                            {
                                gui::LabelAndValue("Shader handle:", std::to_string(material.ShaderHandle));
                                gui::LabelAndValue("Name checksum:", std::to_string(material.NameChecksum));
                                gui::LabelAndValue("Material flags:", std::to_string(material.MaterialFlags));
                                gui::LabelAndValue("Num textures:", std::to_string(material.NumTextures));
                                gui::LabelAndValue("Num constants:", std::to_string(material.NumConstants));
                                gui::LabelAndValue("Max constants:", std::to_string(material.MaxConstants));
                                gui::LabelAndValue("Texture offset:", std::to_string(material.TextureOffset));
                                gui::LabelAndValue("Constant name checksums offset:", std::to_string(material.ConstantNameChecksumsOffset));
                                gui::LabelAndValue("Constant block offset:", std::to_string(material.ConstantBlockOffset));

                                if (ImGui::TreeNode("Constants"))
                                {
                                    for (u32 i = 0; i < material.NumConstants; i++)
                                    {
                                        MaterialConstant& constant = material.Constants[i];
                                        const u32 constantNameChecksum = material.ConstantNameChecksums[i];
                                        std::optional<string> constantNameGuess = HashGuesser::GuessHashOriginString(constantNameChecksum);
                                        string constantName = constantNameGuess.value_or(std::to_string(constantNameChecksum));
                                        ImGui::InputFloat4(constantName.c_str(), constant.Constants);
                                    }
                                    ImGui::TreePop();
                                }

                                //Material texture descriptions
                                std::vector<TextureDesc> TextureDescs = {};

                                //Constant name checksums
                                std::vector<u32> ConstantNameChecksums = {};

                                //Constant values
                                std::vector<MaterialConstant> Constants = {};

                                ImGui::TreePop();
                            }
                            ImGui::TreePop();
                        }
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNode("High lod meshes"))
                    {
                        for (Terrain& highLod : instance.Subzones)
                        {
                            if (ImGui::TreeNode(fmt::format("{}##{}", highLod.Name, (u64)&highLod).c_str()))
                            {
                                if (ImGui::TreeNode("Road materials"))
                                {
                                    for (RfgMaterial& material : highLod.RoadMaterials)
                                    {
                                        std::optional<string> nameGuess = HashGuesser::GuessHashOriginString(material.NameChecksum);
                                        string name = nameGuess.value_or(std::to_string(material.NameChecksum)) + "##" + std::to_string((u64)&material);
                                        if (ImGui::TreeNode(name.c_str()))
                                        {
                                            gui::LabelAndValue("Shader handle:", std::to_string(material.ShaderHandle));
                                            gui::LabelAndValue("Name checksum:", std::to_string(material.NameChecksum));
                                            gui::LabelAndValue("Material flags:", std::to_string(material.MaterialFlags));
                                            gui::LabelAndValue("Num textures:", std::to_string(material.NumTextures));
                                            gui::LabelAndValue("Num constants:", std::to_string(material.NumConstants));
                                            gui::LabelAndValue("Max constants:", std::to_string(material.MaxConstants));
                                            gui::LabelAndValue("Texture offset:", std::to_string(material.TextureOffset));
                                            gui::LabelAndValue("Constant name checksums offset:", std::to_string(material.ConstantNameChecksumsOffset));
                                            gui::LabelAndValue("Constant block offset:", std::to_string(material.ConstantBlockOffset));

                                            if (ImGui::TreeNode("Constants"))
                                            {
                                                for (u32 i = 0; i < material.NumConstants; i++)
                                                {
                                                    MaterialConstant& constant = material.Constants[i];
                                                    const u32 constantNameChecksum = material.ConstantNameChecksums[i];
                                                    std::optional<string> constantNameGuess = HashGuesser::GuessHashOriginString(constantNameChecksum);
                                                    string constantName = constantNameGuess.value_or(std::to_string(constantNameChecksum));
                                                    ImGui::InputFloat4(constantName.c_str(), constant.Constants);
                                                }
                                                ImGui::TreePop();
                                            }

                                            ImGui::TreePop();
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                                ImGui::TreePop();
                            }
                        }
                        ImGui::TreePop();
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
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