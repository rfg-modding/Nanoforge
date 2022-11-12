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
#include <RfgTools++/hashes/Hash.h>
#include <RfgTools++/formats/packfiles/Packfile3.h>
#include <WinUser.h>
#include <imgui_internal.h>
#include <imgui.h>
#include <shellapi.h>

CVar CVar_DisableHighQualityTerrain("Disable high quality terrain", ConfigType::Bool,
    "If true high lod terrain won't be used in the map editor. You must close and re-open a map after changing this for it to go into effect.",
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
    "Load and draw building meshes in the map editor. You must close and re-open a map after changing this for it to go into effect.",
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
CVar CVar_HideBoundingBoxes("Hide binding boxes", ConfigType::Bool,
    "Hide all binding boxes in the map editor",
    ConfigValue(false),
    false,  //ShowInSettings
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
    DocumentType = "MapEditor";

    //Determine if high lod terrain and buildings should be loaded. If these are false now you can't reload them without closing a map and re-opening it.
    useHighLodTerrain_ = !CVar_DisableHighQualityTerrain.Get<bool>();
    bool loadBuildings = CVar_DrawChunkMeshes.Get<bool>();

    //Create scene
    Scene = state->Renderer->CreateScene();

    //Init scene camera
    Scene->Cam.Init({ 250.0f, 500.0f, 250.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 1.2f;

    //Start territory loading thread
    Territory.Init(state->PackfileVFS, TerritoryName, TerritoryShortname, useHighLodTerrain_, loadBuildings);
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

    //Used to mark the document as having unsaved changes if Territory::LoadAsync() is called for the first time and does a succesful import. This is mainly so players don't close the project after import without saving and wipe the import
    if (Territory.JustImported && TerritoryLoadTask && !TerritoryLoadTask->Running() && !_handledImport)
    {
        UnsavedChanges = true;
        _handledImport = true;
    }

    //Initialize zone edit tracking for zones which don't have the variable. Projects from older versions don't have this
    if (Territory.Ready() || Territory.Object && !_zoneInitialized)
    {
        _zoneInitialized = true;
        for (ObjectHandle zone : Territory.Object.GetObjectList("Zones"))
            if (!zone.Has("ChildObjectEdited"))
                zone.Set<bool>("ChildObjectEdited", true);
    }

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
            bool hasChildren = selectedObject_.Valid() && selectedObject_.Has("Children") && selectedObject_.GetObjectList("Children").size() > 0;
            bool hasParent = selectedObject_.Valid() && selectedObject_.Get<ObjectHandle>("Parent").Valid();

            if (ImGui::MenuItem("Clone", "Ctrl + D", nullptr, canClone))
            {
                ShallowCloneObject(selectedObject_);
                _scrollToObjectInOutliner = selectedObject_;
            }
            if (ImGui::MenuItem("Deep clone", "", nullptr, canClone))
            {
                DeepCloneObject(selectedObject_, true);
                _scrollToObjectInOutliner = selectedObject_;
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Orphan object", nullptr, nullptr, hasParent))
            {
                orphanObjectPopupHandle_ = selectedObject_;
                orphanObjectPopup_.Open();
            }
            if (ImGui::MenuItem("Orphan children", nullptr, nullptr, hasChildren))
            {
                orphanChildrenPopupHandle_ = selectedObject_;
                orphanChildrenPopup_.Open();
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
            if (ImGui::MenuItem("Add dummy object", nullptr))
            {
                AddGenericObject();
                _scrollToObjectInOutliner = selectedObject_;
            }
            ImGui::SameLine();
            gui::HelpMarker("Creates a dummy object that you can turn into any object type. Improperly configured objects can crash the game, so use at your own risk.", nullptr);

            ImGui::Separator();
            if (ImGui::MenuItem("Delete", "Delete", nullptr, canDelete))
            {
                DeleteObject(selectedObject_);
            }

            ImGui::Separator();
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
        //else if (string mapName = Territory.Object.Get<string>("Name"); mapName == "zonescript_terr01.vpp_pc" || mapName == "zonescript_dlc01.vpp_pc")
        //    ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Export only supports MP and WC maps currently.");
        else
        {
            string mapName = Territory.Object.Get<string>("Name");
            bool isSPMap = mapName == "zonescript_terr01.vpp_pc" || mapName == "zonescript_dlc01.vpp_pc";

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

            //Select original vpp path. Only for binary patch generation
            if (!isSPMap)
            {
                string originalVppPath = originalVppPath_;
                string initialOriginalVppPath = originalVppPath;
                ImGui::InputText("Original vpp_pc path", &originalVppPath);
                ImGui::SameLine();
                if (ImGui::Button("...##OriginalVppPath"))
                {
                    std::optional<string> newPath = OpenFile("vpp_pc");
                    if (newPath)
                        originalVppPath = newPath.value();
                }
                originalVppPath_ = originalVppPath;
                ImGui::SameLine();
                if (ImGui::Button("Auto"))
                {
                    string maybeOriginalPath = state->PackfileVFS->DataFolderPath() + "\\" + Path::GetFileNameNoExtension(Territory.Object.Get<string>("Name")) + ".original.vpp_pc";
                    if (std::filesystem::exists(maybeOriginalPath))
                        originalVppPath_ = maybeOriginalPath;
                }
                ImGui::SameLine();
                gui::HelpMarker("This option is required by the binary patch option. It requires a copy of the maps original vpp_pc file. You can make a copy of the vpp and give it a name like map.original.vpp_pc. The auto button looks for a file with that format.", ImGui::GetDefaultFont());
            }

            //Disable map export button if export is disabled
            const bool canExport = Territory.Ready() && Territory.Object;
            if (!canExport)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
            }

            bool exportPressed = false;
            bool exportBinaryPatchPressed = false;

            exportPressed = ImGui::Button("Export");

            MapExportType exportType;
            if (exportPressed)
                exportType = MapExportType::Vpp;
            else if (exportBinaryPatchPressed)
                exportType = MapExportType::BinaryPatch;

            if (exportPressed || exportBinaryPatchPressed)
            {
                exportTask_ = Task::Create("Export map");
                if (std::filesystem::exists(CVar_MapExportPath.Get<string>()))
                {
                    TaskScheduler::QueueTask(exportTask_, std::bind(&TerritoryDocument::ExportTask, this, state, exportType));
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
            ImGui::TextColored(gui::SecondaryTextColor, _exportResultString);
        }

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
}

void TerritoryDocument::Keybinds(GuiState* state)
{
    PROFILER_FUNCTION();
    if (state->CurrentProject && (state->CurrentProject->Loading || state->CurrentProject->Saving))
    {
        return;
    }

    ImGuiModFlags keyMods = ImGui::GetMergedModFlags();
    if ((keyMods & ImGuiKeyModFlags_Ctrl) == ImGuiKeyModFlags_Ctrl) //Ctrl down
    {
        //Win32 virtual keycodes: https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
        if (ImGui::IsKeyPressed(0x44, false) && selectedObject_) //D
        {
            ShallowCloneObject(selectedObject_);
            _scrollToObjectInOutliner = selectedObject_;
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
    else //Ctrl not down
    {
        if (ImGui::IsKeyPressed(VK_DELETE, false) && selectedObject_ && !ImGui::IsAnyItemActive())
        {
            DeleteObject(selectedObject_);
        }
        if (ImGui::IsKeyPressed(0x46) /*F key*/ && !ImGui::IsAnyItemFocused())
        {
            _highlightHoveredZone = !_highlightHoveredZone; //Toggle hovered zone highlight
        }
        if (ImGui::IsKeyPressed(0x47) /*G key*/ && !ImGui::IsAnyItemFocused())
        {
            _highlightHoveredObject = !_highlightHoveredObject; //Toggle hovered object highlight
        }
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
        ObjectEdited(deleteObjectPopupHandle_);

        //Orphan child objects
        OrphanChildren(deleteObjectPopupHandle_);
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
        ObjectEdited(removeWorldAnchorPopupHandle_);
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
        ObjectEdited(removeDynamicLinkPopupHandle_);
    }
    else if (result == PopupResult::Cancel)
    {
        removeDynamicLinkPopupHandle_ = NullObjectHandle;
    }

    //Orphan single object popup
    if (PopupResult result = orphanObjectPopup_.Update(state); result == PopupResult::Yes)
    {
        OrphanObject(orphanObjectPopupHandle_);
        _scrollToObjectInOutliner = orphanObjectPopupHandle_;
    }

    //Orphan all children popup
    if (PopupResult result = orphanChildrenPopup_.Update(state); result == PopupResult::Yes)
    {
        OrphanChildren(orphanChildrenPopupHandle_);
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
            ZoneObjectClass* objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());
            if (!objectClass)
                continue;
            if (!objectClass->Show)
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
                //Show secondary bbox instead if T key is held down or option is checked in bbox header. If the checkbox is toggled T is used to toggle the primary bbox instead.
                if (ImGui::IsKeyDown(0x54) != drawSecondaryBbox_)
                {
                    if (object.Has("BBmin") && object.Has("BBmax"))
                    {
                        bmin = object.Get<Vec3>("BBmin");
                        bmax = object.Get<Vec3>("BBmax");
                    }
                }

                //Calculate color that changes with time
                Vec3 color = objectClass->Color;
                f32 colorMagnitude = objectClass->Color.Magnitude();
                //Negative values used for brighter colors so they get darkened instead of lightened//Otherwise doesn't work on objects with white debug color
                f32 multiplier = colorMagnitude > 0.85f ? -1.0f : 1.0f;
                color.x = objectClass->Color.x + powf(sin(Scene->TotalTime * 2.0f), 2.0f) * multiplier;
                color.y = objectClass->Color.y + powf(sin(Scene->TotalTime), 2.0f) * multiplier;
                color.z = objectClass->Color.z + powf(sin(Scene->TotalTime), 2.0f) * multiplier;

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
                if (!CVar_HideBoundingBoxes.Get<bool>())
                {
                    if (objectClass->DrawSolid)
                        Scene->DrawBoxLit(bmin, bmax, color);
                    else
                        Scene->DrawBox(bmin, bmax, color);
                }

                //Draw long vertical line to help locate the selected object
                if (ImGui::IsKeyDown(0x52) /*R key*/) //Only show when R key is down
                    Scene->DrawLine(lineStart, lineEnd, color);

                //Draw lines indicating object orientation (red = right, green = up, blue = forward)
                if (CVar_DrawObjectOrientLines.Get<bool>() && object.Has("Orient"))
                {
                    //Determine line length
                    Vec3 size = bmax - bmin;
                    f32 orientLineScale = 2.0f; //How many times larger than the object orient lines should be
                    f32 lineLength = orientLineScale * std::max({ size.x, size.y, size.z }); //Make lines equal length, as long as the widest side of the bbox

                    Mat3 orient = object.Get<Mat3>("Orient");
                    Vec3 center = object.Get<Vec3>("Position");//bmin + (size / 2.0f);
                    Scene->DrawLine(center, center + (orient.rvec * lineLength), { 1.0f, 0.0f, 0.0f }); //Right
                    Scene->DrawLine(center, center + (orient.uvec * lineLength), { 0.0f, 1.0f, 0.0f }); //Up
                    Scene->DrawLine(center, center + (orient.fvec * lineLength), { 0.0f, 0.0f, 1.0f }); //Forward
                }
            }
            else //If not selected just draw bounding box with static color
            {
                if (!CVar_HideBoundingBoxes.Get<bool>() && object.Has("Orient"))
                {
                    if (objectClass->DrawSolid)
                        Scene->DrawBoxLit(bmin, bmax, objectClass->Color);
                    else
                        Scene->DrawBox(bmin, bmax, objectClass->Color);
                }
            }
        }
    }

    //Draw a solid box around the zone being mouse hovered in the outliner if that option is enabled
    if (_highlightHoveredZone && _hoveredZone.Valid())
    {
        //Attempt to calculate zone XZ coorsd based on shortname
        string shortName = _hoveredZone.Get<string>("ShortName");
        std::vector<std::string_view> split = String::SplitString(shortName, "_");
        if (split.size() == 2)
        {
            std::string_view xStr = split[0];
            std::string_view zStr = split[1];
            
            //Strip 0 from start of coord if it starts with that. E.g. 07 -> 7
            if (xStr.starts_with('0'))
                xStr.remove_prefix(1);
            if (zStr.starts_with('0'))
                zStr.remove_prefix(1);

            //String to int
            i32 zoneX = -1;
            i32 zoneZ = -1;
            std::from_chars(xStr.data(), xStr.data() + xStr.size(), zoneX);
            std::from_chars(zStr.data(), zStr.data() + zStr.size(), zoneZ);

            if (zoneX != -1 && zoneZ != -1)
            {
                //Calculate zone box size & position
                Vec3 zoneOrigin = { -4088.0, 0.0f, 4088.0 }; //Position of the corner of a zone with xz coords 0,0 in world coordinates (x,y,z)
                f32 zoneWidth = 511.0f; //Width of the xz axes of a zone in world coordinates
                f32 zoneHeight = _zoneBoxHeight; //Height of a zone in world coordinates

                Vec3 zoneBmin = zoneOrigin;
                zoneBmin.x += (f32)zoneX * zoneWidth;
                zoneBmin.z -= (f32)zoneZ * zoneWidth;
                zoneBmin.y = -zoneHeight;

                Vec3 zoneBmax = zoneBmin;
                zoneBmax.x += zoneWidth;
                zoneBmax.z -= zoneWidth;
                zoneBmax.y = zoneHeight;

                //Finally, draw the box
                Vec3 color = { 0.0f, (f32)zoneX / 15.0f, (f32)zoneZ / 15.0f }; //{ 0.8f, 0.0f, 0.0f };
                Scene->DrawBoxSolid(zoneBmin, zoneBmax, color);
            }
        }
    }

    if (_highlightHoveredObject && _hoveredObject.Valid())
    {
        Vec3 bmin = _hoveredObject.Property("Bmin").Get<Vec3>();
        Vec3 bmax = _hoveredObject.Property("Bmax").Get<Vec3>();
        Vec3 pos = _hoveredObject.Has("Position") ? _hoveredObject.Property("Position").Get<Vec3>() : Vec3{ 0.0f, 0.0f, 0.0f };

        //Calculate color that changes with time so it's easy to find in dense maps
        Vec3 color = { 0.7f, 0.0f, 0.5f };
        ZoneObjectClass* objectClass = Territory.GetObjectClass(_hoveredObject.Property("ClassnameHash").Get<u32>());
        if (objectClass)
        {
            f32 colorMagnitude = objectClass->Color.Magnitude();
            //Negative values used for brighter colors so they get darkened instead of lightened//Otherwise doesn't work on objects with white debug color
            f32 multiplier = colorMagnitude > 0.85f ? -1.0f : 1.0f;
            color.x = objectClass->Color.x + powf(sin(Scene->TotalTime * 2.0f), 2.0f) * multiplier;
            color.y = objectClass->Color.y + powf(sin(Scene->TotalTime), 2.0f) * multiplier;
            color.z = objectClass->Color.z + powf(sin(Scene->TotalTime), 2.0f) * multiplier;
        }

        //Keep color in a certain range so it stays visible against the terrain
        f32 magnitudeMin = 0.20f;
        f32 colorMin = 0.20f;
        if (color.Magnitude() < magnitudeMin)
        {
            color.x = std::max(color.x, colorMin);
            color.y = std::max(color.y, colorMin);
            color.z = std::max(color.z, colorMin);
        }

        Scene->DrawBoxSolid(bmin, bmax, color);
    }

    Scene->NeedsRedraw = true;
    updateDebugDraw_ = false;
}

void TerritoryDocument::DrawObjectCreationPopup(GuiState* state)
{
    //if (showObjectCreationPopup_)
    //    ImGui::OpenPopup("Create object");
    //if (ImGui::BeginPopupModal("Create object", nullptr, ImGuiWindowFlags_NoCollapse))
    //{
    //    static bool persistent = false;
    //    //Todo: Split each creator into separate functions. Starting simple to bootstrap the map editor. This won't scale to the 20+ object types the game has though.

    //    //player_start fields
    //    static bool playerStart_Respawn = true;
    //    static bool playerStart_Indoor = false;
    //    static bool playerStart_InitialSpawn = false;
    //    static string playerStart_MpTeam = "EDF";
    //    //Todo: Implement selectors for these when SP support is added to map editor
    //    //static bool playerStart_ChekpointRespawn = false;
    //    //static bool playerStart_ActivityRespawn = false;
    //    //static string playerStart_MissionInfo;

    //    //multi_object_marker fields


    //    //weapon fields


    //    //trigger_region fields


    //    //item fields


    //    bool notImplemented = false;
    //    state->FontManager->FontMedium.Push();
    //    ImGui::Text("Creating %s", objectCreationType_.c_str());
    //    state->FontManager->FontMedium.Pop();

    //    ImGui::Checkbox("Persistent", &persistent);

    //    //Content
    //    if (objectCreationType_ == "player_start")
    //    {
    //        static std::vector<string> playerTeamOptions = { "EDF", "Guerilla" /*Mispelled because the game mispelled it*/, "Neutral" };
    //        if (ImGui::BeginCombo("MP team", playerStart_MpTeam.c_str()))
    //        {
    //            for (const string& str : playerTeamOptions)
    //            {
    //                bool selected = (str == playerStart_MpTeam);
    //                if (ImGui::Selectable(str.c_str(), &selected))
    //                {
    //                    playerStart_MpTeam = str;
    //                }
    //            }
    //            ImGui::EndCombo();
    //        }
    //        ImGui::Checkbox("Respawn", &playerStart_Respawn);
    //        ImGui::Checkbox("Indoor", &playerStart_Indoor);
    //        ImGui::Checkbox("Initial spawn", &playerStart_InitialSpawn);
    //    }
    //    else if (objectCreationType_ == "multi_object_marker")
    //    {

    //    }
    //    else if (objectCreationType_ == "weapon")
    //    {

    //    }
    //    else if (objectCreationType_ == "trigger_region")
    //    {

    //    }
    //    else if (objectCreationType_ == "item")
    //    {
    //        //Todo: Consider having separate creator option/shortcut for bagman flags
    //    }
    //    else
    //    {
    //        notImplemented = true;
    //        ImGui::Text("Creator not implemented for this type...");
    //    }

    //    //Create/cancel buttons
    //    if (!notImplemented && ImGui::Button("Create"))
    //    {
    //        //Don't bother creating a new object if there isn't an unused handle available.
    //        u32 newHandle = GetNewObjectHandle();
    //        if (newHandle == 0xFFFFFFFF)
    //        {
    //            LOG_ERROR("Failed to create new object. Couldn't find unused object handle. Please report this to a developer. This shouldn't really ever happen.");
    //            return;
    //        }

    //        ObjectHandle newObject = Registry::Get().CreateObject("", objectCreationType_);
    //        newObject.Set<u32>("Handle", newHandle);
    //        newObject.Set<u32>("Num", 0);
    //        newObject.Set<Vec3>("Bmin", { 0.0f, 0.0f, 0.0f });
    //        newObject.Set<Vec3>("Bmin", { 5.0f, 5.0f, 5.0f }); //TODO: Come up with a better way of generating default bbox

    //        if (objectCreationType_ == "player_start")
    //        {
    //            u16 flags = 65280;
    //            if (persistent) flags += 1;

    //            //Create new object and give it a unique handle
    //            newObject.Set<string>("Name", playerStart_MpTeam + " player start");
    //            newObject.Set<u32>("ClassnameHash", 1794022917);
    //            newObject.Set<u16>("Flags", flags);
    //            newObject.Set<u16>("BlockSize", 0);
    //            newObject.Set<u16>("NumProps", 0);
    //            newObject.Set<u16>("PropBlockSize", 0);
    //            newObject.Set<u32>("ParentHandle", 0xFFFFFFFF);
    //            newObject.Set<u32>("SiblingHandle", 0xFFFFFFFF);
    //            newObject.Set<u32>("ChildHandle", 0xFFFFFFFF);
    //            newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
    //            newObject.Set<ObjectHandle>("Sibling", NullObjectHandle);
    //            newObject.SetObjectList("Children", {});
    //            newObject.Set<bool>("Persistent", persistent);

    //            //Add object to zone and select it
    //            ObjectHandle zone = Territory.Object.GetObjectList("Zones")[0];
    //            zone.GetObjectList("Objects").push_back(newObject);
    //            newObject.Set<ObjectHandle>("Zone", zone);
    //            selectedObject_ = newObject;
    //        }
    //        else if (objectCreationType_ == "multi_object_marker")
    //        {

    //        }
    //        else if (objectCreationType_ == "weapon")
    //        {

    //        }
    //        else if (objectCreationType_ == "trigger_region")
    //        {

    //        }
    //        else if (objectCreationType_ == "item")
    //        {
    //            //Todo: Consider having separate creator option/shortcut for bagman flags
    //        }

    //        showObjectCreationPopup_ = false;
    //        ImGui::CloseCurrentPopup();
    //        ImGui::EndPopup();
    //        return;
    //    }
    //    if (!notImplemented) ImGui::SameLine();
    //    if (ImGui::Button("Cancel"))
    //    {
    //        //TODO: Delete temporary object if one is used for this
    //        showObjectCreationPopup_ = false;
    //        ImGui::CloseCurrentPopup();
    //        ImGui::EndPopup();
    //        return;
    //    }
    //    ImGui::EndPopup();
    //}
}

ObjectHandle TerritoryDocument::SimpleCloneObject(ObjectHandle object, bool useTerritoryZone)
{
    //Don't bother creating a new object if there isn't an unused handle available.
    u32 newHandle = GetNewObjectHandle();
    u32 newNum = GetNewObjectNum();
    if (newHandle == 0xFFFFFFFF)
    {
        LOG_ERROR("Failed to clone object {}. Couldn't find unused object handle. Please report this to a developer. This shouldn't really ever happen.", object.UID());
        return NullObjectHandle;
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
    newObject.Set<u32>("Num", newNum);

    //Add to zone + set as edited
    if (useTerritoryZone) //Add the object to the first zone in the map. Used when transferring objects between maps since we don't want to add it to a zone in the source map
    {
        ObjectHandle zone = Territory.Object.GetObjectList("Zones")[0];
        newObject.Set<ObjectHandle>("Zone", zone);
        zone.AppendObjectList("Objects", newObject);
    }
    else //Add the object to the same zone as the original object
    {
        ObjectHandle zone = newObject.Get<ObjectHandle>("Zone");
        zone.GetObjectList("Objects").push_back(newObject);
    }
    ObjectEdited(newObject);

    return newObject;
}

ObjectHandle TerritoryDocument::ShallowCloneObject(ObjectHandle object, bool selectNewObject, bool useTerritoryZone)
{
    ObjectHandle newObject = SimpleCloneObject(object, useTerritoryZone);

    //Clear relative handles. These are only cloned in a deep copy
    newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
    newObject.Set<ObjectHandle>("Child", NullObjectHandle);
    newObject.SetObjectList("Children", {});
    ObjectEdited(newObject);

    if (selectNewObject)
    {
        selectedObject_ = newObject;
        _scrollToObjectInOutliner = newObject;
    }

    return newObject;
}

ObjectHandle TerritoryDocument::DeepCloneObject(ObjectHandle object, bool selectNewObject, u32 depth, bool useTerritoryZone)
{
    if (depth > MAX_DEEP_CLONE_DEPTH)
    {
        LOG_ERROR("Deep clone error! Exceeded maximum deep clone hierarchy depth of {}", MAX_DEEP_CLONE_DEPTH);
        return NullObjectHandle;
    }

    ObjectHandle newObject = SimpleCloneObject(object, useTerritoryZone);
    std::vector<ObjectHandle> oldChildren = newObject.GetObjectList("Children"); //Make a copy of the children list so we can edit it while iterating
    std::vector<ObjectHandle>& children = newObject.GetObjectList("Children");
    children.clear();

    //Clone children
    for (ObjectHandle oldChild : oldChildren)
    {
        ObjectHandle newChild = DeepCloneObject(oldChild, false, depth + 1, useTerritoryZone);
        if (newChild)
        {
            newChild.Set<ObjectHandle>("Parent", newObject);
            children.push_back(newChild);
        }
    }

    //Set first child handle
    if (children.size() > 0)
    {
        newObject.Set<ObjectHandle>("Child", children[0]); //Note: You shouldn't use this property anymore. Just updating for compatibility sake. Use Children list instead
    }
    ObjectEdited(newObject);

    //Clear parent if depth == 0. The object being cloned becomes a root object
    if (depth == 0)
    {
        newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
    }

    if (selectNewObject)
    {
        selectedObject_ = newObject;
        _scrollToObjectInOutliner = newObject;
    }

    return newObject;
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

void TerritoryDocument::AddGenericObject(ObjectHandle parent)
{
    Registry& registry = Registry::Get();
    std::string_view classname = "object_dummy";
    u32 classnameHash = Hash::HashVolition(classname);
    u32 handle = GetNewObjectHandle();
    u32 num = GetNewObjectNum();
    bool persistent = true;
    u16 flags = 65280;
    if (persistent)
        flags |= 1;

    Vec3 bmin = { 0.0f, 0.0f, 0.0f };
    Vec3 bmax = { 10.0f, 10.0f, 10.0f };
    Vec3 position = bmin + ((bmax - bmin) / 2.0f);

    if (classnameHash != 2671133140)
    {
        ShowMessageBox("Failed to calculate object_dummy classname hash. Someone broke the hashing function... Contact a Nanoforge developer.", "Invalid classname hash");
        return;
    }
    if (handle == 0xFFFFFFFF)
    {
        ShowMessageBox("Failed to create generic object. Couldn't find an unused object handle. Contact a Nanoforge developer.", "No unused handles");
        return;
    }

    ObjectHandle newObj = registry.CreateObject("", classname);
    newObj.Set<u32>("ClassnameHash", classnameHash);
    newObj.Set<u32>("Handle", handle);
    newObj.Set<u32>("Num", num);
    newObj.Set<Vec3>("Bmin", bmin);
    newObj.Set<Vec3>("Bmax", bmax);
    newObj.Set<u16>("Flags", flags); //TODO: Find good default with non persistence
    newObj.Set<u16>("BlockSize", 0); //This gets recalculated on export so we just set it to 0
    newObj.Set<u32>("ParentHandle", 0xFFFFFFFF);
    newObj.Set<u32>("SiblingHandle", 0xFFFFFFFF);
    newObj.Set<u32>("ChildHandle", 0xFFFFFFFF);
    newObj.Set<u16>("NumProps", 0); //Also re-calculated on export
    newObj.Set<u16>("PropBlockSize", 0); //^^^
    newObj.Set<ObjectHandle>("Parent", parent);
    newObj.Set<ObjectHandle>("Sibling", NullObjectHandle);
    newObj.Set<bool>("Persistent", persistent);
    newObj.SetStringList("RfgPropertyNames", { "op" });

    //Add properties that all object types have
    newObj.Set<Vec3>("Position", position);
    newObj.Set<Mat3>("Orient", {});

    //Set zone + add object to zones object list
    ObjectHandle zone = Territory.Zones[0];
    newObj.Set<ObjectHandle>("Zone", zone);
    zone.AppendObjectList("Objects", newObj);
    selectedObject_ = newObj;
    ObjectEdited(selectedObject_);

    if (parent)
    {
        parent.AppendObjectList("Children", newObj);
        ObjectEdited(parent);
    }
}

void TerritoryDocument::OrphanObject(ObjectHandle object)
{
    ObjectHandle parent = object.Get<ObjectHandle>("Parent");
    if (parent)
    {
        std::vector<ObjectHandle>& oldChildrenList = parent.GetObjectList("Children");
        oldChildrenList.erase(std::remove(oldChildrenList.begin(), oldChildrenList.end(), object), oldChildrenList.end());
    }

    object.Set<ObjectHandle>("Parent", NullObjectHandle);
    ObjectEdited(object);
}

void TerritoryDocument::OrphanChildren(ObjectHandle parent)
{
    std::vector<ObjectHandle> children = parent.GetObjectList("Children"); //Get a copy of the object list. Don't use a reference since OrphanObject() modifies the child list
    for (ObjectHandle child : children)
        OrphanObject(child);

    parent.GetObjectList("Children").clear(); //Clear just in case OrphanObject didn't get them all
    ObjectEdited(parent);
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

u32 TerritoryDocument::GetNewObjectNum()
{
    //Give new object a unique handle
    u32 largestNum = 0;
    for (ObjectHandle zone : Territory.Object.GetObjectList("Zones"))
    {
        for (ObjectHandle obj : zone.GetObjectList("Objects"))
        {
            if (obj.Get<u32>("Num") > largestNum)
                largestNum = obj.Get<u32>("Num");
        }
    }

    //Error if handle is out of valid range
    if (largestNum >= std::numeric_limits<u32>::max() - 1)
    {
        LOG_ERROR("Failed to find unused object handle. Returning 0xFFFFFFFF.");
        return 0xFFFFFFFF;
    }

    //TODO: See if we can start at 0 instead
    //TODO: See if we can discard handle + num completely and regenerate them on export. One problem is that changes to one zone might require regenerating all zones on the SP map (assuming there's cross zone object relations)
    return largestNum + 1;
}

void TerritoryDocument::Outliner(GuiState* state)
{
    PROFILER_FUNCTION();

    Outliner_DrawFilters(state);

    //Clear hovered objects so if no object is hovered this frame they're null
    _hoveredZone = NullObjectHandle;
    _hoveredObject = NullObjectHandle;
    _contextMenuAddDummyChildParent = NullObjectHandle;

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
        bool anyZoneHovered = false;
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

            //Track which zone is hovered for zone highlight option
            bool zoneNodeHovered = ImGui::IsItemHovered();
            if (zoneNodeHovered)
            {
                _hoveredZone = zone;
                anyZoneHovered = true;
            }

            if (singleZone || treeNodeOpen)
            {
                for (ObjectHandle object : zone.Property("Objects").GetObjectList())
                {
                    //Don't draw objects with parents at the top of the tree. They'll be drawn as subnodes when their parent calls DrawObjectNode()
                    if (!object || (object.Has("Parent") && object.Property("Parent").Get<ObjectHandle>()))
                        continue;
                    if (object.Has("Deleted") && object.Get<bool>("Deleted"))
                        continue;

                    u32 depth = 0;
                    Outliner_DrawObjectNode(state, object, depth + 1);
                }
                ImGui::TreePop();
            }
        }

        if (!anyZoneHovered)
        {
            _hoveredZone = NullObjectHandle;
        }
        if (_scrollToBottomOfOutliner)
        {
            ImGui::SetScrollY(1.0f);
            _scrollToBottomOfOutliner = false;
        }

        ImGui::PopStyleColor(3);
        ImGui::EndTable();
    }

    if (_contextMenuAddDummyChildParent)
    {
        AddGenericObject(_contextMenuAddDummyChildParent);
        _openNodeNextFrame = _contextMenuAddDummyChildParent;
        _contextMenuAddDummyChildParent = NullObjectHandle;
    }
    if (_contextMenuShallowCloneObject)
    {
        ShallowCloneObject(_contextMenuShallowCloneObject);
        _scrollToObjectInOutliner = selectedObject_;
        _contextMenuShallowCloneObject = NullObjectHandle;
    }
    if (_contextMenuDeepCloneObject)
    {
        DeepCloneObject(_contextMenuDeepCloneObject);
        _scrollToObjectInOutliner = selectedObject_;
        _contextMenuDeepCloneObject = NullObjectHandle;
    }
}

string GetObjectHandleName(ObjectHandle handle)
{
    string name = "";
    if (handle.Has("Name"))
    {
        name = handle.Property("Name").Get<string>();
        if (name == "")
            name = std::to_string(handle.UID());

        name += ", " + std::to_string(handle.Get<u32>("Num"));
    }
    else
    {
        name = std::to_string(handle.UID());
    }

    return name;
}

void TerritoryDocument::SetObjectParent(ObjectHandle object, ObjectHandle oldParent, ObjectHandle newParent)
{
    /*
        Note: We don't need to update the Sibling handle in this function. Thats now recalculated on map export (see rfg/export/MapExporter.cpp)
              Object hierarchies should be traversed using the Parent and Children properties of objects instead. 
              Sibling will likely be removed in the rewrite since its not needed anymore, not kept up to date, and a bigger pain to use than the parent handle + child lists
    */

    if (oldParent == newParent)
        return; //Nothing will change

    //Don't allow objects to be their own parent
    if (object == newParent)
    {
        LOG_ERROR("Failed to update relation data! Tried to make an object its own parent.");
        return;
    }

    //Remove object from old parents children list
    if (oldParent)
    {
        std::vector<ObjectHandle>& oldChildrenList = oldParent.GetObjectList("Children");
        oldChildrenList.erase(std::remove(oldChildrenList.begin(), oldChildrenList.end(), object), oldChildrenList.end());
    }
    object.Set<ObjectHandle>("Parent", NullObjectHandle);

    if (!newParent)
        return; //Orphaning the object. Already cleared parent handle + removed child from the parents children list. Nothing more to do

    //Setting new parent
    newParent.AppendObjectList("Children", object);
    object.Set<ObjectHandle>("Parent", newParent);
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
        name = "Unnamed";// selectedObject_.Property("Type").Get<string>();

    //Name
    state->FontManager->FontMedium.Push();
    if (_inspectorEditingName)
    {
        auto applyEdit = [&]()
        {
            selectedObject_.Set<string>("Name", _inspectorNameEditBuffer);
            _inspectorEditingName = false;
            ObjectEdited(selectedObject_);
        };
        if (ImGui::InputText("##ObjectNameEdit", &_inspectorNameEditBuffer, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            applyEdit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            applyEdit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            _inspectorEditingName = false;
        }
    }
    else
    {
        ImGui::Text(name);
        ImGui::SameLine();
        if (ImGui::Button("Edit name"))
        {
            _inspectorEditingName = true;
            _inspectorNameEditBuffer = selectedObject_.Get<string>("Name");
        }
    }
    state->FontManager->FontMedium.Pop();

    //Object class selector
    string objectClassname = selectedObject_.Get<string>("Type");
    if (ImGui::BeginCombo("##ClassnameCombo", objectClassname.c_str()))
    {
        //Search bar
        ImGui::InputText("Search", &_inspectorObjectClassSelectorSearch);
        ImGui::SameLine();
        if (ImGui::Button(ICON_VS_CHROME_CLOSE))
        {
            _inspectorObjectClassSelectorSearch = "";
        }
        ImGui::Separator();

        //Entries
        for (ZoneObjectClass& objectClass : Territory.ZoneObjectClasses)
        {
            if (objectClass.Name == "unknown")
                continue;
            if (_inspectorObjectClassSelectorSearch != "" && !String::Contains(String::ToLower(objectClass.Name), String::ToLower(_inspectorObjectClassSelectorSearch)))
                continue;

            bool selected = (objectClass.Name == objectClassname);
            if (ImGui::Selectable(objectClass.Name.c_str(), selected))
            {
                //Set new object class. Must update relevant properties in registry object
                selectedObject_.Set<u32>("ClassnameHash", objectClass.Hash);
                selectedObject_.Set<string>("Type", objectClass.Name);
                ObjectEdited(selectedObject_);
            }
        }

        ImGui::EndCombo();
    }

    //Handle, num
    u32 handle = selectedObject_.Property("Handle").Get<u32>();
    u32 num = selectedObject_.Property("Num").Get<u32>();
    ImGui::Text(fmt::format("{}, {}", handle, num).c_str());
    
    //Description
    string description = selectedObject_.Get<string>("Description");
    ImGui::InputText("Description", &description);
    selectedObject_.Set<string>("Description", description);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Bounding box"))
    {
        ImGui::Indent(15.0f);
        ImGui::Checkbox("Draw secondary bbox", &drawSecondaryBbox_);
        gui::TooltipOnPrevious("When checked the secondary bbox (BBmin and BBmax) will be drawn instead of the primary. You can also hold the `T` key to temporarily show the secondary bbox. While this is checked the `T` key will shown the primary bbox instead.");

        /* Primary BBox - Objects are required to have this */
        if (ImGui::Button("Center on position"))
        {
            Vec3 bboxSize = selectedObject_.Get<Vec3>("Bmax") - selectedObject_.Get<Vec3>("Bmin");
            Vec3 newBmin = selectedObject_.Get<Vec3>("Position") - (bboxSize / 2.0f);
            Vec3 newBmax = selectedObject_.Get<Vec3>("Position") + (bboxSize / 2.0f);
            selectedObject_.Set<Vec3>("Bmin", newBmin);
            selectedObject_.Set<Vec3>("Bmax", newBmax);
            ObjectEdited(selectedObject_);
        }
        gui::TooltipOnPrevious("Move the primary bbox so that its center is at the object position");
        if (selectedObject_.Has("BBmin") && selectedObject_.Has("BBmax")) //Calculate Bmin & Bmax from the secondary bbox
        {
            ImGui::SameLine();
            if (ImGui::Button("Calculate##CalcPrimaryBBox"))
            {
                selectedObject_.Set<Vec3>("Bmin", selectedObject_.Get<Vec3>("BBmin") + selectedObject_.Get<Vec3>("Position"));
                selectedObject_.Set<Vec3>("Bmax", selectedObject_.Get<Vec3>("BBmax") + selectedObject_.Get<Vec3>("Position"));
                ObjectEdited(selectedObject_);
            }
            gui::TooltipOnPrevious("Calculate primary bbox from BBmin and BBmax. Equation is `BBprimary = BBsecondary + position`");
        }
        if (Inspector_DrawVec3Editor(selectedObject_.Property("Bmin")))
            ObjectEdited(selectedObject_);
        if (Inspector_DrawVec3Editor(selectedObject_.Property("Bmax")))
            ObjectEdited(selectedObject_);


        /* Secondary BBox - Objects aren't required to have this */
        ImGui::Separator();
        if (selectedObject_.Has("BBmin") && selectedObject_.Has("BBmax"))
        {
            bool removedSecondaryBbox = false;
            if (ImGui::Button("Remove secondary bbox"))
            {
                selectedObject_.Remove("BBmin");
                selectedObject_.Remove("BBmax");

                //Remove from properties list
                const auto [first, last] = std::ranges::remove_if(selectedObject_.GetStringList("RfgPropertyNames"), [](string& str) { return str == "bb"; });
                selectedObject_.GetStringList("RfgPropertyNames").erase(first, last);
                ObjectEdited(selectedObject_);
                removedSecondaryBbox = true;
            }

            if (!removedSecondaryBbox)
            {
                //Calculate BBMin & BBmax from the primary bbox
                ImGui::SameLine();
                if (ImGui::Button("Calculate##CalcSecondaryBBox"))
                {
                    selectedObject_.Set<Vec3>("BBmin", selectedObject_.Get<Vec3>("Bmin") - selectedObject_.Get<Vec3>("Position"));
                    selectedObject_.Set<Vec3>("BBmax", selectedObject_.Get<Vec3>("Bmax") - selectedObject_.Get<Vec3>("Position"));
                    ObjectEdited(selectedObject_);
                }
                gui::TooltipOnPrevious("Calculate secondary bbox from Bmin and Bmax. Equation is `BBsecondary = BBprimary - position`");

                //Bbox editor
                if (Inspector_DrawVec3Editor(selectedObject_.Property("BBmin")))
                    ObjectEdited(selectedObject_);
                if (Inspector_DrawVec3Editor(selectedObject_.Property("BBmax")))
                    ObjectEdited(selectedObject_);
            }
        }
        else //Doesn't have a secondary bbox (BBmin and BBmax)
        {
            if (ImGui::Button("Add secondary bbox"))
            {
                selectedObject_.Set<Vec3>("BBmin", selectedObject_.Get<Vec3>("Bmin") - selectedObject_.Get<Vec3>("Position"));
                selectedObject_.Set<Vec3>("BBmax", selectedObject_.Get<Vec3>("Bmax") - selectedObject_.Get<Vec3>("Position"));
                if (std::ranges::count_if(selectedObject_.GetStringList("RfgPropertyNames"), [](string& str) { return str == "bb"; }) == 0)
                {
                    selectedObject_.AppendStringList("RfgPropertyNames", "bb");
                }
                ObjectEdited(selectedObject_);
            }
        }

        ImGui::Unindent(15.0f);
    }
    if (ImGui::CollapsingHeader("Flags##FlagsHeader"))
    {
        ImGui::Indent(15.0f);

        //Option to edit flags as a single integer
        _inspectorManualFlagSetter = selectedObject_.Get<u16>("Flags");
        if (ImGui::InputScalar("Merged##FlagsSingleInt", ImGuiDataType_U16, &_inspectorManualFlagSetter))
        {
            selectedObject_.Set<u16>("Flags", _inspectorManualFlagSetter);
            ObjectEdited(selectedObject_);
        }

        //Separate checkboxes for flag
        u16 flags = selectedObject_.Get<u16>("Flags");
#define GET_FLAG(index) (flags & (1 << index)) != 0

        bool persistent = GET_FLAG(0);
        bool flag1      = GET_FLAG(1);
        bool flag2      = GET_FLAG(2);
        bool flag3      = GET_FLAG(3);
        bool flag4      = GET_FLAG(4);
        bool flag5      = GET_FLAG(5);
        bool flag6      = GET_FLAG(6);
        bool flag7      = GET_FLAG(7);
        bool spawnInAnarchy      = GET_FLAG(8);
        bool spawnInTeamAnarchy      = GET_FLAG(9);
        bool spawnInCTF     = GET_FLAG(10);
        bool spawnInSiege     = GET_FLAG(11);
        bool spawnInDamageControl     = GET_FLAG(12);
        bool spawnInBagman     = GET_FLAG(13);
        bool spawnInTeamBagman     = GET_FLAG(14);
        bool spawnInDemolition     = GET_FLAG(15);

        bool changed = false;
        if (ImGui::Checkbox("Persistent", &persistent)) changed = true;
        if (ImGui::Checkbox("Flag1", &flag1))           changed = true;
        if (ImGui::Checkbox("Flag2", &flag2))           changed = true;
        if (ImGui::Checkbox("Flag3", &flag3))           changed = true;
        if (ImGui::Checkbox("Flag4", &flag4))           changed = true;
        if (ImGui::Checkbox("Flag5", &flag5))           changed = true;
        if (ImGui::Checkbox("Flag6", &flag6))           changed = true;
        if (ImGui::Checkbox("Flag7", &flag7))           changed = true;
        if (ImGui::Checkbox("Spawn in Anarchy", &spawnInAnarchy))           changed = true;
        if (ImGui::Checkbox("Spawn in Team Anarchy", &spawnInTeamAnarchy))           changed = true;
        if (ImGui::Checkbox("Spawn in CTF", &spawnInCTF))         changed = true;
        if (ImGui::Checkbox("Spawn in Siege", &spawnInSiege))         changed = true;
        if (ImGui::Checkbox("Spawn in Damage Control", &spawnInDamageControl))         changed = true;
        if (ImGui::Checkbox("Spawn in Bagman", &spawnInBagman))         changed = true;
        if (ImGui::Checkbox("Spawn in Team Bagman", &spawnInTeamBagman))         changed = true;
        if (ImGui::Checkbox("Spawn in Demolition", &spawnInDemolition))         changed = true;

        if (changed)
        {
            u16 newFlags = 0;
            if (persistent) newFlags |= (1 << 0);
            if (flag1)      newFlags |= (1 << 1);
            if (flag2)      newFlags |= (1 << 2);
            if (flag3)      newFlags |= (1 << 3);
            if (flag4)      newFlags |= (1 << 4);
            if (flag5)      newFlags |= (1 << 5);
            if (flag6)      newFlags |= (1 << 6);
            if (flag7)      newFlags |= (1 << 7);
            if (spawnInAnarchy)      newFlags |= (1 << 8);
            if (spawnInTeamAnarchy)      newFlags |= (1 << 9);
            if (spawnInCTF)     newFlags |= (1 << 10);
            if (spawnInSiege)     newFlags |= (1 << 11);
            if (spawnInDamageControl)     newFlags |= (1 << 12);
            if (spawnInBagman)     newFlags |= (1 << 13);
            if (spawnInTeamBagman)     newFlags |= (1 << 14);
            if (spawnInDemolition)     newFlags |= (1 << 15);

            selectedObject_.Set<bool>("Persistent", persistent);
            selectedObject_.Set<u16>("Flags", newFlags);
            ObjectEdited(selectedObject_);
        }
        ImGui::Unindent(15.0f);
    }

    Inspector_DrawRelativeEditor();

    //Draw position and orient first so they're easy to find
    Vec3 initialPos = selectedObject_.Get<Vec3>("Position");
    if (Inspector_DrawVec3Editor(selectedObject_.Property("Position")))
    {
        //Auto update bounding box positions when position is changed
        Vec3 delta = selectedObject_.Get<Vec3>("Position") - initialPos;
        if (selectedObject_.Has("Bmin"))
            selectedObject_.Set<Vec3>("Bmin", selectedObject_.Get<Vec3>("Bmin") + delta);
        if (selectedObject_.Has("Bmax"))
            selectedObject_.Set<Vec3>("Bmax", selectedObject_.Get<Vec3>("Bmax") + delta);

        //If object has a chunk mesh update its position
        if (Territory.Chunks.contains(selectedObject_.UID()))
        {
            RenderChunk* chunk = Territory.Chunks[selectedObject_.UID()];
            chunk->Position += delta;
        }

        ObjectEdited(selectedObject_);
    }
    if (selectedObject_.Has("Orient") && Inspector_DrawMat3Editor(selectedObject_.Property("Orient")))
    {
        //If object has a chunk mesh update its orient
        if (Territory.Chunks.contains(selectedObject_.UID()))
        {
            RenderChunk* chunk = Territory.Chunks[selectedObject_.UID()];
            chunk->Rotation = selectedObject_.Get<Mat3>("Orient");
        }
        ObjectEdited(selectedObject_);
    }

    //Object properties
    static std::vector<string> HiddenProperties = //These ones aren't drawn or have special logic
    {
        "Handle", "Num", "ClassnameHash", "RfgPropertyNames", "BlockSize", "ParentHandle", "SiblingHandle", "ChildHandle",
        "NumProps", "PropBlockSize", "Type", "Parent", "Child", "Sibling", "Children", "Zone", "Bmin", "Bmax", "BBmin", "BBmax",
        "Position", "Orient", "Name", "Persistent", "Flags", "ChunkMesh"
    };
    string propertyToRemove = "";
    for (PropertyHandle prop : selectedObject_.Properties())
    {
        if (std::ranges::find(HiddenProperties, prop.Name()) != HiddenProperties.end())
            continue; //Skip hidden properties

        bool supportedProperty = true;
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
            gui::TooltipOnPrevious("int32");
        }
        else if (prop.IsType<u64>())
        {
            u64 data = prop.Get<u64>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U64, &data))
            {
                prop.Set<u64>(data);
                propertyEdited = true;
            }
            gui::TooltipOnPrevious("uint64");
        }
        else if (prop.IsType<u32>())
        {
            u32 data = prop.Get<u32>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U32, &data))
            {
                prop.Set<u32>(data);
                propertyEdited = true;
            }
            gui::TooltipOnPrevious("uint32");
        }
        else if (prop.IsType<u16>())
        {
            u16 data = prop.Get<u16>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U16, &data))
            {
                prop.Set<u16>(data);
                propertyEdited = true;
            }
            gui::TooltipOnPrevious("uint16");
        }
        else if (prop.IsType<u8>())
        {
            u8 data = prop.Get<u8>();
            if (ImGui::InputScalar(prop.Name().c_str(), ImGuiDataType_U8, &data))
            {
                prop.Set<u8>(data);
                propertyEdited = true;
            }
            gui::TooltipOnPrevious("uint8");
        }
        else if (prop.IsType<f32>())
        {
            f32 data = prop.Get<f32>();
            if (ImGui::InputFloat(prop.Name().c_str(), &data))
            {
                prop.Set<f32>(data);
                propertyEdited = true;
            }
            gui::TooltipOnPrevious("float");
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
            supportedProperty = false;
            ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
        }
        if (supportedProperty)
        {
            ImGui::SameLine();
            if (ImGui::Button("X"))//ICON_VS_CHROME_CLOSE))
            {
                int messageBoxResult = ShowMessageBox(fmt::format("Are you sure you want to delete the property '{}'", prop.Name()), "Delete property?", MB_YESNO, MB_ICONWARNING);
                if (messageBoxResult == IDYES)
                {
                    propertyToRemove = prop.Name(); //Actually removed outside of the loop to avoid iterator invalidation
                }
            }
        }
        ImGui::PopID();

        if (propertyEdited)
        {
            updateDebugDraw_ = true; //Update in case value related to debug draw is changed
            ObjectEdited(selectedObject_); //Mark file as edited. Puts * on tab + lets you save changes with Ctrl + S
        }
    }

    if (propertyToRemove != "")
    {
        selectedObject_.Remove(propertyToRemove);
        ObjectEdited(selectedObject_);
    }

    //Add property button
    ImGui::Separator();
    state->FontManager->FontMedium.Push();
    ImGui::Text("Add property:");
    //ImGui::Separator();
    state->FontManager->FontMedium.Pop();

    static std::vector<string> types =
    {
        "string", "bool", "float", "uint", "vec3", "matrix33" //Only types used by MP properties. There's a few other used by SP but it'll need other changes to support
    };

    ImGui::InputText("Name", &_inspectorNewPropName);
    if (ImGui::BeginCombo("Type", _inspectorNewPropType.c_str()))
    {
        for (string& type : types)
        {
            bool selected = (type == _inspectorNewPropType);
            if (ImGui::Selectable(type.c_str(), selected))
            {
                _inspectorNewPropType = type;
            }
        }

        ImGui::EndCombo();
    }
    if (ImGui::Button("Add"))
    {
        if (String::TrimWhitespace(_inspectorNewPropName) == "")
        {
            ShowMessageBox("You must enter a name.", "Name not set", MB_OK, MB_ICONWARNING);
            return;
        }
        if (String::TrimWhitespace(_inspectorNewPropType) == "")
        {
            ShowMessageBox("You must choose a type.", "Name not set", MB_OK, MB_ICONWARNING);
            return;
        }

        //Ensure property name isn't already in use
        bool alreadyExists = false;
        for (PropertyHandle prop : selectedObject_.Properties())
        {
            if (prop.Name() == _inspectorNewPropName)
            {
                ShowMessageBox(fmt::format("This object already has a property named '{}'", _inspectorNewPropName), "Property already exists", MB_OK);
                return;
            }
        }

        PropertyHandle newProp = selectedObject_.Property(_inspectorNewPropName);
        if (_inspectorNewPropType == "string")
        {
            newProp.Set<string>("");
        }
        else if (_inspectorNewPropType == "bool")
        {
            newProp.Set<bool>(false);
        }
        else if (_inspectorNewPropType == "float")
        {
            newProp.Set<f32>(0.0f);
        }
        else if (_inspectorNewPropType == "uint")
        {
            newProp.Set<u32>(0);
        }
        else if (_inspectorNewPropType == "vec3")
        {
            newProp.Set<Vec3>({});
        }
        else if (_inspectorNewPropType == "matrix33")
        {
            newProp.Set<Mat3>({});
        }

        //Add property to RfgPropertyNames list if it isn't already in there
        if (std::ranges::count_if(selectedObject_.GetStringList("RfgPropertyNames"), [&](const string& str) { return str == _inspectorNewPropName; }) == 0)
        {
            selectedObject_.AppendStringList("RfgPropertyNames", _inspectorNewPropName);
        }
        else
        {
            Log->warn("Didn't add {} to RfgPropertyNames. Already in the list.", _inspectorNewPropName);
        }
        ObjectEdited(selectedObject_);
        _inspectorNewPropName = ""; //Reset afterwards
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
    gui::TooltipOnPrevious("ObjectHandle");
    return false; //Editing not yet supported by this type so it always returns false
}

bool TerritoryDocument::Inspector_DrawObjectHandleListEditor(PropertyHandle prop)
{
    const f32 indent = 15.0f;
    ImGui::Text(prop.Name() + ":");
    gui::TooltipOnPrevious("ObjectHandle list");
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
    gui::TooltipOnPrevious("vec3");

    return edited;
}

bool TerritoryDocument::Inspector_DrawMat3Editor(PropertyHandle prop)
{
    bool edited = false;
    ImGui::Text(prop.Name() + ":");
    gui::TooltipOnPrevious("matrix33");
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
    gui::TooltipOnPrevious("string");
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
    gui::TooltipOnPrevious("bool");
    
    return edited;
}

void TerritoryDocument::Inspector_DrawRelativeEditor()
{
    //ImGui::Indent(15.0f);
    ObjectHandle zone = selectedObject_.Get<ObjectHandle>("Zone");

    //Parent combo selector
    {
        ObjectHandle currentParent = selectedObject_.Get<ObjectHandle>("Parent");
        string currentParentName = currentParent ? GetObjectHandleName(currentParent) : "Not set";
        if (ImGui::BeginCombo("Parent", currentParentName.c_str()))
        {
            //Search bar
            ImGui::InputText("Search", &_inspectorParentSelectorSearch);
            ImGui::SameLine();
            if (ImGui::Button(ICON_VS_CHROME_CLOSE))
            {
                _inspectorParentSelectorSearch = "";
            }
            ImGui::Separator();

            //Options
            for (ObjectHandle newParent : zone.GetObjectList("Objects"))
            {
                if (newParent == selectedObject_)
                    continue; //Can't be your own parent
                if (!newParent || (newParent && newParent.Has("Deleted") && newParent.Get<bool>("Deleted")))
                    continue; //Parent must be alive

                string name = GetObjectHandleName(newParent);
                if (_inspectorParentSelectorSearch != "" && !String::Contains(String::ToLower(name), String::ToLower(_inspectorParentSelectorSearch)))
                    continue;

                bool selected = (newParent == currentParent);
                if (ImGui::Selectable(name.c_str(), selected))
                {
                    SetObjectParent(selectedObject_, currentParent, newParent); //Set new parent
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            selectedObject_.Set<ObjectHandle>("Parent", NullObjectHandle); //Make object an orphan
            SetObjectParent(selectedObject_, currentParent, NullObjectHandle);
        }
    }

    //ImGui::Unindent(15.0f);
}

void TerritoryDocument::ExportTask(GuiState* state, MapExportType exportType)
{
    bool exportResult = false;

    string mapName = Territory.Object.Get<string>("Name");
    bool isSPMap = mapName == "zonescript_terr01.vpp_pc" || mapName == "zonescript_dlc01.vpp_pc";
    if (isSPMap)
    {
        if (exportType == MapExportType::Vpp)
        {
            exportResult = ExportMapSP(state, CVar_MapExportPath.Get<string>());
        }
        else if (exportType == MapExportType::BinaryPatch)
        {
            Log->error("Failed to export {}. Binary patch export isn't supported for SP maps.", mapName);
            exportResult = false;
        }
    }
    else
    {
        if (exportType == MapExportType::Vpp)
        {
            exportResult = ExportMap(state, CVar_MapExportPath.Get<string>());
        }
        else if (exportType == MapExportType::BinaryPatch)
        {
            exportResult = ExportBinaryPatch(state);
        }
    }

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
bool TerritoryDocument::ExportMap(GuiState* state, const string& exportPath)
{
    string terrPackfileName = Territory.Object.Get<string>("Name");
    string mapName = Path::GetFileNameNoExtension(Territory.Object.Get<string>("Name"));

    //Steps: Export zones, extract vpp_pc, extract containers, copy zones to containers, repack containers, update asm_pc, repack vpp_pc, cleanup
    const f32 numSteps = 9.0f;
    const f32 percentagePerStep = 1.0f / numSteps;

    exportPercentage_ = 0.0f;
    exportStatus_ = "Exporting zones...";

    //Make sure folder exists for writing temporary files
    string tempFolderPath = state->CurrentProject->Path + "\\Temp\\";
    std::filesystem::create_directories(tempFolderPath);

    //Export zone files
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
    exportStatus_ = "Exporting editor data...";

    //Write additional editor data that isn't preserved in zone files
    Exporters::ExportEditorMapData(Territory.Object, tempFolderPath + "vpp\\");

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Repacking " + terrPackfileName + "...";

    //Repack main vpp_pc
    Packfile3::Pack(tempFolderPath + "\\vpp\\", tempFolderPath + "\\" + terrPackfileName, false, false);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Copying vpp_pc to export folder...";

    //Copy vpp_pc to output path + delete temporary files
    std::filesystem::copy_file(tempFolderPath + "\\" + terrPackfileName, exportPath + "\\" + terrPackfileName, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove_all(tempFolderPath);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Done!";
    return true;
}

bool TerritoryDocument::ExportBinaryPatch(GuiState* state)
{
    //Make sure folder exists for writing temporary files
    string mapName = Path::GetFileNameNoExtension(Territory.Object.Get<string>("Name"));
    string tempFolderPath = state->CurrentProject->Path + "\\PatchTemp\\";
    std::filesystem::create_directories(tempFolderPath);

    //Generate whole vpp_pc
    ExportMap(state, tempFolderPath);

    //Get original vpp_pc
    exportStatus_ = "Getting original map vpp_pc...";
    if (std::filesystem::exists(originalVppPath_))
    {
        std::filesystem::copy_file(originalVppPath_, tempFolderPath + "\\original.vpp_pc", std::filesystem::copy_options::overwrite_existing);
    }
    else
    {
        exportStatus_ = "Couldn't find original map vpp_pc. Make sure path is correct.";
        return false;
    }

    //Generate binary patch with xdelta
    exportStatus_ = "Generating binary patch...";
    string xdeltaPath = BuildConfig::AssetFolderPath + "xdelta3-3.1.0-x86_64.exe";
    string args = fmt::format(" -e -S -s \"{0}\\original.vpp_pc\" \"{0}\\{1}.vpp_pc\" \"{0}\\mapPatch.xdelta\"", tempFolderPath, mapName);
    string moddedVppPath = tempFolderPath + Territory.Object.Get<string>("Name");
    string originalVppPath = tempFolderPath + "original.vpp_pc";
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;
    if (CreateProcessA(xdeltaPath.c_str(), (char*)args.c_str(), nullptr, nullptr, false, 0, nullptr, nullptr, &startupInfo, &processInfo))
    {
        WaitForSingleObject(processInfo.hProcess, INFINITE); //Wait for xdelta to finish
    }
    else
    {
        exportStatus_ = "Failed to run xdelta.exe. Last error code: " + std::to_string(GetLastError());
    }
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    //Copy patch to output folder
    exportStatus_ = "Copying patch to output folder & cleaning up...";
    string patchTempPath = fmt::format("{}\\mapPatch.xdelta", tempFolderPath);
    string patchOutputPath = fmt::format("{}\\{}.xdelta", CVar_MapExportPath.Get<string>(), mapName);
    std::filesystem::copy_file(patchTempPath, patchOutputPath, std::filesystem::copy_options::overwrite_existing);

    //Delete temp folder
    std::filesystem::remove_all(tempFolderPath);

    return true;
}

struct ZoneSearchResult
{
    FileHandle File;
    ObjectHandle Object;

    ZoneSearchResult(FileHandle fileHandle, ObjectHandle obj)
    {
        File = fileHandle;
        Object = obj;
    }
};
bool UpdateSpPackfileZones(PackfileVFS* packfileVFS, std::vector<ZoneSearchResult>& editedZones, const string& packfileName, const string& tempFolderPath, bool compressed = false, bool condensed = false);

bool TerritoryDocument::ExportMapSP(GuiState* state, const string& exportPath)
{
    string terrPrefix = "";
    string terrPackfileName = Territory.Object.Get<string>("Name");
    string mapName = "";
    if (terrPackfileName == "zonescript_terr01.vpp_pc")
        mapName = "terr01";
    else if (terrPackfileName == "zonescript_dlc01.vpp_pc")
        mapName = "dlc01";
    else
    {
        exportStatus_ = "Failed to export map! '{}' not recognized as an SP map.";
        LOG_ERROR(exportStatus_);
        return false;
    }

    //Steps: Export zones, extract vpp_pc, extract containers, copy zones to containers, repack containers, update asm_pc, repack vpp_pc, cleanup
    const f32 numSteps = 8.0f;
    const f32 percentagePerStep = 1.0f / numSteps;

    exportPercentage_ = 0.0f;
    exportStatus_ = "Exporting zones...";

    //Make sure folder exists for writing temporary files
    string tempFolderPath = state->CurrentProject->Path + "\\Temp\\";
    string repackFolderPath = state->CurrentProject->Path + "\\Temp\\vpp\\";
    std::filesystem::create_directories(tempFolderPath);
    std::filesystem::create_directories(repackFolderPath);

    //Export zone files
    if (!Exporters::ExportTerritory(Territory.Object, tempFolderPath, true))
    {
        exportStatus_ = fmt::format("Failed to export game files for territory '{}'", mapName);
        LOG_ERROR(exportStatus_);
        return false;
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Locating zone files...";

    //Locate edited rfgzone_pc files in terr01_l0 and terr01_l1
    std::vector<ZoneSearchResult> editedZoneLocations = {};
    bool anyZoneInTerr01L0 = false;
    bool anyZoneInTerr01L1 = false;
    for (ObjectHandle zone : Territory.Object.GetObjectList("Zones"))
    {
        //Search terr01_l0.vpp_pc first
        std::vector<FileHandle> l0SearchResult = state->PackfileVFS->GetFiles(fmt::format("{}_l0.vpp_pc", mapName), zone.Get<string>("Name"), true, true);
        if (l0SearchResult.size() == 1)
        {
            editedZoneLocations.push_back({ l0SearchResult[0], zone });
            anyZoneInTerr01L0 = true;
            continue;
        }

        //Then search terr01_l1.vpp_pc
        std::vector<FileHandle> l1SearchResult = state->PackfileVFS->GetFiles(fmt::format("{}_l1.vpp_pc", mapName), zone.Get<string>("Name"), true, true);
        if (l1SearchResult.size() == 1)
        {
            editedZoneLocations.push_back({ l1SearchResult[0], zone });
            anyZoneInTerr01L1 = true;
            continue;
        }
        
        //Didn't find the zone file. Cancel export
        exportStatus_ = fmt::format("Map export failed! Couldn't find {}.", zone.Get<string>("Name"));
        LOG_ERROR(exportStatus_);
        return false;
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = fmt::format("Updating {}_l0.vpp_pc zones...", mapName);

    if (anyZoneInTerr01L0)
    {
        if (!UpdateSpPackfileZones(state->PackfileVFS, editedZoneLocations, fmt::format("{}_l0.vpp_pc", mapName), tempFolderPath, false, false))
        {
            exportStatus_ = fmt::format("Map export failed! Error while updating zone data in {}_l0.vpp_pc.", mapName);
            LOG_ERROR(exportStatus_);
            return false;
        }
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = fmt::format("Updating {}_l1.vpp_pc zones...", mapName);
    if (anyZoneInTerr01L1)
    {
        if (!UpdateSpPackfileZones(state->PackfileVFS, editedZoneLocations, fmt::format("{}_l1.vpp_pc", mapName), tempFolderPath, false, false))
        {
            exportStatus_ = fmt::format("Map export failed! Error while updating zone data in {}_l1.vpp_pc.", mapName);
            LOG_ERROR(exportStatus_);
            return false;
        }
    }

    //Extract vpp_pc
    exportPercentage_ += percentagePerStep;
    exportStatus_ = fmt::format("Updating zonescript_{}.vpp_pc zones...", mapName);
    Handle<Packfile3> vppHandle = state->PackfileVFS->GetPackfile(terrPackfileName);
    if (!vppHandle)
    {
        LOG_ERROR("SP map export failed to find {}. Make sure it's in your data folder and restart Nanoforge.", terrPackfileName);
        return false;
    }
    //Reparse packfile from games data folder. Simplest way to do this for the moment until PackfileVFS is updated to be able to handle reloading packfiles safely
    //Can't use data in PackfileVFS since the packfile contents would've changed if the user does multiple map exports in a row
    string packfilePath = vppHandle->Path;
    Handle<Packfile3> currVpp = CreateHandle<Packfile3>(packfilePath);
    currVpp->ReadMetadata();
    currVpp->ExtractSubfiles(repackFolderPath, false);

    //Copy zones into repack folder
    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Copying zones to " + terrPackfileName + "...";
    for (auto entry : std::filesystem::directory_iterator(tempFolderPath))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".rfgzone_pc")
            continue; //Skip anything that isn't a .rfgzone_pc file

        string destinationPath = repackFolderPath + entry.path().filename().string();
        std::filesystem::copy_file(entry.path(), destinationPath, std::filesystem::copy_options::overwrite_existing);
    }

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Repacking " + terrPackfileName + "...";

    //Repack zonscript_xxxx.vpp_pc
    Packfile3::Pack(repackFolderPath, tempFolderPath + "\\" + terrPackfileName, true, false);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Copying map vpp_pc files to export folder...";

    //Copy edited vpp files to output path
    std::filesystem::copy_file(tempFolderPath + "\\" + terrPackfileName, exportPath + "\\" + terrPackfileName, std::filesystem::copy_options::overwrite_existing);
    if (anyZoneInTerr01L0)
        std::filesystem::copy_file(tempFolderPath + "\\" + mapName + "_l0.vpp_pc", exportPath + "\\" + mapName + "_l0.vpp_pc", std::filesystem::copy_options::overwrite_existing);
    if (anyZoneInTerr01L1)
        std::filesystem::copy_file(tempFolderPath + "\\" + mapName + "_l1.vpp_pc", exportPath + "\\" + mapName + "_l1.vpp_pc", std::filesystem::copy_options::overwrite_existing);

    //Delete temporary files
    std::filesystem::remove_all(tempFolderPath);

    exportPercentage_ += percentagePerStep;
    exportStatus_ = "Done!";
    return true;
}

bool UpdateSpPackfileZones(PackfileVFS* packfileVFS, std::vector<ZoneSearchResult>& editedZones, const string& packfileName, const string& tempFolderPath, bool compressed, bool condensed)
{
    //Extract vpp_pc. Reparsed instead of using the copy from PackfileVFS since it doesn't auto update them after the file changes.
    Handle<Packfile3> currVpp = CreateHandle<Packfile3>(packfileVFS->DataFolderPath() + "\\" + packfileName);
    string extractFolder = tempFolderPath + Path::GetFileNameNoExtension(packfileName) + "\\";
    currVpp->ReadMetadata();
    currVpp->ExtractSubfiles(extractFolder, false);
    
    //Copy new zone files to the str2_pc file that contains them
    std::filesystem::create_directories(extractFolder + "Unpack\\");
    for (ZoneSearchResult& result : editedZones)
    {
        if (result.File.GetPackfile()->Name() != packfileName)
            continue; //Zone not in this packfile

        //Extract str2_pc file
        string str2Name = result.File.ContainerName();
        string str2OutFolder = extractFolder + "Unpack\\" + str2Name + "\\";
        Packfile3 packfile = { extractFolder + str2Name };
        packfile.ReadMetadata();
        packfile.ExtractSubfiles();
        std::filesystem::create_directories(str2OutFolder);
        packfile.ExtractSubfiles(str2OutFolder, true);

        //Copy zone to str2 folder
        std::filesystem::copy_file(tempFolderPath + result.Object.Get<string>("Name"), str2OutFolder + "\\" + result.Object.Get<string>("Name"), std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file(tempFolderPath + "p_" + result.Object.Get<string>("Name"), str2OutFolder + "\\p_" + result.Object.Get<string>("Name"), std::filesystem::copy_options::overwrite_existing);

        //Repack str2_pc file
        Packfile3::Pack(str2OutFolder, extractFolder + str2Name, true, true);
    }

    //Update asm_pc files
    //All the vanilla SP maps have a single asm_pc with the same name as the packfile. Writing it this way just in case a modder decides to add another with AsmTool
    for (auto& entry : std::filesystem::directory_iterator(extractFolder))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".asm_pc")
            continue;

        UpdateAsmPc(entry.path().string());
    }

    //Repack vpp_pc
    Packfile3::Pack(extractFolder, tempFolderPath + "\\" + packfileName, false, false);
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

        //When checked the zone being moused over in the outliner has a solid box draw on it in the editor. Also toggleable with the F key
        ImGui::Checkbox("Highlight zones", &_highlightHoveredZone);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::InputFloat("Height", &_zoneBoxHeight);
        ImGui::SameLine();
        gui::HelpMarker("Draw a solid box over zones when they're moused over in the outliner. Can also be toggled with the F key", ImGui::GetDefaultFont());

        //When checked draw solid bbox over moused over objects
        ImGui::Checkbox("Highlight objects", &_highlightHoveredObject);
        ImGui::SameLine();
        gui::HelpMarker("Draw a solid box over objects when they're moused over in the outliner. Can also be toggled with the G key", ImGui::GetDefaultFont());

        if (ImGui::Checkbox("Hide bounding boxes", &CVar_HideBoundingBoxes.Get<bool>()))
        {
            Config::Get()->Save();
        }

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

void TerritoryDocument::Outliner_DrawObjectNode(GuiState* state, ObjectHandle object, u32 depth)
{
    if (depth > MAX_OUTLINER_DEPTH)
    {
        ImGui::Text("Can't draw object! Hit tree depth limit!");
        return;
    }

    if (object.Has("Deleted") && object.Get<bool>("Deleted"))
        return;

    //Don't show node if it and it's children object types are being hidden
    bool showObjectOrChildren = Outliner_ShowObjectOrChildren(object);
    bool visible = Outliner_ShowObjectOrChildren(object) || object == _openNodeNextFrame;
    if (!visible)
        return;

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

    bool hasChildren = object.GetObjectList("Children").size() != 0;
    bool hasParent = object.Get<ObjectHandle>("Parent").Valid();

    ImGui::PushID(object.UID()); //Push unique ID for the UI element
    if (_openNodeNextFrame == object) //Force open the node if set last frame
    {
        ImGui::SetNextItemOpen(true);
        _openNodeNextFrame = NullObjectHandle;
    }

    //Draw node
    f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    bool nodeOpen = ImGui::TreeNodeEx(objectLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_OpenOnArrow |
        (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf) | (selected ? ImGuiTreeNodeFlags_Selected : 0));

    if (_scrollToObjectInOutliner == object) //Scroll to node if set
    {
        ImGui::ScrollToItem();
        _scrollToObjectInOutliner = NullObjectHandle;
    }
    if (ImGui::IsItemClicked())
    {
        if (object == selectedObject_)
            selectedObject_ = NullObjectHandle; //Clicked selected object again, deselect it
        else
            selectedObject_ = object; //Clicked object, select it
    }
    if (ImGui::BeginPopupContextItem()) //Right click context menu
    {
        //Get list of open map editors for object transfer
        std::vector<Handle<IDocument>>& documents = state->Documents;
        std::vector<Handle<IDocument>> otherMapEditors = {};
        std::ranges::copy_if(documents, std::back_inserter(otherMapEditors), [this](Handle<IDocument> doc) -> bool { return doc->DocumentType == "MapEditor" && doc.get() != this; });
        bool otherMapsOpen = otherMapEditors.size() > 0;

        if (ImGui::Selectable("Clone"))
        {
            _contextMenuShallowCloneObject = object;
        }

        if (ImGui::BeginMenu("Clone to...", otherMapsOpen))
        {
            for (Handle<IDocument> doc : otherMapEditors)
            {
                TerritoryDocument* otherMap = (TerritoryDocument*)doc.get();
                bool ready = otherMap->Territory.Ready();
                string name = doc->Title + (ready ? "" : " - Loading...");
                if (ImGui::MenuItem(name.c_str(), nullptr, nullptr, ready))
                {
                    ObjectHandle newObject = otherMap->ShallowCloneObject(object, true, true);
                    newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::Selectable("Deep clone"))
        {
            _contextMenuDeepCloneObject = object;
        }

        if (ImGui::BeginMenu("Deep clone to...", otherMapsOpen))
        {
            for (Handle<IDocument> doc : otherMapEditors)
            {
                TerritoryDocument* otherMap = (TerritoryDocument*)doc.get();
                bool ready = otherMap->Territory.Ready();
                string name = doc->Title + (ready ? "" : " - Loading...");
                if (ImGui::MenuItem(name.c_str(), nullptr, nullptr, ready))
                {
                    ObjectHandle newObject = otherMap->DeepCloneObject(object, true, 0, true);
                    newObject.Set<ObjectHandle>("Parent", NullObjectHandle);
                }
            }

            ImGui::EndMenu();
        }


        ImGui::Separator();
        if (ImGui::Selectable("Add dummy child"))
        {
            _contextMenuAddDummyChildParent = object;
        }

        if (!hasParent) ImGui::BeginDisabled();
        if (ImGui::Selectable("Orphan"))
        {
            orphanObjectPopupHandle_ = object;
            orphanObjectPopup_.Open();
        }
        if (!hasParent) ImGui::EndDisabled();

        if (!hasChildren) ImGui::BeginDisabled();
        if (ImGui::Selectable("Orphan children"))
        {
            orphanChildrenPopupHandle_ = object;
            orphanChildrenPopup_.Open();
        }
        if (!hasChildren) ImGui::EndDisabled();


        ImGui::Separator();
        if (ImGui::Selectable("Delete"))
        {
            deleteObjectPopupHandle_ = object;
            deleteObjectPopup_.Open();
        }


        ImGui::Separator();
        if (ImGui::Selectable("Copy scriptx reference"))
        {
            CopyScriptxReference(selectedObject_);
        }

        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
    {
        gui::TooltipOnPrevious(object.Property("Type").Get<string>(), ImGui::GetIO().FontDefault);
        _hoveredObject = object;
    }

    ZoneObjectClass* objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());
    Vec3 color = objectClass ? objectClass->Color : Vec3{1.0f, 1.0f, 1.0f};
    const char* labelIcon = objectClass ? objectClass->LabelIcon : ICON_FA_EXCLAMATION_TRIANGLE;

    //Draw node icon
    ImGui::PushStyleColor(ImGuiCol_Text, { color.x, color.y, color.z, 1.0f });
    ImGui::SameLine();
    ImGui::SetCursorPosX(nodeXPos + 22.0f);
    ImGui::Text(labelIcon);
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
                Outliner_DrawObjectNode(state, child, depth + 1);
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
    ZoneObjectClass* objectClass = Territory.GetObjectClass(object.Property("ClassnameHash").Get<u32>());
    if (objectClass && objectClass->Show)
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

void TerritoryDocument::ObjectEdited(ObjectHandle object)
{
    if (!object)
        return;

    UnsavedChanges = true;
    ObjectHandle zone = object.Get<ObjectHandle>("Zone");
    if (!zone)
    {
        //Just in case. Don't need editor crashing on common operators just because this wasn't set
        LOG_ERROR("Object {} doesn't have its zone set. SP exports might not work for this map. Contact the Nanoforge developer.");
        return;
    }

    //This should be true if any child object was edited and never set back to false
    //Used during SP map export so it can selectively export zones
    zone.Set<bool>("ChildObjectEdited", true);
}