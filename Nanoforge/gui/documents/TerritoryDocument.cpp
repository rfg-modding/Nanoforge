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
#include "render/imgui/imgui_ext.h"
#include "rfg/export/Exporters.h"
#include "gui/util/WinUtil.h"
#include "render/resources/RenderChunk.h"
#include "application/project/Project.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "rfg/PackfileVFS.h"
#include <RfgTools++/hashes/HashGuesser.h>
#include <RfgTools++/formats/packfiles/Packfile3.h>
#include <WinUser.h>
#include <imgui_internal.h>
#include <imgui.h>

CVar CVar_DisableHighQualityTerrain("Disable high quality terrain", ConfigType::Bool,
    "If true high lod terrain won't be used in the territory viewer. You must close and re-open any viewers after changing this for it to go into effect.",
    ConfigValue(false),
    true,  //ShowInSettings
    false, //IsFolderPath
    false //IsFilePath
);
CVar CVar_DrawObjectOrientLines("Draw object orientation lines", ConfigType::Bool,
    "Draw lines indicating the orientation of the currently selected object in the map editor. Red = right, green = up, blue = forward",
    ConfigValue(true),
    true,  //ShowInSettings
    false, //IsFolderPath
    false //IsFilePath
);
CVar CVar_DrawChunkMeshes("Draw building meshes", ConfigType::Bool,
    "Draw building meshes in the map editor",
    ConfigValue(true),
    true,  //ShowInSettings
    false, //IsFolderPath
    false //IsFilePath
);
CVar CVar_MapExportPath("Map export path", ConfigType::String,
    "Current map export folder path",
    ConfigValue(""),
    false,  //ShowInSettings
    true, //IsFolderPath
    false //IsFilePath
);

TerritoryDocument::TerritoryDocument(GuiState* state, std::string_view territoryName, std::string_view territoryShortname)
    : TerritoryName(territoryName), TerritoryShortname(territoryShortname)
{
    state_ = state;
    HasMenuBar = true;
    NoWindowPadding = true;
    HasCustomOutlinerAndInspector = true;

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
    updateDebugDraw_ = true;
}

