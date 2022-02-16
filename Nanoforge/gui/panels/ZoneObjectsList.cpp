#include "ZoneObjectsList.h"
#include "render/imgui/ImGuiConfig.h"
#include "property_panel/PropertyPanelContent.h"
#include "render/imgui/imgui_ext.h"
#include "util/Profiler.h"
#include "gui/GuiState.h"
#include "rfg/Territory.h"
#include "render/imgui/ImGuiFontManager.h"

ZoneObjectsList::ZoneObjectsList()
{

}

ZoneObjectsList::~ZoneObjectsList()
{

}

void ZoneObjectsList::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin(ICON_FA_BOXES " Zone objects", open))
    {
        ImGui::End();
        return;
    }

    //TODO: Re-implement
    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Object list needs to be re-implemented for the editor format!");

    //if (!state->CurrentTerritory || !state->CurrentTerritory->Ready())
    //{
    //    ImGui::TextWrapped("%s Zone data still loading or no territory has been opened (Tools > Open Territory > Terr01 for main campaign). ", ICON_FA_EXCLAMATION_CIRCLE);
    //}
    //else
    //{
    //    //Zone object filters
    //    if (ImGui::CollapsingHeader(ICON_FA_FILTER " Filters##CollapsingHeader"))
    //    {
    //        if (ImGui::Button("Show all types"))
    //        {
    //            for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
    //            {
    //                objectClass.Show = true;
    //            }
    //            state->CurrentTerritoryUpdateDebugDraw = true;
    //        }
    //        ImGui::SameLine();
    //        if (ImGui::Button("Hide all types"))
    //        {
    //            for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
    //            {
    //                objectClass.Show = false;
    //            }
    //            state->CurrentTerritoryUpdateDebugDraw = true;
    //        }
    //        ImGui::Checkbox("Hide distant objects", &onlyShowNearZones_);
    //        gui::HelpMarker("If checked then objects outside of the viewing range are hidden. The viewing range is configurable through the buttons in the top left corner of the map viewer.", ImGui::GetIO().FontDefault);

    //        //Draw object filters sub-window
    //        if (ImGui::BeginChild("##Zone object filters list", ImVec2(0, 200.0f), true))
    //        {
    //            ImGui::Text(" " ICON_FA_EYE "     " ICON_FA_BOX);
    //            gui::TooltipOnPrevious("Toggles whether bounding boxes are drawn for the object class. The second checkbox toggles between wireframe and solid bounding boxes.", nullptr);

    //            for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
    //            {
    //                if (ImGui::Checkbox((string("##showBB") + objectClass.Name).c_str(), &objectClass.Show))
    //                    state->CurrentTerritoryUpdateDebugDraw = true;
    //                ImGui::SameLine();
    //                if (ImGui::Checkbox((string("##solidBB") + objectClass.Name).c_str(), &objectClass.DrawSolid))
    //                    state->CurrentTerritoryUpdateDebugDraw = true;

    //                ImGui::SameLine();
    //                ImGui::Text(objectClass.Name);
    //                ImGui::SameLine();
    //                ImGui::TextColored(gui::SecondaryTextColor, "|  %d objects", objectClass.NumInstances);
    //                ImGui::SameLine();

    //                //Todo: Use a proper string formatting lib here
    //                if (ImGui::ColorEdit3(string("##" + objectClass.Name).c_str(), (f32*)&objectClass.Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
    //                    state->CurrentTerritoryUpdateDebugDraw = true;
    //            }
    //            ImGui::EndChild();
    //        }
    //        ImGui::Separator();
    //    }

    //    //Zone object table
    //    if (state->CurrentTerritory->Ready())
    //    {
    //        //Set custom highlight colors for the table
    //        ImVec4 selectedColor = { 0.157f, 0.350f, 0.588f, 1.0f };
    //        ImVec4 highlightColor = { selectedColor.x * 1.1f, selectedColor.y * 1.1f, selectedColor.z * 1.1f, 1.0f };
    //        ImGui::PushStyleColor(ImGuiCol_Header, selectedColor);
    //        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, highlightColor);
    //        ImGui::PushStyleColor(ImGuiCol_HeaderActive, highlightColor);

    //        //Draw table
    //        ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter |
    //            ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
    //            ImGuiTableFlags_Hideable | ImGuiTableFlags_SizingFixedFit;
    //        if (ImGui::BeginTable("ZoneObjectTable", 4, flags))
    //        {
    //            ImGui::TableSetupScrollFreeze(0, 1);
    //            ImGui::TableSetupColumn("Name");
    //            ImGui::TableSetupColumn("Flags");
    //            ImGui::TableSetupColumn("Num");
    //            ImGui::TableSetupColumn("Handle");
    //            ImGui::TableHeadersRow();

    //            //Loop through visible zones
    //            for (auto& zone : state->CurrentTerritory->ZoneFiles)
    //            {
    //                if (onlyShowNearZones_ && !zone.RenderBoundingBoxes)
    //                    continue;

    //                ImGui::TableNextRow();
    //                ImGui::TableNextColumn();

    //                //Close zone node if none of it's child objects a visible (based on object type filters)
    //                bool anyChildrenVisible = ZoneAnyChildObjectsVisible(state, zone);
    //                bool visible = onlyShowNearZones_ ? anyChildrenVisible : true;
    //                if (ImGui::TreeNodeEx(zone.Name.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | (visible ? 0 : ImGuiTreeNodeFlags_Leaf)))
    //                {
    //                    for (auto& object : zone.Zone.ObjectsHierarchical)
    //                    {
    //                        DrawObjectNode(state, object);
    //                    }
    //                    ImGui::TreePop();
    //                }

    //            }

    //            ImGui::PopStyleColor(3);
    //            ImGui::EndTable();
    //        }
    //    }
    //    else
    //    {
    //        ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Loading zones...");
    //    }
    //}

    ImGui::End();
}

