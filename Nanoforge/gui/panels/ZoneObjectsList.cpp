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

    if (!state->CurrentTerritory || !state->CurrentTerritory->Ready())
    {
        ImGui::TextWrapped("%s Zone data still loading or no territory has been opened (Tools > Open Territory > Terr01 for main campaign). ", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        //Zone object filters
        if (ImGui::CollapsingHeader(ICON_FA_FILTER " Filters##CollapsingHeader"))
        {
            if (ImGui::Button("Show all types"))
            {
                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    objectClass.Show = true;
                }
                state->CurrentTerritoryUpdateDebugDraw = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Hide all types"))
            {
                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    objectClass.Show = false;
                }
                state->CurrentTerritoryUpdateDebugDraw = true;
            }
            ImGui::Checkbox("Hide distant objects", &onlyShowNearZones_);
            gui::HelpMarker("If checked then objects outside of the viewing range are hidden. The viewing range is configurable through the buttons in the top left corner of the map viewer.", ImGui::GetIO().FontDefault);

            //Draw object filters sub-window
            if (ImGui::BeginChild("##Zone object filters list", ImVec2(0, 200.0f), true))
            {
                ImGui::Text(" " ICON_FA_EYE "     " ICON_FA_BOX);
                gui::TooltipOnPrevious("Toggles whether bounding boxes are drawn for the object class. The second checkbox toggles between wireframe and solid bounding boxes.", nullptr);

                for (auto& objectClass : state->CurrentTerritory->ZoneObjectClasses)
                {
                    if (ImGui::Checkbox((string("##showBB") + objectClass.Name).c_str(), &objectClass.Show))
                        state->CurrentTerritoryUpdateDebugDraw = true;
                    ImGui::SameLine();
                    if (ImGui::Checkbox((string("##solidBB") + objectClass.Name).c_str(), &objectClass.DrawSolid))
                        state->CurrentTerritoryUpdateDebugDraw = true;

                    ImGui::SameLine();
                    ImGui::Text(objectClass.Name);
                    ImGui::SameLine();
                    ImGui::TextColored(gui::SecondaryTextColor, "|  %d objects", objectClass.NumInstances);
                    ImGui::SameLine();

                    //Todo: Use a proper string formatting lib here
                    if (ImGui::ColorEdit3(string("##" + objectClass.Name).c_str(), (f32*)&objectClass.Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
                        state->CurrentTerritoryUpdateDebugDraw = true;
                }
                ImGui::EndChild();
            }
            ImGui::Separator();
        }

        //Zone object table
        if (state->CurrentTerritory->Ready())
        {
            //Set custom highlight colors for the table
            ImVec4 selectedColor = { 0.157f, 0.350f, 0.588f, 1.0f };
            ImVec4 highlightColor = { selectedColor.x * 1.1f, selectedColor.y * 1.1f, selectedColor.z * 1.1f, 1.0f };
            ImGui::PushStyleColor(ImGuiCol_Header, selectedColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, highlightColor);
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, highlightColor);

            //Draw table
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
                for (ObjectHandle zone : state->CurrentTerritory->Zones)
                {
                    if (onlyShowNearZones_ && !zone.Property("RenderBoundingBoxes").Get<bool>())
                        continue;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    //Close zone node if none of it's child objects a visible (based on object type filters)
                    bool anyChildrenVisible = ZoneAnyChildObjectsVisible(state, zone);
                    bool visible = onlyShowNearZones_ ? anyChildrenVisible : true;
                    bool selected = (zone == state->ZoneObjectList_SelectedObject);
                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | (visible ? 0 : ImGuiTreeNodeFlags_Leaf);
                    if (selected)
                        flags |= ImGuiTreeNodeFlags_Selected;

                    if (ImGui::TreeNodeEx(zone.Property("Name").Get<string>().c_str(), flags))
                    {
                        for (ObjectHandle object : zone.Property("Objects").GetObjectList())
                        {
                            //Don't draw objects with parents at the top of the tree. They'll be drawn as subnodes when their parent calls DrawObjectNode()
                            if (!object || object.Property("Parent").Get<ObjectHandle>())
                                continue;

                            DrawObjectNode(state, object);
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::PopStyleColor(3);
                ImGui::EndTable();
            }
        }
        else
        {
            ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Loading zones...");
        }
    }

    ImGui::End();
}

void ZoneObjectsList::DrawObjectNode(GuiState* state, ObjectHandle object)
{
    //Don't show node if it and it's children object types are being hidden
    bool showObjectOrChildren = ShowObjectOrChildren(state, object);
    bool visible = onlyShowNearZones_ ? ShowObjectOrChildren(state, object) : true;
    if (!visible)
        return;

    auto& objectClass = state->CurrentTerritory->GetObjectClass(object.Property("ClassnameHash").Get<u32>());

    //Update node index and selection state
    bool selected = (object == state->ZoneObjectList_SelectedObject);

    //Attempt to find a human friendly name for the object
    string name = object.Property("Name").Get<string>();

    //Determine best object name
    string objectLabel = "      "; //Empty space for node icon
    if (name != "")
        objectLabel += name; //Use custom name if available
    else
        objectLabel += object.Property("Classname").Get<string>(); //Otherwise use object type name (e.g. rfg_mover, navpoint, obj_light, etc)

    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    //Draw node
    ImGui::PushID((u64)&object); //Push unique ID for the UI element
    f32 nodeXPos = ImGui::GetCursorPosX(); //Store position of the node for drawing the node icon later
    bool nodeOpen = ImGui::TreeNodeEx(objectLabel.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth |
        (object.SubObjects().size() == 0 ? ImGuiTreeNodeFlags_Leaf : 0) | (selected ? ImGuiTreeNodeFlags_Selected : 0));
    if (ImGui::IsItemClicked())
        state->ZoneObjectList_SelectedObject = object;

    //Update selection state
    if (ImGui::IsItemClicked())
    {
        state->SetSelectedZoneObject(object);
        state->PropertyPanelContentFuncPtr = &PropertyPanel_ZoneObject;
    }
    if (ImGui::IsItemHovered())
    {
        gui::TooltipOnPrevious(object.Property("Classname").Get<string>(), ImGui::GetIO().FontDefault);
    }

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
        for (ObjectHandle child : object.SubObjects())
            if (child)
                DrawObjectNode(state, child);
        ImGui::TreePop();
    }
    ImGui::PopID();
}

bool ZoneObjectsList::ZoneAnyChildObjectsVisible(GuiState* state, ObjectHandle zone)
{
    for (ObjectHandle object : zone.Property("Objects").GetObjectList())
        if (ShowObjectOrChildren(state, object))
            return true;

    return false;
}

bool ZoneObjectsList::ShowObjectOrChildren(GuiState* state, ObjectHandle object)
{
    auto& objectClass = state->CurrentTerritory->GetObjectClass(object.Property("ClassnameHash").Get<u32>());
    if (objectClass.Show)
        return true;

    Registry& registry = Registry::Get();
    for (ObjectHandle child : object.SubObjects())
        if (child.Valid() && ShowObjectOrChildren(state, child))
            return true;

    return false;
}