TerritoryDocument::~TerritoryDocument()
{
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

    //Focus the window when right clicking it. Otherwise had to left click the window first after selecting another to control the camera again. Was annoying
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
    {
        ImGui::SetWindowFocus();
    }

    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    //Set current territory to most recently focused territory window
    if (ImGui::IsWindowFocused() && ImGui::IsWindowHovered())
    {
        Scene->Cam.InputActive = true;
    }
    else
    {
        Scene->Cam.InputActive = false;
    }

    //Force visibility update if new terrain instances are loaded
    size_t numLoadedInstance = std::ranges::count_if(Territory.TerrainInstances, [](TerrainInstance& instance) { return instance.Loaded; });
    if (numLoadedTerrainInstances_ != numLoadedInstance)
    {
        numLoadedTerrainInstances_ = (u32)numLoadedInstance;
        Scene->NeedsRedraw = true;
        terrainVisiblityUpdateNeeded_ = true;
    }

    //Update high/low lod mesh visibility based on camera distance
    if ((ImGui::IsWindowFocused() || terrainVisiblityUpdateNeeded_) && useHighLodTerrain_)
    {
        PROFILER_SCOPED("Update mesh visibility");
        std::lock_guard<std::shared_mutex> lock(Territory.TerrainLock);
        f32 highLodTerrainDistance = highLodTerrainEnabled_ ? highLodTerrainDistance_ : -1.0f;
        for (TerrainInstance& terrain : Territory.TerrainInstances)
        {
            if (!terrain.Loaded)
                continue;

            u32 numMeshes = (u32)std::min({ terrain.LowLodMeshes.size(), terrain.HighLodMeshes.size(), terrain.Subzones.size() });
            for (u32 i = 0; i < numMeshes; i++)
            {
                //Calc distance from camera ignoring Y so elevation doesn't matter
                auto& subzone = terrain.Subzones[i];
                Vec2 subzonePos = subzone.Get<Vec3>("Position").XZ();
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
                for (auto& rockMesh : terrain.RockMeshes)
                    if (rockMesh.SubzoneIndex == i)
                        rockMesh.Mesh->Visible = highLodVisible;
            }

        }
        terrainVisiblityUpdateNeeded_ = false;
    }

    //Update chunk visibility
    for (auto& kv : Territory.Chunks)
    {
        RenderChunk* chunk = kv.second;
        Vec2 chunkPos = chunk->Position.XZ();
        Vec2 cameraPos = Scene->Cam.PositionVec3().XZ();
        f32 distanceFromCamera = chunkPos.Distance(cameraPos);
        chunk->Visible = distanceFromCamera <= chunkDistance_ && CVar_DrawChunkMeshes.Get<bool>();
    }

    //Update debug draw regardless of focus state since we'll never be focused when using the other panels which control debug draw
    updateDebugDraw_ = true; //Now set to true permanently to support time based coloring. Quick hack, probably should remove this variable later.
    if (Territory.Zones.size() != 0 && updateDebugDraw_ && Territory.Ready())
    {
        UpdateDebugDraw(state);
        PrimitivesNeedRedraw = false;
    }

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if (contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
    {
        PROFILER_SCOPED("Resize scene");
        Scene->HandleResize((u32)contentAreaSize.x, (u32)contentAreaSize.y);
    }

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

    DrawMenuBar(state);
    DrawPopups(state);
    Keybinds(state);

    //Draw export status bar if one is in progress
    bool exportInProgressOrCompleted = exportTask_ && (exportTask_->Running() || exportTask_->Completed());
    if (exportInProgressOrCompleted)
    {
        ImGui::ProgressBar(exportPercentage_, ImVec2(230.0f, ImGui::GetFontSize() * 1.1f));
        ImGui::SameLine();
        ImGui::Text(exportStatus_.c_str());
    }
}

#pragma warning(disable:4100)
void TerritoryDocument::Save(GuiState* state)
{
    PROFILER_FUNCTION();
    if (!state->CurrentProject)
    {
        LOG_ERROR("Failed to save map! No project open. This shouldn't be possible...");
        return;
    }
    Registry::Get().Save(state->CurrentProject->Path + "\\Registry\\");
}
#pragma warning(default:4100)

//Types not yet supported by the object creator
static std::vector<string> UnsupportedObjectCreatorTypes = 
{
    "rfg_mover", "cover_node", "navpoint", "general_mover", "object_action_node", "object_squad_spawn_node", "object_npc_spawn_node", "object_guard_node",
    "object_path_road", "shape_cutter", "object_vehicle_spawn_node", "ladder", "constraint", "object_bftp_node", 
    "object_turret_spawn_node", "obj_zone", "object_patrol", "object_dummy", "object_raid_node", "object_delivery_node", "marauder_ambush_region",
    "object_activity_spawn", "object_mission_start_node", "object_demolitions_master_node", "object_restricted_area", "object_house_arrest_node",
    "object_area_defense_node", "object_safehouse", "object_convoy_end_point", "object_courier_end_point", "object_riding_shotgun_node", "object_upgrade_node", 
    "object_ambient_behavior_region", "object_roadblock_node", "object_spawn_region", "object_bounding_box", "obj_light", "effect_streaming_node",
    "object_effect"
};

void TerritoryDocument::DrawMenuBar(GuiState* state)
{
    PROFILER_FUNCTION();

    ImGui::SetNextItemWidth(Scene->Width());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 8.0f }); //Must manually set padding here since the parent window has padding disabled to get the viewport flush with the window border.
    bool openScenePopup = false;
    bool openCameraPopup = false;
    bool openExportPopup = false;
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Scene"))
                openScenePopup = true;
            if (ImGui::MenuItem("Camera"))
                openCameraPopup = true;

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Object"))
        {
            bool canClone = selectedObject_.Valid();
            bool canDelete = selectedObject_.Valid();
            bool canRemoveWorldAnchors = selectedObject_.Valid() && selectedObject_.Has("world_anchors");
            bool canRemoveDynamicLinks = selectedObject_.Valid() && selectedObject_.Has("dynamic_links");

            if (ImGui::MenuItem("Clone", "Ctrl + D", nullptr, canClone))
            {
                CloneObject(selectedObject_);
            }
            //Note: Temporarily disabled in order to get first version of map editor out ASAP. First want the bare minimum map editor
            //if (ImGui::BeginMenu("Create"))
            //{
            //    for (ZoneObjectClass& objectClass : Territory.ZoneObjectClasses)
            //    {
            //        if (String::EqualIgnoreCase(objectClass.Name, "Unknown"))
            //            continue;

            //        bool classSupportedByCreator = (std::ranges::find(UnsupportedObjectCreatorTypes, objectClass.Name) == UnsupportedObjectCreatorTypes.end());
            //        if (ImGui::MenuItem(objectClass.Name.c_str(), classSupportedByCreator ? "" : "Not supported yet", nullptr, classSupportedByCreator))
            //        {
            //            showObjectCreationPopup_ = true;
            //            objectCreationType_ = objectClass.Name;
            //        }
            //        if (!classSupportedByCreator)
            //            gui::TooltipOnPrevious("Type not yet supported by object creator");
            //    }

            //    ImGui::EndMenu();
            //}
            if (ImGui::MenuItem("Delete", "Delete", nullptr, canDelete))
            {
                DeleteObject(selectedObject_);
            }
            if (ImGui::MenuItem("Copy scriptx reference", "Ctrl + I", nullptr, canDelete))
            {
                CopyScriptxReference(selectedObject_);
            }
            if (ImGui::MenuItem("Remove world anchors", "Ctrl + B", nullptr, canRemoveWorldAnchors))
            {
                RemoveWorldAnchors(selectedObject_);
            }
            if (ImGui::MenuItem("Remove dynamic links", "Ctrl + N", nullptr, canRemoveDynamicLinks))
            {
                RemoveDynamicLinks(selectedObject_);
            }

            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Export"))
        {
            openExportPopup = true;
        }

        ImGui::EndMenuBar();
    }

    //Have to open the popup in the same scope as BeginPopup(), can't do it in the menu item result. Annoying restriction for imgui popups.
    if (openScenePopup)
        ImGui::OpenPopup("##ScenePopup");
    if (openCameraPopup)
        ImGui::OpenPopup("##CameraPopup");
    if (openExportPopup)
        ImGui::OpenPopup("##MapExportPopup");

    DrawObjectCreationPopup(state);

    //Scene settings popup
    if (ImGui::BeginPopup("##ScenePopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_SUN " Scene settings");
        state->FontManager->FontL.Pop();

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

        ImGui::SliderFloat("Zone object distance", &zoneObjDistance_, 0.0f, 10000.0f);
        ImGui::SameLine();
        gui::HelpMarker("Zone object bounding boxes and meshes aren't drawn beyond this distance from the camera.", ImGui::GetIO().FontDefault);

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

        bool drawChunkMeshes = CVar_DrawChunkMeshes.Get<bool>();
        if (ImGui::Checkbox("Show buildings", &drawChunkMeshes))
            CVar_DrawChunkMeshes.Get<bool>() = drawChunkMeshes;
        if (drawChunkMeshes)
        {
            ImGui::SliderFloat("Building distance", &chunkDistance_, 0.0f, 10000.0f);
            ImGui::SameLine();
            gui::HelpMarker("Beyond this distance from the camera buildings won't be drawn.", ImGui::GetIO().FontDefault);
            terrainVisiblityUpdateNeeded_ = true;
        }

        ImGui::EndPopup();
    }

    //Camera settings popup
    if (ImGui::BeginPopup("##CameraPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CAMERA " Camera");
        state->FontManager->FontL.Pop();

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

        if (ImGui::SliderFloat("Fov", &fov, 40.0f, 120.0f))
            Scene->Cam.SetFovDegrees(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            Scene->Cam.SetLookSensitivity(lookSensitivity);

        if (ImGui::InputFloat3("Position", (float*)&Scene->Cam.camPosition))
        {
            Scene->Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitchDegrees()));
        gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYawDegrees()));
        ImGui::EndPopup();
    }

    //Map export popup
    if (ImGui::BeginPopup("##MapExportPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_MOUNTAIN " Export map");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        if (!Territory.Ready() || !Territory.Object)
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Still loading map...");
        else if (!Territory.Object)
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Map data failed to load.");
        else if (!state->CurrentProject || !state->CurrentProject->Loaded())
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "No project open. Must have one open to export maps.");
        else if (exportTask_ && (exportTask_->Running() || !exportTask_->Completed()))
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Export in progress. Please wait...");
        else if (string mapName = Territory.Object.Get<string>("Name"); mapName == "zonescript_terr01.vpp_pc" || mapName == "zonescript_dlc01.vpp_pc")
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Export only supports MP and WC maps currently.");
        else
        {
            //Select export folder path
            string exportPath = CVar_MapExportPath.Get<string>();
            string initialPath = exportPath;
            ImGui::InputText("Export path", &exportPath);
            ImGui::SameLine();
            if (ImGui::Button("..."))
            {
                std::optional<string> newPath = OpenFolder();
                if (newPath)
                    exportPath = newPath.value();
            }
            CVar_MapExportPath.Get<string>() = exportPath;
            if (initialPath != exportPath)
                Config::Get()->Save();

            //Disable mesh export button if export is disabled
            static string resultString = "";
            const bool canExport = Territory.Ready() && Territory.Object;
            if (!canExport)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            bool exportPressed = ImGui::Button("Export");
            bool exportPatchPressed = ImGui::Button("Export patch");
            ImGui::SameLine();
            gui::HelpMarker("Writes a patch file, for use with RfgMapPatcher. This results in a much smaller download since it only includes edited files. Only use this for distribution purposes. You should use the other export button when developing your map since it'll automatically generate the vpp_pc for you.", ImGui::GetIO().FontDefault);
            if (exportPressed || exportPatchPressed)
            {
                exportTask_ = Task::Create("Export map");
                if (std::filesystem::exists(CVar_MapExportPath.Get<string>()))
                {
                    TaskScheduler::QueueTask(exportTask_, std::bind(&TerritoryDocument::ExportTask, this, state, exportPatchPressed));
                }
                else
                {
                    exportStatus_ = "Export path doesn't exist. Please select a valid folder.";
                }
                ImGui::CloseCurrentPopup();
            }

            if (!canExport)
            {
                ImGui::PopItemFlag();
                ImGui::PopStyleVar();
                gui::TooltipOnPrevious("You must wait for the map to be fully loaded to export it. See the tasks button at the bottom left of the screen.", state->FontManager->FontDefault.GetFont());
            }

            ImGui::SameLine();
            ImGui::TextColored(gui::SecondaryTextColor, resultString);
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void TerritoryDocument::Keybinds(GuiState* state)
{
    PROFILER_FUNCTION();

    ImGuiKeyModFlags keyMods = ImGui::GetMergedKeyModFlags();
    if ((keyMods & ImGuiKeyModFlags_Ctrl) == ImGuiKeyModFlags_Ctrl) //Ctrl down
    {
        //Win32 virtual keycodes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
        if (ImGui::IsKeyPressed(0x44, false) && selectedObject_) //D
        {
            CloneObject(selectedObject_);
        }
        else if (ImGui::IsKeyPressed(0x49, false) && selectedObject_) //I
        {
            CopyScriptxReference(selectedObject_);
        }
        else if (ImGui::IsKeyPressed(0x42, false) && selectedObject_ && selectedObject_.Has("world_anchors")) //B
        {
            RemoveWorldAnchors(selectedObject_);
        }
        else if (ImGui::IsKeyPressed(0x4E, false) && selectedObject_ && selectedObject_.Has("dynamic_links")) //N
        {
            RemoveDynamicLinks(selectedObject_);
        }
    }
    else if (ImGui::IsKeyPressed(VK_DELETE, false) && selectedObject_ && !ImGui::IsAnyItemActive())
    {
        DeleteObject(selectedObject_);
    }
}

void TerritoryDocument::DrawPopups(GuiState* state)
{
    PROFILER_FUNCTION();

    //Object deletion popup
    if (PopupResult result = deleteObjectPopup_.Update(state); result == PopupResult::Yes)
    {
        if (deleteObjectPopupHandle_ == selectedObject_)
            selectedObject_ = NullObjectHandle;

        //TODO: Add actual object deletion via registry
        //Registry::Get().DeleteObject(selected);
        deleteObjectPopupHandle_.Set<bool>("Deleted", true);
        UnsavedChanges = true;
    }
    else if (result == PopupResult::Cancel)
    {
        deleteObjectPopupHandle_ = NullObjectHandle;
    }

    //World anchors deletion popup
    if (PopupResult result = removeWorldAnchorPopup_.Update(state); result == PopupResult::Yes)
    {
        removeWorldAnchorPopupHandle_.Remove("world_anchors");

        //Remove world_anchors from RfgPropertyNames so it doesn't cause an export error when it's not found
        std::vector<string> propertyNames = removeWorldAnchorPopupHandle_.GetStringList("RfgPropertyNames");
        auto find = std::ranges::find(propertyNames, "world_anchors");
        if (find != propertyNames.end())
        {
            propertyNames.erase(find);
            removeWorldAnchorPopupHandle_.SetStringList("RfgPropertyNames", propertyNames);
        }
        UnsavedChanges = true;
    }
    else if (result == PopupResult::Cancel)
    {
        removeWorldAnchorPopupHandle_ = NullObjectHandle;
    }

    //Dynamic links deletion popup
    if (PopupResult result = removeDynamicLinkPopup_.Update(state); result == PopupResult::Yes)
    {
        removeDynamicLinkPopupHandle_.Remove("dynamic_links");

        //Remove dynamic_links from RfgPropertyNames so it doesn't cause an export error when it's not found
        std::vector<string> propertyNames = removeDynamicLinkPopupHandle_.GetStringList("RfgPropertyNames");
        auto find = std::ranges::find(propertyNames, "dynamic_links");
        if (find != propertyNames.end())
        {
            propertyNames.erase(find);
            removeDynamicLinkPopupHandle_.SetStringList("RfgPropertyNames", propertyNames);
        }
        UnsavedChanges = true;
    }
    else if (result == PopupResult::Cancel)
    {
        removeDynamicLinkPopupHandle_ = NullObjectHandle;
    }
}

void TerritoryDocument::UpdateDebugDraw(GuiState* state)
{
    PROFILER_FUNCTION();

    //Reset primitives first to ensure old primitives get cleared
    Scene->ResetPrimitives();

    //Update zone visibility based on distance from camera
    for (ObjectHandle zone : Territory.Zones)
    {
        if (zone.Property("ActivityLayer").Get<bool>() || zone.Property("MissionLayer").Get<bool>())
            continue; //Activity and mission visibility is manually controlled

        Vec2 subzonePos = zone.Property("Position").Get<Vec3>().XZ();
        Vec2 cameraPos = Scene->Cam.PositionVec3().XZ();
        f32 distanceFromCamera = subzonePos.Distance(cameraPos);
        zone.Property("RenderBoundingBoxes").Set<bool>(distanceFromCamera <= zoneObjDistance_);
    }

    //Draw bounding boxes
    for (ObjectHandle zone : Territory.Zones)
    {
        if (!zone.Property("RenderBoundingBoxes").Get<bool>())
            continue;

        for (ObjectHandle object : zone.Property("Objects").GetObjectList())
        {
            auto objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());
            if (!objectClass.Show)
                continue;
            if (object.Has("Deleted") && object.Get<bool>("Deleted"))
                continue;

            Vec3 bmin = object.Property("Bmin").Get<Vec3>();
            Vec3 bmax = object.Property("Bmax").Get<Vec3>();
            Vec3 pos = object.Has("Position") ? object.Property("Position").Get<Vec3>() : Vec3{ 0.0f, 0.0f, 0.0f };

            //If object is selected in zone object list panel use different drawing method for visibilty
            bool selected = (object == selectedObject_);
            if (selected)
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
                lineStart.x = (bmin.x + bmax.x) / 2.0f;
                lineStart.y = bmin.y;
                lineStart.z = (bmin.z + bmax.z) / 2.0f;
                Vec3 lineEnd = lineStart;
                lineEnd.y += 300.0f;

                //Draw object bounding box and line from it's bottom into the sky
                if (objectClass.DrawSolid)
                    Scene->DrawBoxLit(bmin, bmax, color);
                else
                    Scene->DrawBox(bmin, bmax, color);

                //Draw long vertical line to help locate the selected object
                if (ImGui::IsKeyDown(0x52)) //Only show when R key is down
                    Scene->DrawLine(lineStart, lineEnd, color);

                //Draw lines indicating object orientation (red = right, green = up, blue = forward)
                if (CVar_DrawObjectOrientLines.Get<bool>() && object.Has("Orient"))
                {
                    //Determine line length
                    Vec3 size = bmax - bmin;
                    f32 orientLineScale = 2.0f; //How many times larger than the object orient lines should be
                    f32 lineLength = orientLineScale * std::max({ size.x, size.y, size.z }); //Make lines equal length, as long as the widest side of the bbox

                    Mat3 orient = object.Get<Mat3>("Orient");
                    Vec3 center = bmin + (size / 2.0f);
                    Scene->DrawLine(center, center + (orient.rvec * lineLength), { 1.0f, 0.0f, 0.0f }); //Right
                    Scene->DrawLine(center, center + (orient.uvec * lineLength), { 0.0f, 1.0f, 0.0f }); //Up
                    Scene->DrawLine(center, center + (orient.fvec * lineLength), { 0.0f, 0.0f, 1.0f }); //Forward
                }
            }
            else //If not selected just draw bounding box with static color
            {
                if (objectClass.DrawSolid)
                    Scene->DrawBoxLit(bmin, bmax, objectClass.Color);
                else
                    Scene->DrawBox(bmin, bmax, objectClass.Color);
            }
        }
    }

    Scene->NeedsRedraw = true;
    updateDebugDraw_ = false;
}

