#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"

//Todo: May want to move this to be under each zone in the zone window list since we'll have a third panel for per-object properties
void ZoneObjectsList_Update(GuiState* state, bool* open)
{
    if (!ImGui::Begin("Zone objects", open))
    {
        ImGui::End();
        return;
    }

    if (state->SelectedZone == InvalidZoneIndex || state->SelectedZone >= state->ZoneManager->ZoneFiles.size())
    {
        ImGui::Text("%s Select a zone to see the objects it contains.", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        if (ImGui::CollapsingHeader(ICON_FA_FILTER " Filters##CollapsingHeader"))
        {
            //Draw object filters sub-window
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.134f, 0.160f, 0.196f, 1.0f));
            ImGui::BeginChild("##Zone object filters list", ImVec2(0, 200.0f), true);

            ImGui::Text(" " ICON_FA_EYE);
            gui::TooltipOnPrevious("Toggles whether bounding boxes are drawn for the object class", nullptr);
            ImGui::SameLine();
            ImGui::Text(" " ICON_FA_MARKER);
            gui::TooltipOnPrevious("Toggles whether class name labels are drawn for the object class", nullptr);

            for (auto& objectClass : state->ZoneManager->ZoneObjectClasses)
            {
                ImGui::Checkbox((string("##showBB") + objectClass.Name).c_str(), &objectClass.Show);
                ImGui::SameLine();
                ImGui::Checkbox((string("##showLabel") + objectClass.Name).c_str(), &objectClass.ShowLabel);
                ImGui::SameLine();
                ImGui::Text(objectClass.Name);
                ImGui::SameLine();
                ImGui::TextColored(gui::SecondaryTextColor, "|  %d objects", objectClass.NumInstances);
                ImGui::SameLine();
                //Todo: Use a proper string formatting lib here
                ImGui::ColorEdit3(string("##" + objectClass.Name).c_str(), (f32*)&objectClass.Color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                //ImGui::Text("|  %d instances", objectClass.NumInstances);
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        //Draw zone objects list
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_BOXES " Zone objects");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //Object list
        auto& zone = state->ZoneManager->ZoneFiles[state->SelectedZone].Zone;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.134f, 0.160f, 0.196f, 1.0f));
        ImGui::BeginChild("##Zone object list", ImVec2(0, 0), true);

        u32 index = 0;
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.

        int node_clicked = -1;
        for (auto& object : zone.ObjectsHierarchical)
        {
            auto objectClass = state->ZoneManager->GetObjectClass(object.Self->ClassnameHash);
            if (!objectClass.Show)
                continue;

            //Todo: Use a formatting lib/func here. This is bad
            if (ImGui::TreeNodeEx((string(objectClass.LabelIcon) + object.Self->Classname + "##" + std::to_string(index)).c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | 
                (object.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None) | (object.Selected ? ImGuiTreeNodeFlags_Selected : 0)))
            {
                //Update selection state
                if (ImGui::IsItemClicked())
                {
                    state->SetSelectedZoneObject(&object);
                }
                object.Selected = &object == state->SelectedObject;


                for (auto& childObject : object.Children)
                {
                    auto childObjectClass = state->ZoneManager->GetObjectClass(childObject.Self->ClassnameHash);
                    if (!childObjectClass.Show)
                        continue;

                    if (ImGui::TreeNodeEx((string(objectClass.LabelIcon) + childObject.Self->Classname + "##" + std::to_string(index)).c_str(), ImGuiTreeNodeFlags_SpanAvailWidth | 
                        (childObject.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None) | (childObject.Selected ? ImGuiTreeNodeFlags_Selected : 0)))
                    {
                        //Update selection state
                        if (ImGui::IsItemClicked())
                        {
                            state->SetSelectedZoneObject(&childObject);
                        }
                        childObject.Selected = &childObject == state->SelectedObject;
                        ImGui::TreePop();
                    }
                    index++;
                }
                ImGui::TreePop();
            }
            index++;
        }
        ImGui::PopStyleVar();

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::End();
}