void ZoneObjectsList::DrawObjectNode(GuiState* state, ObjectHandle object)
{
    //TODO: Re-implement
    return;

    ////Don't show node if it and it's children object types are being hidden
    //bool showObjectOrChildren = ShowObjectOrChildren(state, object);
    //bool visible = onlyShowNearZones_ ? ShowObjectOrChildren(state, object) : true;
    //if (!visible)
    //    return;

    //auto& objectClass = state->CurrentTerritory->GetObjectClass(object.Self->ClassnameHash);

    ////Update node index and selection state
    //object.Selected = &object == state->SelectedObject;

    ////Attempt to find a human friendly name for the object
    //string name = "";
    //auto* displayName = object.Self->GetProperty<StringProperty>("display_name");
    //auto* chunkName = object.Self->GetProperty<StringProperty>("chunk_name");
    //auto* animationType = object.Self->GetProperty<StringProperty>("animation_type");
    //auto* activityType = object.Self->GetProperty<StringProperty>("activity_type");
    //auto* raidType = object.Self->GetProperty<StringProperty>("raid_type");
    //auto* courierType = object.Self->GetProperty<StringProperty>("courier_type");
    //auto* spawnSet = object.Self->GetProperty<StringProperty>("spawn_set");
    //auto* itemType = object.Self->GetProperty<StringProperty>("item_type");
    //auto* dummyType = object.Self->GetProperty<StringProperty>("dummy_type");
    //auto* weaponType = object.Self->GetProperty<StringProperty>("weapon_type");
    //auto* regionKillType = object.Self->GetProperty<StringProperty>("region_kill_type");
    //auto* deliveryType = object.Self->GetProperty<StringProperty>("delivery_type");
    //auto* squadDef = object.Self->GetProperty<StringProperty>("squad_def");
    //auto* missionInfo = object.Self->GetProperty<StringProperty>("mission_info");
    //if (displayName)
    //    name = displayName->Data;
    //else if (chunkName)
    //    name = chunkName->Data;
    //else if (animationType)
    //    name = animationType->Data;
    //else if (activityType)
    //    name = activityType->Data;
    //else if (raidType)
    //    name = raidType->Data;
    //else if (courierType)
    //    name = courierType->Data;
    //else if (spawnSet)
    //    name = spawnSet->Data;
    //else if (itemType)
    //    name = itemType->Data;
    //else if (dummyType)
    //    name = dummyType->Data;
    //else if (weaponType)
    //    name = weaponType->Data;
    //else if (regionKillType)
    //    name = regionKillType->Data;
    //else if (deliveryType)
    //    name = deliveryType->Data;
    //else if (squadDef)
    //    name = squadDef->Data;
    //else if (missionInfo)
    //    name = missionInfo->Data;

    ////Store selected object in global gui state for debug draw in TerritoryDocument.cpp, ~line 290
    //if (object.Selected)
    //    state->ZoneObjectList_SelectedObject = object.Self;

    ////Determine best object name
    //string objectLabel = "      "; //Empty space for node icon
    //if (name != "")
    //    objectLabel += name; //Use custom name if available
    //else
    //    objectLabel += object.Self->Classname; //Otherwise use object type name (e.g. rfg_mover, navpoint, obj_light, etc)

    //ImGui::TableNextRow();
    //ImGui::TableNextColumn();

    ////Draw node
    //ImGui::PushID((u64)&object); //Push unique ID for the UI element
    //f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    //bool nodeOpen = ImGui::TreeNodeEx(objectLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth |
    //    (object.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : 0) | (object.Selected ? ImGuiTreeNodeFlags_Selected : 0));

    ////Update selection state
    //if (ImGui::IsItemClicked())
    //{
    //    state->SetSelectedZoneObject(&object);
    //    state->PropertyPanelContentFuncPtr = &PropertyPanel_ZoneObject;
    //}

    ////Draw node icon
    //ImGui::PushStyleColor(ImGuiCol_Text, { objectClass.Color.x, objectClass.Color.y, objectClass.Color.z, 1.0f });
    //ImGui::SameLine();
    //ImGui::SetCursorPosX(nodeXPos + 22.0f);
    //ImGui::Text(objectClass.LabelIcon);
    //ImGui::PopStyleColor();

    ////Flags
    //ImGui::TableNextColumn();
    //ImGui::Text(std::to_string(object.Self->Flags));

    ////Num
    //ImGui::TableNextColumn();
    //ImGui::Text(std::to_string(object.Self->Num));

    ////Handle
    //ImGui::TableNextColumn();
    //ImGui::Text(std::to_string(object.Self->Handle));

    ////Draw child nodes
    //if (nodeOpen)
    //{
    //    for (auto& childObject : object.Children)
    //    {
    //        DrawObjectNode(state, childObject);
    //    }
    //    ImGui::TreePop();
    //}
    //ImGui::PopID();
}

bool ZoneObjectsList::ZoneAnyChildObjectsVisible(GuiState* state, ObjectHandle zone)
{
    //TODO: Re-implement
    return false;

    //for (auto& object : zone.Zone.ObjectsHierarchical)
    //{
    //    if (ShowObjectOrChildren(state, object))
    //        return true;
    //}
    //return false;
}

bool ZoneObjectsList::ShowObjectOrChildren(GuiState* state, ObjectHandle object)
{
    //TODO: Re-implement
    return false;

    //auto& objectClass = state->CurrentTerritory->GetObjectClass(object.Self->ClassnameHash);
    //if (objectClass.Show)
    //    return true;

    //for (auto& child : object.Children)
    //{
    //    if (ShowObjectOrChildren(state, child))
    //        return true;
    //}

    //return false;
}