void TerritoryDocument::DrawObjectCreationPopup(GuiState* state)
{
    if (showObjectCreationPopup_)
        ImGui::OpenPopup("Create object");
    if (ImGui::BeginPopupModal("Create object", nullptr, ImGuiWindowFlags_NoCollapse))
    {
        static bool persistent = false;
        //Todo: Split each creator into separate functions. Starting simple to bootstrap the map editor. This won't scale to the 20+ object types the game has though.

        //player_start fields
        static bool playerStart_Respawn = true;
        static bool playerStart_Indoor = false;
        static bool playerStart_InitialSpawn = false;
        static string playerStart_MpTeam = "EDF";
        //Todo: Implement selectors for these when SP support is added to map editor
        //static bool playerStart_ChekpointRespawn = false;
        //static bool playerStart_ActivityRespawn = false;
        //static string playerStart_MissionInfo;

        //multi_object_marker fields


        //weapon fields


        //trigger_region fields


        //item fields


        bool notImplemented = false;
        state->FontManager->FontMedium.Push();
        ImGui::Text("Creating %s", objectCreationType_.c_str());
        state->FontManager->FontMedium.Pop();

        ImGui::Checkbox("Persistent", &persistent);

        //Content
        if (objectCreationType_ == "player_start")
        {
            static std::vector<string> playerTeamOptions = { "EDF", "Guerilla" /*Mispelled because the game mispelled it*/, "Neutral" };
            if (ImGui::BeginCombo("MP team", playerStart_MpTeam.c_str()))
            {
                for (const string& str : playerTeamOptions)
                {
                    bool selected = (str == playerStart_MpTeam);
                    if (ImGui::Selectable(str.c_str(), &selected))
                    {
                        playerStart_MpTeam = str;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Checkbox("Respawn", &playerStart_Respawn);
            ImGui::Checkbox("Indoor", &playerStart_Indoor);
            ImGui::Checkbox("Initial spawn", &playerStart_InitialSpawn);
        }
        else if (objectCreationType_ == "multi_object_marker")
        {

        }
        else if (objectCreationType_ == "weapon")
        {

        }
        else if (objectCreationType_ == "trigger_region")
        {

        }
        else if (objectCreationType_ == "item")
        {
            //Todo: Consider having separate creator option/shortcut for bagman flags
        }
        else
        {
            notImplemented = true;
            ImGui::Text("Creator not implemented for this type...");
        }

        //Create/cancel buttons
        if (!notImplemented && ImGui::Button("Create"))
        {
            //Don't bother creating a new object if there isn't an unused handle available.
            u32 newHandle = GetNewObjectHandle();
            if (newHandle == 0xFFFFFFFF)
            {
                LOG_ERROR("Failed to create new object. Couldn't find unused object handle. Please report this to a developer. This shouldn't really ever happen.");
                return;
            }

            ObjectHandle newObject = Registry::Get().CreateObject("", objectCreationType_);
            newObject.Set<u32>("Handle", newHandle);
            newObject.Set<u32>("Num", 0);
            newObject.Set<Vec3>("Bmin", { 0.0f, 0.0f, 0.0f });
            newObject.Set<Vec3>("Bmin", { 5.0f, 5.0f, 5.0f }); //TODO: Come up with a better way of generating default bbox

            if (objectCreationType_ == "player_start")
            {
                u16 flags = 65280;
                if (persistent) flags += 1;

                //Create new object and give it a unique handle
                newObject.Set<string>("Name", playerStart_MpTeam + " player start");
                newObject.Set<u32>("ClassnameHash", 1794022917);
                newObject.Set<u16>("Flags", flags);
                newObject.Set<u16>("BlockSize", 0);
                newObject.Set<u16>("NumProps", 0);
                newObject.Set<u16>("PropBlockSize", 0);
                newObject.Set<u32>("ParentHandle", 0xFFFFFFFF);
                newObject.Set<u32>("SiblingHandle", 0xFFFFFFFF);
                newObject.Set<u32>("ChildHandle", 0xFFFFFFFF);
                newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
                newObject.Set<ObjectHandle>("Sibling", NullObjectHandle);
                newObject.SetObjectList("Children", {});
                newObject.Set<bool>("Persistent", persistent);

                //Add object to zone and select it
                ObjectHandle zone = Territory.Object.GetObjectList("Zones")[0];
                zone.GetObjectList("Objects").push_back(newObject);
                newObject.Set<ObjectHandle>("Zone", zone);
                selectedObject_ = newObject;
            }
            else if (objectCreationType_ == "multi_object_marker")
            {

            }
            else if (objectCreationType_ == "weapon")
            {

            }
            else if (objectCreationType_ == "trigger_region")
            {

            }
            else if (objectCreationType_ == "item")
            {
                //Todo: Consider having separate creator option/shortcut for bagman flags
            }

            showObjectCreationPopup_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        if (!notImplemented) ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            //TODO: Delete temporary object if one is used for this
            showObjectCreationPopup_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
}

void TerritoryDocument::CloneObject(ObjectHandle object)
{
    //Don't bother creating a new object if there isn't an unused handle available.
    u32 newHandle = GetNewObjectHandle();
    if (newHandle == 0xFFFFFFFF)
    {
        LOG_ERROR("Failed to clone object {}. Couldn't find unused object handle. Please report this to a developer. This shouldn't really ever happen.", object.UID());
        return;
    }

    ObjectHandle newObject = Registry::Get().CreateObject();
    if (object.Get<string>("Name") != "")
        newObject.Set<string>("Name", object.Get<string>("Name"));
    if (object.Get<string>("Type") != "")
        newObject.Set<string>("Type", object.Get<string>("Type"));

    //Copy properties
    for (PropertyHandle prop : object.Properties())
    {
#define COPY_PROPERTY(type) if (prop.IsType<type>()) newObject.Set<type>(prop.Name(), prop.Get<type>());
        COPY_PROPERTY(std::monostate);
        COPY_PROPERTY(i32);
        COPY_PROPERTY(i8);
        COPY_PROPERTY(u64);
        COPY_PROPERTY(u32);
        COPY_PROPERTY(u16);
        COPY_PROPERTY(u8);
        COPY_PROPERTY(f32);
        COPY_PROPERTY(bool);
        COPY_PROPERTY(string);
        COPY_PROPERTY(std::vector<ObjectHandle>);
        COPY_PROPERTY(std::vector<string>);
        COPY_PROPERTY(Vec3);
        COPY_PROPERTY(Mat3);
        COPY_PROPERTY(ObjectHandle);
        COPY_PROPERTY(BufferHandle);
    }
    newObject.Set<u32>("Handle", newHandle);

    ObjectHandle zone = newObject.Get<ObjectHandle>("Zone");
    zone.GetObjectList("Objects").push_back(newObject);
    selectedObject_ = newObject;
    UnsavedChanges = true;
}

void TerritoryDocument::DeleteObject(ObjectHandle object)
{
    if (!object || deleteObjectPopup_.IsOpen())
        return;

    deleteObjectPopup_.Open();
    deleteObjectPopupHandle_ = object;
}

void TerritoryDocument::CopyScriptxReference(ObjectHandle object)
{
    if (!object.Has("Handle") || !object.IsType<u32>("Handle"))
    {
        Log->error("Failed to copy scriptx reference for object {}. Doesn't have valid property Handle:u32", object.UID());
        return;
    }
    if (!object.Has("Num") || !object.IsType<u32>("Num"))
    {
        Log->error("Failed to copy scriptx reference for object {}. Doesn't have valid property Num:u32", object.UID());
        return;
    }

    //Put object scriptx reference in system clipboard. Used to reference the object in scriptx files (used by vanilla game for some mission/activity scripting)
    ImGui::LogToClipboard();
    ImGui::LogText("<object>%X\n", object.Get<u32>("Handle"));
    ImGui::LogText("    <object_number>%d</object_number>\n", object.Get<u32>("Num"));
    ImGui::LogText("</object>");
    ImGui::LogFinish();
}

void TerritoryDocument::RemoveWorldAnchors(ObjectHandle object)
{
    if (!object || !object.Has("world_anchors") || removeWorldAnchorPopup_.IsOpen())
        return;

    removeWorldAnchorPopup_.Open();
    removeWorldAnchorPopupHandle_ = object;
}

void TerritoryDocument::RemoveDynamicLinks(ObjectHandle object)
{
    if (!object || !object.Has("dynamic_links") || removeDynamicLinkPopup_.IsOpen())
        return;

    removeDynamicLinkPopup_.Open();
    removeDynamicLinkPopupHandle_ = object;
}

u32 TerritoryDocument::GetNewObjectHandle()
{
    //Give new object a unique handle
    u32 largestHandle = 0;
    for (ObjectHandle zone : Territory.Object.GetObjectList("Zones"))
    {
        for (ObjectHandle obj : zone.GetObjectList("Objects"))
        {
            if (obj.Get<u32>("Handle") > largestHandle)
                largestHandle = obj.Get<u32>("Handle");
        }
    }

    //Error if handle is out of valid range
    if (largestHandle >= std::numeric_limits<u32>::max() - 1)
    {
        LOG_ERROR("Failed to find unused object handle. Returning 0xFFFFFFFF.");
        return 0xFFFFFFFF;
    }

    //TODO: See if we can start at 0 instead. All vanilla game handles are very large but they might've be generated them with a hash or something
    //TODO: See if we can discard handle + num completely and regenerate them on export. One problem is that changes to one zone might require regenerating all zones on the SP map (assuming there's cross zone object relations)
    return largestHandle + 1;
}

void TerritoryDocument::Outliner(GuiState* state)
{
    PROFILER_FUNCTION();

    Outliner_DrawFilters(state);

    //Set custom highlight colors for the table
    ImVec4 selectedColor = { 0.157f, 0.350f, 0.588f, 1.0f };
    ImVec4 highlightColor = { selectedColor.x * 1.1f, selectedColor.y * 1.1f, selectedColor.z * 1.1f, 1.0f };
    ImGui::PushStyleColor(ImGuiCol_Header, selectedColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, highlightColor);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, highlightColor);

    //Draw object tree
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("ZoneObjectTable", 4, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Flags");
        ImGui::TableSetupColumn("Num");
        ImGui::TableSetupColumn("Handle");
        ImGui::TableHeadersRow();

        //Loop through visible zones
        for (ObjectHandle zone : Territory.Zones)
        {
            if (outliner_OnlyShowNearZones_ && !zone.Property("RenderBoundingBoxes").Get<bool>())
                continue;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            //Don't draw zone if none of the objects in it are visible
            bool anyChildrenVisible = Outliner_ZoneAnyChildObjectsVisible(zone);
            if (!anyChildrenVisible)
                continue;

            bool selected = (zone == selectedObject_);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | (anyChildrenVisible ? 0 : ImGuiTreeNodeFlags_Leaf);
            if (selected)
                flags |= ImGuiTreeNodeFlags_Selected;

            //Draw tree node for zones if there's > 1. Otherwise draw the objects at the root of the tree
            bool singleZone = Territory.Zones.size() == 1;
            bool treeNodeOpen = false;
            if (!singleZone)
                treeNodeOpen = ImGui::TreeNodeEx(zone.Property("Name").Get<string>().c_str(), flags);

            if (singleZone || treeNodeOpen)
            {
                for (ObjectHandle object : zone.Property("Objects").GetObjectList())
                {
                    //Don't draw objects with parents at the top of the tree. They'll be drawn as subnodes when their parent calls DrawObjectNode()
                    if (!object || (object.Has("Parent") && object.Property("Parent").Get<ObjectHandle>()))
                        continue;
                    if (object.Has("Deleted") && object.Get<bool>("Deleted"))
                        continue;

                    Outliner_DrawObjectNode(state, object);
                }
                ImGui::TreePop();
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::EndTable();
    }
}

void TerritoryDocument::Inspector(GuiState* state)
{
    PROFILER_FUNCTION();
    if (!selectedObject_)
    {
        ImGui::Text("%s Select an object in the outliner to see its properties", ICON_FA_EXCLAMATION_CIRCLE);
        return;
    }

    string name = selectedObject_.Property("Name").Get<string>();
    if (name == "")
        name = selectedObject_.Property("Type").Get<string>();

    //Name
    state->FontManager->FontMedium.Push();
    ImGui::Text(name);
    state->FontManager->FontMedium.Pop();

    //Type
    ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
    ImGui::Text(selectedObject_.Property("Type").Get<string>());
    ImGui::PopStyleColor();

    //Handle, num
    u32 handle = selectedObject_.Property("Handle").Get<u32>();
    u32 num = selectedObject_.Property("Num").Get<u32>();
    ImGui::SameLine();
    ImGui::Text(fmt::format(", {}, {}", handle, num).c_str());
    ImGui::Separator();

    //Relative objects
    if (ImGui::CollapsingHeader("Relations"))
    {
        ImGui::Indent(15.0f);
        if (Inspector_DrawObjectHandleEditor(selectedObject_.Property("Zone"))) UnsavedChanges = true;
        if (Inspector_DrawObjectHandleEditor(selectedObject_.Property("Parent"))) UnsavedChanges = true;
        if (Inspector_DrawObjectHandleEditor(selectedObject_.Property("Sibling"))) UnsavedChanges = true;
        if (Inspector_DrawObjectHandleListEditor(selectedObject_.Property("Children"))) UnsavedChanges = true;
        ImGui::Unindent(15.0f);
    }
    if (ImGui::CollapsingHeader("Bounding box"))
    {
        ImGui::Indent(15.0f);
        if (Inspector_DrawVec3Editor(selectedObject_.Property("Bmin"))) UnsavedChanges = true;
        if (Inspector_DrawVec3Editor(selectedObject_.Property("Bmax"))) UnsavedChanges = true;
        if (selectedObject_.Has("BBmin"))
            if (Inspector_DrawVec3Editor(selectedObject_.Property("BBmin")))
                UnsavedChanges = true;
        if (selectedObject_.Has("BBmax"))
            if (Inspector_DrawVec3Editor(selectedObject_.Property("BBmax")))
                UnsavedChanges = true;

        ImGui::Unindent(15.0f);
    }

    //Draw name, persistent, position, and orient first so they're easy to find
    if (Inspector_DrawBoolEditor(selectedObject_.Property("Persistent")))
    {
        u16 flags = selectedObject_.Get<u16>("Flags");
        if (selectedObject_.Get<bool>("Persistent"))
            flags |= 1; //Enable persistent flag bit
        else
            flags &= ~1; //Disable persistent flag bit

        UnsavedChanges = true;
    }
    if (Inspector_DrawStringEditor(selectedObject_.Property("Name"))) UnsavedChanges = true;

    Vec3 initialPos = selectedObject_.Get<Vec3>("Position");
    if (Inspector_DrawVec3Editor(selectedObject_.Property("Position")))
    {
        //Auto update bounding box positions when position is changed
        Vec3 delta = selectedObject_.Get<Vec3>("Position") - initialPos;
        if (selectedObject_.Has("Bmin"))
            selectedObject_.Set<Vec3>("Bmin", selectedObject_.Get<Vec3>("Bmin") + delta);
        if (selectedObject_.Has("Bmax"))
            selectedObject_.Set<Vec3>("Bmax", selectedObject_.Get<Vec3>("Bmax") + delta);
        if (selectedObject_.Has("BBmin"))
            selectedObject_.Set<Vec3>("BBmin", selectedObject_.Get<Vec3>("BBmin") + delta);
        if (selectedObject_.Has("BBmax"))
            selectedObject_.Set<Vec3>("BBmax", selectedObject_.Get<Vec3>("BBmax") + delta);

        //If object has a chunk mesh update its position
        if (Territory.Chunks.contains(selectedObject_.UID()))
        {
            RenderChunk* chunk = Territory.Chunks[selectedObject_.UID()];
            chunk->Position += delta;
        }

        UnsavedChanges = true;
    }
    if (selectedObject_.Has("Orient") && Inspector_DrawMat3Editor(selectedObject_.Property("Orient")))
    {
        //If object has a chunk mesh update its orient
        if (Territory.Chunks.contains(selectedObject_.UID()))
        {
            RenderChunk* chunk = Territory.Chunks[selectedObject_.UID()];
            chunk->Rotation = selectedObject_.Get<Mat3>("Orient");
        }
        UnsavedChanges = true;
    }

    //Object properties
    static std::vector<string> HiddenProperties = //These ones aren't drawn or have special logic
    {
        "Handle", "Num", "ClassnameHash", "RfgPropertyNames", "BlockSize", "ParentHandle", "SiblingHandle", "ChildHandle",
        "NumProps", "PropBlockSize", "Type", "Parent", "Child", "Sibling", "Children", "Zone", "Bmin", "Bmax", "BBmin", "BBmax",
        "Position", "Orient", "Name", "Persistent"
    };
    for (PropertyHandle prop : selectedObject_.Properties())
    {
        if (std::ranges::find(HiddenProperties, prop.Name()) != HiddenProperties.end())
            continue; //Skip hidden properties

        bool propertyEdited = false;
        const f32 indent = 15.0f;
        ImGui::PushID(fmt::format("{}_{}", selectedObject_.UID(), prop.Index()).c_str());
        if (prop.IsType<i32>())
        {
            i32 data = prop.Get<i32>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_S32, &data))
            {
                prop.Set<i32>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<u64>())
        {
            u64 data = prop.Get<u64>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U64, &data))
            {
                prop.Set<u64>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<u32>())
        {
            u32 data = prop.Get<u32>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U32, &data))
            {
                prop.Set<u32>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<u16>())
        {
            u16 data = prop.Get<u16>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U16, &data))
            {
                prop.Set<u16>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<u8>())
        {
            u8 data = prop.Get<u8>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U8, &data))
            {
                prop.Set<u8>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<f32>())
        {
            f32 data = prop.Get<f32>();
            if (ImGui::InputFloat(prop.Name().c_str(), &data))
            {
                prop.Set<f32>(data);
                propertyEdited = true;
            }
        }
        else if (prop.IsType<bool>())
        {
            if (Inspector_DrawBoolEditor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<string>())
        {
            if (Inspector_DrawStringEditor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<std::vector<ObjectHandle>>())
        {
            if (Inspector_DrawObjectHandleListEditor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<std::vector<string>>())
        {
            ImGui::Text(prop.Name() + ":");
            ImGui::Indent(indent);

            std::vector<string> strings = prop.Get<std::vector<string>>();
            if (strings.size() > 0)
            {
                for (string str : strings)
                {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    ImGui::Text(str);
                }
            }
            else
            {
                ImGui::Text("Empty");
            }
            ImGui::Unindent(indent);
        }
        else if (prop.IsType<Vec3>())
        {
            if (Inspector_DrawVec3Editor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<Mat3>())
        {
            if (Inspector_DrawMat3Editor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<ObjectHandle>())
        {
            if (Inspector_DrawObjectHandleEditor(prop))
                propertyEdited = true;
        }
        else if (prop.IsType<BufferHandle>())
        {
            BufferHandle handle = prop.Get<BufferHandle>();
            string name = "";
            if (handle.Name() != "")
                name += handle.Name() + ", ";

            string uid;
            if (handle.UID() == NullUID)
                uid = "Null";
            else
                uid = std::to_string(handle.UID());

            gui::LabelAndValue(prop.Name() + ":", fmt::format("Buffer({}{}, {} bytes)", name, uid, handle.Size()));
        }
        else
        {
            ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
        }
        ImGui::PopID();

        if (propertyEdited)
        {
            updateDebugDraw_ = true; //Update in case value related to debug draw is changed
            UnsavedChanges = true; //Mark file as edited. Puts * on tab + lets you save changes with Ctrl + S
        }
    }
}

bool TerritoryDocument::Inspector_DrawObjectHandleEditor(PropertyHandle prop)
{
    ObjectHandle handle = prop.Get<ObjectHandle>();
    string name = "";
    if (handle.Has("Name"))
    {
        name = handle.Property("Name").Get<string>();
        if (name != "")
            name += ", ";
    }

    string uid;
    if (handle.UID() == NullUID)
        uid = "Null";
    else
        uid = std::to_string(handle.UID());

    gui::LabelAndValue(prop.Name() + ":", fmt::format("Object({}{})", name, uid));
    return false; //Editing not yet supported by this type so it always returns false
}

bool TerritoryDocument::Inspector_DrawObjectHandleListEditor(PropertyHandle prop)
{
    const f32 indent = 15.0f;
    ImGui::Text(prop.Name() + ":");
    ImGui::Indent(indent);

    std::vector<ObjectHandle> handles = prop.Get<std::vector<ObjectHandle>>();
    if (handles.size() > 0)
    {
        for (ObjectHandle handle : handles)
        {
            string name = "";
            if (handle.Has("Name"))
            {
                name = handle.Property("Name").Get<string>();
                if (name != "")
                    name += ", ";
            }

            string uid;
            if (handle.UID() == NullUID)
                uid = "Null";
            else
                uid = std::to_string(handle.UID());

            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::Text(fmt::format("Object({}{})", name, uid));
        }
    }
    else
    {
        ImGui::Text("Empty");
    }
    ImGui::Unindent(indent);
    return false; //Editing not yet supported by this type so it always returns false
}

bool TerritoryDocument::Inspector_DrawVec3Editor(PropertyHandle prop)
{
    bool edited = false;
    Vec3 data = prop.Get<Vec3>();
    Vec3 initialValue = data;
    if (ImGui::InputFloat3(prop.Name().c_str(), (f32*)&data))
    {
        prop.Set<Vec3>(data);
        edited = true; //Update in case value related to debug draw is changed
    }

    return edited;
}

bool TerritoryDocument::Inspector_DrawMat3Editor(PropertyHandle prop)
{
    bool edited = false;
    ImGui::Text(prop.Name() + ":");
    ImGui::Indent(15.0f);

    Mat3 data = prop.Get<Mat3>();
    if (ImGui::InputFloat3("Right", (f32*)&data.rvec))
    {
        prop.Set<Mat3>(data);
        edited = true;
    }
    if (ImGui::InputFloat3("Up", (f32*)&data.uvec))
    {
        prop.Set<Mat3>(data);
        edited = true;
    }
    if (ImGui::InputFloat3("Forward", (f32*)&data.fvec))
    {
        prop.Set<Mat3>(data);
        edited = true;
    }
    ImGui::Unindent(15.0f);

    return edited;
}

bool TerritoryDocument::Inspector_DrawStringEditor(PropertyHandle prop)
{
    bool edited = false;
    string data = prop.Get<string>();
    if (ImGui::InputText(prop.Name().c_str(), &data))
    {
        prop.Set<string>(data);
        edited = true;
    }
    return edited;
}

bool TerritoryDocument::Inspector_DrawBoolEditor(PropertyHandle prop)
{
    bool edited = false;
    bool data = prop.Get<bool>();
    if (ImGui::Checkbox(prop.Name().c_str(), &data))
    {
        prop.Set<bool>(data);
        edited = true;
    }
    
    return edited;
}

void TerritoryDocument::ExportTask(GuiState* state, bool exportPatch)
{
    bool exportResult = exportPatch ? ExportPatch() : ExportMap(state);
    if (exportResult)
    {
        //Get current time
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        std::time_t startTime = std::chrono::system_clock::to_time_t(now);
        char timeStrBuffer[128];
        tm localTime;
        errno_t err = localtime_s(&localTime, &startTime);

        //Update result string
        if (std::strftime(timeStrBuffer, sizeof(timeStrBuffer), "%H:%M:%S", &localTime) > 0)
            exportStatus_ = "Export finished at " + string(timeStrBuffer);
        else
            exportStatus_ = "Export finished.";
    }
    else
    {
        exportStatus_ = "Failed to export map. Check log.";
    }
}

void UpdateAsmPc(const string& asmPath);
bool TerritoryDocument::ExportMap(GuiState* state)
{
    string terrPackfileName = Territory.Object.Get<string>("Name");
    string mapName = Path::GetFileNameNoExtension(Territory.Object.Get<string>("Name"));

    //Steps: Export zones, extract vpp_pc, extract containers, copy zones to containers, repack containers, update asm_pc, repack vpp_pc, cleanup
    const f32 numSteps = 8.0f;
    const f32 percentagePerStep = 1.0f / numSteps;

    exportPercentage_ = 0.0f;
    exportStatus_ = "Exporting zones...";

    //Make sure folder exists for writing temporary files
    string tempFolderPath = state->CurrentProject->Path + "\\Temp\\";
    std::filesystem::create_directories(tempFolderPath);

    //Export zone files
    string exportFolderPath = CVar_MapExportPath.Get<string>();
    if (!Exporters::ExportTerritory(Territory.Object, tempFolderPath))
    {
        LOG_ERROR("Failed to export territory '{}'", mapName);
        return false;
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Extracting " + terrPackfileName + "...";

    //Zone file export suceeded. Now repack the str2_pc/vpp_pc files containing the zones + update the asm_pc files
    //Extract vpp_pc
    Handle<Packfile3> vppHandle = state->PackfileVFS->GetPackfile(terrPackfileName);
    if (!vppHandle)
    {
        LOG_ERROR("Failed to get map vpp_pc '{}' when repacking zone data.", terrPackfileName);
        return false;
    }
    //Reparse packfile from games data folder. Simplest way to do this for the moment until PackfileVFS is updated to be able to handle reloading packfiles safely
    //Can't use data in PackfileVFS since the packfile contents would've changed if the user does multiple map exports in a row
    string packfilePath = vppHandle->Path;
    Handle<Packfile3> currVpp = CreateHandle<Packfile3>(packfilePath);
    currVpp->ReadMetadata();

    string packfileOutPath = tempFolderPath + "vpp\\";
    std::filesystem::create_directories(packfileOutPath);
    currVpp->ExtractSubfiles(packfileOutPath, false);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Extracting containers...";

    //Extract any str2_pc files in the vpp_pc
    for (auto entry : std::filesystem::directory_iterator(packfileOutPath))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".str2_pc")
            continue;

        string str2OutPath = tempFolderPath + "vpp\\containers\\" + Path::GetFileNameNoExtension(entry.path().filename()) + "\\";
        Packfile3 str2(entry.path().string());
        str2.ExtractSubfiles(str2OutPath, true);
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Copying zones to containers...";

    //Copy zones into vpp_pc and ns_base.str2_pc
    string zoneFilename = mapName + ".rfgzone_pc";
    string pZoneFilename = "p_" + zoneFilename;
    string zonePath = tempFolderPath + "\\" + zoneFilename;
    string pZonePath = tempFolderPath + "\\" + pZoneFilename;
    std::filesystem::copy_file(zonePath, packfileOutPath + "\\" + zoneFilename, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(pZonePath, packfileOutPath + "\\" + pZoneFilename, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(zonePath, packfileOutPath + "\\containers\\ns_base\\" + zoneFilename, std::filesystem::copy_options::overwrite_existing);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Repacking containers...";

    //Repack str2_pc files
    string nsBaseOutPath = fmt::format("{}\\vpp\\ns_base.str2_pc", tempFolderPath);
    string minimapOutPath = fmt::format("{}\\vpp\\{}_map.str2_pc", tempFolderPath, mapName);
    std::filesystem::remove(nsBaseOutPath);
    std::filesystem::remove(minimapOutPath);
    Packfile3::Pack(fmt::format("{}\\vpp\\containers\\ns_base\\", tempFolderPath), nsBaseOutPath, true, true);
    Packfile3::Pack(fmt::format("{}\\vpp\\containers\\{}_map\\", tempFolderPath, mapName), minimapOutPath, true, true);
    std::filesystem::remove_all(tempFolderPath + "\\vpp\\containers\\"); //Delete containers subfolder. Don't need files after they've been repacked

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Updating asm_pc...";

    //Update asm_pc file
    UpdateAsmPc(tempFolderPath + "\\vpp\\" + mapName + ".asm_pc");

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Repacking " + terrPackfileName + "...";

    //Repack main vpp_pc
    Packfile3::Pack(tempFolderPath + "\\vpp\\", tempFolderPath + "\\" + terrPackfileName, false, false);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Copying vpp_pc to export folder...";

    //Copy vpp_pc to output path + delete temporary files
    std::filesystem::copy_file(tempFolderPath + "\\" + terrPackfileName, CVar_MapExportPath.Get<string>() + "\\" + terrPackfileName, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove_all(tempFolderPath);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Done!";
}

constexpr u32 RFG_PATCHFILE_SIGNATURE = 1330528590; //Equals ASCII string "NANO"
constexpr u32 RFG_PATCHFILE_VERSION = 1;
bool TerritoryDocument::ExportPatch()
{
    exportPercentage_ = 0.0f;

    //Export zone files
    string terrPackfileName = Territory.Object.Get<string>("Name"); //Name of the map vpp_pc
    string mapName = Path::GetFileNameNoExtension(terrPackfileName); //Name of the map
    string exportFolderPath = CVar_MapExportPath.Get<string>(); //Folder to write final patch file to
    if (!Exporters::ExportTerritory(Territory.Object, exportFolderPath))
    {
        LOG_ERROR("Map patch export failed! Map export failed for '{}'", mapName);
        return false;
    }

    string zoneFilePath = fmt::format("{}\\{}.rfgzone_pc", exportFolderPath, mapName);
    string pZoneFilePath = fmt::format("{}\\p_{}.rfgzone_pc", exportFolderPath, mapName);
    std::vector<u8> zoneBytes = File::ReadAllBytes(zoneFilePath);
    std::vector<u8> pZoneBytes = File::ReadAllBytes(pZoneFilePath);
    if (zoneBytes.size() == 0 || pZoneBytes.size() == 0)
    {
        LOG_ERROR("Map patch export failed! More or both exported zone files are empty. Sizes: {}, {}", zoneBytes.size(), pZoneBytes.size());
        return false;
    }

    //Merge zone files into single patch file. Meant to be used with the separate RfgMapPatcher tool. Makes the final exports much smaller.
    //Write patch file header
    BinaryWriter patch(fmt::format("{}\\{}.RfgPatch", exportFolderPath, mapName));
    patch.Write<u32>(RFG_PATCHFILE_SIGNATURE);
    patch.Write<u32>(RFG_PATCHFILE_VERSION);
    patch.WriteNullTerminatedString(terrPackfileName);
    patch.Align(4);
    
    //Write zone file sizes
    patch.Write<size_t>(zoneBytes.size());
    patch.Write<size_t>(pZoneBytes.size());

    //Write zone file data
    patch.WriteSpan<u8>(zoneBytes);
    patch.Align(4);
    patch.WriteSpan<u8>(pZoneBytes);
    patch.Align(4);

    //Remove zone files. We only care about the final patch file with this option
    std::filesystem::remove(zoneFilePath);
    std::filesystem::remove(pZoneFilePath);

    exportPercentage_ = 1.0f;
    return true;
}

void UpdateAsmPc(const string& asmPath)
{
    AsmFile5 asmFile;
    BinaryReader reader(asmPath);
    asmFile.Read(reader, Path::GetFileName(asmPath));

    //Update containers
    for (auto& file : std::filesystem::directory_iterator(Path::GetParentDirectory(asmPath)))
    {
        if (!file.is_regular_file() || file.path().extension() != ".str2_pc")
            continue;

        //Parse container file to get up to date values
        string packfilePath = file.path().string();
        if (!std::filesystem::exists(packfilePath))
            continue;

        Packfile3 packfile(packfilePath);
        packfile.ReadMetadata();

        for (AsmContainer& container : asmFile.Containers)
        {
            struct AsmPrimitiveSizeIndices { size_t HeaderIndex = -1; size_t DataIndex = -1; };
            std::vector<AsmPrimitiveSizeIndices> sizeIndices = {};
            size_t curPrimSizeIndex = 0;
            //Pre-calculate indices of header/data size for each primitive in Container::PrimitiveSizes
            for (AsmPrimitive& prim : container.Primitives)
            {
                AsmPrimitiveSizeIndices& indices = sizeIndices.emplace_back();
                indices.HeaderIndex = curPrimSizeIndex++;
                if (prim.DataSize == 0)
                    indices.DataIndex = -1; //This primitive has no gpu file and so it only has one value in PrimitiveSizes
                else
                    indices.DataIndex = curPrimSizeIndex++;
            }
            const bool virtualContainer = container.CompressedSize == 0 && container.DataOffset == 0;

            if (!virtualContainer && String::EqualIgnoreCase(Path::GetFileNameNoExtension(packfile.Name()), container.Name))
            {
                container.CompressedSize = packfile.Header.CompressedDataSize;

                size_t dataStart = 0;
                dataStart += 2048; //Header size
                dataStart += packfile.Entries.size() * 28; //Each entry is 28 bytes
                dataStart += BinaryWriter::CalcAlign(dataStart, 2048); //Align(2048) after end of entries
                dataStart += packfile.Header.NameBlockSize; //Filenames list
                dataStart += BinaryWriter::CalcAlign(dataStart, 2048); //Align(2048) after end of file names

                printf("Setting container offset '%s', packfile name: '%s'\n", container.Name.c_str(), packfile.Name().c_str());
                container.DataOffset = dataStart;
            }

            for (size_t entryIndex = 0; entryIndex < packfile.Entries.size(); entryIndex++)
            {
                Packfile3Entry& entry = packfile.Entries[entryIndex];
                for (size_t primitiveIndex = 0; primitiveIndex < container.Primitives.size(); primitiveIndex++)
                {
                    AsmPrimitive& primitive = container.Primitives[primitiveIndex];
                    AsmPrimitiveSizeIndices& indices = sizeIndices[primitiveIndex];
                    if (String::EqualIgnoreCase(primitive.Name, packfile.EntryNames[entryIndex]))
                    {
                        //If DataSize = 0 assume this primitive has no gpu file
                        if (primitive.DataSize == 0)
                        {
                            primitive.HeaderSize = entry.DataSize; //Uncompressed size
                            if (!virtualContainer)
                            {
                                container.PrimitiveSizes[indices.HeaderIndex] = primitive.HeaderSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                            }
                        }
                        else //Otherwise assume primitive has cpu and gpu file
                        {
                            primitive.HeaderSize = entry.DataSize; //Cpu file uncompressed size
                            primitive.DataSize = packfile.Entries[entryIndex + 1].DataSize; //Gpu file uncompressed size
                            if (!virtualContainer)
                            {
                                container.PrimitiveSizes[indices.HeaderIndex] = primitive.HeaderSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                                container.PrimitiveSizes[indices.DataIndex] = primitive.DataSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                            }
                        }
                    }
                }
            }
        }
    }

    asmFile.Write(asmPath);
}

void TerritoryDocument::Outliner_DrawFilters(GuiState* state)
{
    //Filters out objects shown in the outliner
    if (ImGui::Button(ICON_FA_FILTER))
        ImGui::OpenPopup("##ObjectFiltersPopup");
    if (ImGui::BeginPopup("##ObjectFiltersPopup"))
    {
        if (ImGui::Button("Show all types"))
        {
            for (auto& objectClass : Territory.ZoneObjectClasses)
            {
                objectClass.Show = true;
            }

            updateDebugDraw_ = true;
            Territory.UpdateObjectClassInstanceCounts();
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide all types"))
        {
            for (auto& objectClass : Territory.ZoneObjectClasses)
            {
                objectClass.Show = false;
            }
            updateDebugDraw_ = true;
            Territory.UpdateObjectClassInstanceCounts();
        }

        if (ImGui::Checkbox("Hide distant objects", &outliner_OnlyShowNearZones_))
        {
            Territory.UpdateObjectClassInstanceCounts();
        }
        ImGui::SameLine();
        gui::HelpMarker("If checked then objects outside of the viewing range are hidden. The viewing range is configurable through View > Scene in the map editor menu bar.", ImGui::GetIO().FontDefault);
        ImGui::SameLine();
        ImGui::Checkbox("Only show persistent", &outliner_OnlyShowPersistentObjects_);

        //Set custom highlight colors for the table
        ImVec4 selectedColor = { 0.157f, 0.350f, 0.588f, 1.0f };
        ImVec4 highlightColor = { selectedColor.x * 1.1f, selectedColor.y * 1.1f, selectedColor.z * 1.1f, 1.0f };
        ImGui::PushStyleColor(ImGuiCol_Header, selectedColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, highlightColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, highlightColor);

        //Draw object filters table. One row per object type
        ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
            ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("ObjectFilterTable", 5, flags, { 450.0f, 500.0f }))
        {
            ImGui::TableSetupScrollFreeze(0, 1); //Freeze header row so it's always visible
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Visible");
            ImGui::TableSetupColumn("Solid");
            ImGui::TableSetupColumn("Count");
            ImGui::TableSetupColumn("Color");
            ImGui::TableHeadersRow();

            for (auto& objectClass : Territory.ZoneObjectClasses)
            {
                ImGui::TableNextRow();

                //Type
                ImGui::TableNextColumn();
                ImGui::Text(objectClass.Name);
                
                //Visible
                ImGui::TableNextColumn();
                if (ImGui::Checkbox((string("##Visible") + objectClass.Name).c_str(), &objectClass.Show))
                    updateDebugDraw_ = true;

                //Solid
                ImGui::TableNextColumn();
                if (ImGui::Checkbox((string("##Solid") + objectClass.Name).c_str(), &objectClass.DrawSolid))
                    updateDebugDraw_ = true;

                //# of objects of this type
                ImGui::TableNextColumn();
                ImGui::Text("%d objects", objectClass.NumInstances);

                //Object color
                ImGui::TableNextColumn();
                if (ImGui::ColorEdit3(string("##" + objectClass.Name).c_str(), (f32*)&objectClass.Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                    updateDebugDraw_ = true;
            }

            ImGui::PopStyleColor(3);
            ImGui::EndTable();
        }

        ImGui::EndPopup();
    }
}

void TerritoryDocument::Outliner_DrawObjectNode(GuiState* state, ObjectHandle object)
{
    if (object.Has("Deleted") && object.Get<bool>("Deleted"))
        return;

    //Don't show node if it and it's children object types are being hidden
    bool showObjectOrChildren = Outliner_ShowObjectOrChildren(object);
    bool visible = Outliner_ShowObjectOrChildren(object);
    if (!visible)
        return;

    auto& objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());

    //Update node index and selection state
    bool selected = (object == selectedObject_);

    //Attempt to find a human friendly name for the object
    string name = object.Property("Name").Get<string>();

    //Determine best object name
    string objectLabel = "      "; //Empty space for node icon
    if (name != "")
        objectLabel += name; //Use custom name if available
    else
        objectLabel += object.Property("Type").Get<string>(); //Otherwise use object type name (e.g. rfg_mover, navpoint, obj_light, etc)

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    //Draw node
    ImGui::PushID(object.UID()); //Push unique ID for the UI element
    f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    bool nodeOpen = ImGui::TreeNodeEx(objectLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth |
        (object.GetObjectList("Children").size() == 0 ? ImGuiTreeNodeFlags_Leaf : 0) | (selected ? ImGuiTreeNodeFlags_Selected : 0));

    if (ImGui::IsItemClicked())
    {
        if (object == selectedObject_)
            selectedObject_ = NullObjectHandle; //Clicked selected object again, deselect it
        else
            selectedObject_ = object; //Clicked object, select it
    }
    if (ImGui::IsItemHovered())
        gui::TooltipOnPrevious(object.Property("Type").Get<string>(), ImGui::GetIO().FontDefault);

    //Draw node icon
    ImGui::PushStyleColor(ImGuiCol_Text, { objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, 1.0f });
    ImGui::SameLine();
    ImGui::SetCursorPosX(nodeXPos + 22.0f);
    ImGui::Text(objectClass.LabelIcon);
    ImGui::PopStyleColor();

    //Flags
    ImGui::TableNextColumn();
    ImGui::Text(std::to_string(object.Property("Flags").Get<u16>()));

    //Num
    ImGui::TableNextColumn();
    ImGui::Text(std::to_string(object.Property("Num").Get<u32>()));

    //Handle
    ImGui::TableNextColumn();
    ImGui::Text(std::to_string(object.Property("Handle").Get<u32>()));

    //Draw child nodes
    Registry& registry = Registry::Get();
    if (nodeOpen)
    {
        for (ObjectHandle child : object.GetObjectList("Children"))
            if (child || (object.Has("Deleted") && object.Get<bool>("Deleted")))
                Outliner_DrawObjectNode(state, child);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

bool TerritoryDocument::Outliner_ZoneAnyChildObjectsVisible(ObjectHandle zone)
{
    for (ObjectHandle object : zone.Property("Objects").GetObjectList())
        if (Outliner_ShowObjectOrChildren(object))
            return true;

    return false;
}

bool TerritoryDocument::Outliner_ShowObjectOrChildren(ObjectHandle object)
{
    auto& objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());
    if (objectClass.Show)
    {
        if (outliner_OnlyShowPersistentObjects_)
            return object.Get<bool>("Persistent");
        else
            return true;
    }

    Registry& registry = Registry::Get();
    for (ObjectHandle child : object.GetObjectList("Children"))
        if (child && Outliner_ShowObjectOrChildren(child))
            return true;

    return false;
}