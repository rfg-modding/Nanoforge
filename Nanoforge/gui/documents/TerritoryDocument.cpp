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
#include <RfgTools++/hashes/HashGuesser.h>
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

    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    ImVec2 windowMin = ImGui::GetWindowPos();
    ImVec2 windowMax = windowMin;
    windowMax.x += ImGui::GetWindowSize().x;
    windowMax.y += ImGui::GetWindowSize().y;

    //Set current territory to most recently focused territory window
    if (ImGui::IsWindowFocused() && ImGui::IsMouseHoveringRect(windowMin, windowMax))
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
            }

        }
        Territory.UpdateObjectClassInstanceCounts();
        terrainVisiblityUpdateNeeded_ = false;
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

    DrawMenuBar(state);
    Keybinds(state);
}

#pragma warning(disable:4100)
void TerritoryDocument::Save(GuiState* state)
{

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
    "object_ambient_behavior_region", "object_roadblock_node", "object_spawn_region", "object_bounding_box", "obj_light"
};

void TerritoryDocument::DrawMenuBar(GuiState* state)
{
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
            if (ImGui::BeginMenu("Create"))
            {
                for (ZoneObjectClass& objectClass : Territory.ZoneObjectClasses)
                {
                    if (objectClass.Name == "Unknown")
                        continue;

                    bool classSupportedByCreator = (std::ranges::find(UnsupportedObjectCreatorTypes, objectClass.Name) == UnsupportedObjectCreatorTypes.end());
                    if (ImGui::MenuItem(objectClass.Name.c_str(), classSupportedByCreator ? "" : "Not yet supported", nullptr, classSupportedByCreator))
                    {
                        showObjectCreationPopup_ = true;
                        objectCreationType_ = objectClass.Name;
                    }
                    if (!classSupportedByCreator)
                        gui::TooltipOnPrevious("Type not yet supported by object creator");
                }

                ImGui::EndMenu();
            }
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

        ImGui::SliderFloat("Zone object distance", &zoneObjDistance_, 0.0f, 10000.0f);
        ImGui::SameLine();
        gui::HelpMarker("Zone object bounding boxes and meshes aren't drawn beyond this distance from the camera.", ImGui::GetIO().FontDefault);

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
        ImGui::Text("Export map");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        static string ExportPath = "H:/_RFGR_MapExportTest/";
        //ImGui::InputText("Export path", &ExportPath);
        //ImGui::SameLine();
        //if (ImGui::Button("..."))
        //{
        //    auto result = OpenFolder();
        //    if (!result)
        //        return;

        //    ExportPath = result.value();
        //}

        if (!Territory.Ready())
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Still loading map.");
        else if (!Territory.Object)
            ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Map data failed to load");

        //Disable mesh export button if export is disabled
        const bool canExport = Territory.Ready() && Territory.Object;
        if (!canExport)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Export"))
        {
            if (!Exporters::ExportTerritory(Territory.Object, ExportPath))
            {
                LOG_ERROR("Failed to export territory '{}'", Territory.Object.Get<string>("Name"))
            }
        }
        if (!canExport)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
            gui::TooltipOnPrevious("You must wait for the map to be fully loaded to export it. See the tasks button at the bottom left of the screen.", state->FontManager->FontDefault.GetFont());
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void TerritoryDocument::Keybinds(GuiState* state)
{
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
    else if (ImGui::IsKeyPressed(VK_DELETE, false) && selectedObject_)
    {
        DeleteObject(selectedObject_);
    }
}

void TerritoryDocument::UpdateDebugDraw(GuiState* state)
{
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
        //Content
        //Todo: Implement creators: player_start, multi_object_marker, weapon, trigger_region, item (for bagman flags), [effect_streaming_node | object_effect] (dunno which I want, or both?)
        gui::LabelAndValue("objectCreationType_:", objectCreationType_);
        if (objectCreationType_ == "player_start")
        {

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
        else if (objectCreationType_ == "effect_streaming_node")
        {

        }
        else if (objectCreationType_ == "object_effect")
        {

        }

        //Buttons
        if (ImGui::Button("Create"))
        {
            //Todo: Create registry object with selected options
                //TODO: Give it a new unique handle
                //TODO: Add to Zone.Objects
                //TODO: Set parent zone handle property
            showObjectCreationPopup_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }
        ImGui::SameLine();
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
    //TODO: See if we can start at 0 instead. All vanilla game handles are very large but they might've be generated them with a hash or something
    //TODO: See if we can discard handle + num completely and regenerate them on export. One problem is that changes to one zone might require regenerating all zones on the SP map (assuming there's cross zone object relations)
    newObject.Set<u32>("Handle", largestHandle + 1);

    ObjectHandle zone = newObject.Get<ObjectHandle>("Zone");
    zone.GetObjectList("Objects").push_back(newObject);
    selectedObject_ = newObject;
}

void TerritoryDocument::DeleteObject(ObjectHandle object)
{
    int mbResult = MessageBox(NULL, "Are you sure you'd like to delete the object?", "Confirm deletion", MB_YESNO);
    if (mbResult == IDYES) //Yes button clicked
    {
        if (object == selectedObject_)
            selectedObject_ = NullObjectHandle;
        //TODO: Add actual object deletion via registry
        //Registry::Get().DeleteObject(selected);
        object.Set<bool>("Deleted", true);
        return;
    }
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
    if (!object || !object.Has("world_anchors"))
        return;

    int mbResult = MessageBox(NULL, "Are you sure you'd like to remove the world anchors from this object? You can't undo this.", "Remove world anchors?", MB_YESNO);
    if (mbResult == IDYES) //Yes button clicked
    {
        object.Remove("world_anchors");

        //Remove world_anchors from RfgPropertyNames so it doesn't cause an export error when it's not found
        std::vector<string> propertyNames = object.GetStringList("RfgPropertyNames");
        auto find = std::ranges::find(propertyNames, "world_anchors");
        if (find != propertyNames.end())
            propertyNames.erase(find);
    }
}

void TerritoryDocument::RemoveDynamicLinks(ObjectHandle object)
{
    if (!object || !object.Has("dynamic_links"))
        return;

    int mbResult = MessageBox(NULL, "Are you sure you'd like to remove the dynamic links from this object? You can't undo this.", "Remove dynamic links?", MB_YESNO);
    if (mbResult == IDYES) //Yes button clicked
    {
        object.Remove("dynamic_links");

        //Remove dynamic_links from RfgPropertyNames so it doesn't cause an export error when it's not found
        std::vector<string> propertyNames = object.GetStringList("RfgPropertyNames");
        auto find = std::ranges::find(propertyNames, "dynamic_links");
        if (find != propertyNames.end())
            propertyNames.erase(find);
    }
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

            //Close zone node if none of it's child objects a visible (based on object type filters)
            bool anyChildrenVisible = Outliner_ZoneAnyChildObjectsVisible(zone);
            bool visible = outliner_OnlyShowNearZones_ ? anyChildrenVisible : true;
            bool selected = (zone == selectedObject_);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | (visible ? 0 : ImGuiTreeNodeFlags_Leaf);
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

    //Object name, handle, num
    u32 handle = selectedObject_.Property("Handle").Get<u32>();
    u32 num = selectedObject_.Property("Num").Get<u32>();
    state->FontManager->FontMedium.Push();
    ImGui::Text(name);
    state->FontManager->FontMedium.Pop();

    //Object type
    ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
    ImGui::Text(fmt::format("{}, {}, {}", selectedObject_.Property("Type").Get<string>(), handle, num));
    ImGui::PopStyleColor();
    ImGui::Separator();

    //Relative objects
    if (ImGui::CollapsingHeader("Relations"))
    {
        ImGui::Indent(15.0f);
        Inspector_DrawObjectHandleEditor(selectedObject_.Property("Zone"));
        Inspector_DrawObjectHandleEditor(selectedObject_.Property("Parent"));
        Inspector_DrawObjectHandleEditor(selectedObject_.Property("Sibling"));
        Inspector_DrawObjectHandleListEditor(selectedObject_.Property("Children"));
        ImGui::Unindent(15.0f);
    }
    if (ImGui::CollapsingHeader("Bounding box"))
    {
        ImGui::Indent(15.0f);
        Inspector_DrawVec3Editor(selectedObject_.Property("Bmin"));
        Inspector_DrawVec3Editor(selectedObject_.Property("Bmax"));
        if (selectedObject_.Has("BBmin")) Inspector_DrawVec3Editor(selectedObject_.Property("BBmin"));
        if (selectedObject_.Has("BBmax")) Inspector_DrawVec3Editor(selectedObject_.Property("BBmax"));
        ImGui::Unindent(15.0f);
    }

    //Draw name, persistent, position, and orient first so they're easy to find
    Inspector_DrawBoolEditor(selectedObject_.Property("Persistent"));
    Inspector_DrawStringEditor(selectedObject_.Property("Name"));
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
    }
    Inspector_DrawMat3Editor(selectedObject_.Property("Orient"));

    //Object properties
    static std::vector<string> HiddenProperties = //These ones aren't drawn or have special logic
    {
        "Handle", "Num", "Flags", "ClassnameHash", "RfgPropertyNames", "BlockSize", "ParentHandle", "SiblingHandle", "ChildHandle",
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
    bool visible = outliner_OnlyShowNearZones_ ? Outliner_ShowObjectOrChildren(object) : true;
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