#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"

//Todo: May want to move this to be under each zone in the zone window list since we'll have a third panel for per-object properties
void ZoneObjectsList_Update(GuiState* state)
{
    if (!ImGui::Begin("Zone objects", &state->Visible))
    {
        ImGui::End();
        return;
    }

    if (state->selectedZone == InvalidZoneIndex || state->selectedZone >= state->zoneManager_->ZoneFiles.size())
    {
        ImGui::Text("%s Select a zone to see the objects it contains.", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        //Draw object filters sub-window
        state->fontManager_->FontL.Push();
        ImGui::Text(ICON_FA_FILTER " Filters");
        state->fontManager_->FontL.Pop();
        ImGui::Separator();

        ImGui::BeginChild("##Zone object filters list", ImVec2(0, 200.0f), true);

        ImGui::Text(" " ICON_FA_EYE);
        gui::TooltipOnPrevious("Toggles whether bounding boxes are drawn for the object class", nullptr);
        ImGui::SameLine();
        ImGui::Text(" " ICON_FA_MARKER);
        gui::TooltipOnPrevious("Toggles whether class name labels are drawn for the object class", nullptr);

        for (auto& objectClass : state->zoneManager_->ZoneObjectClasses)
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


        //Draw zone objects list
        ImGui::Separator();
        state->fontManager_->FontL.Push();
        ImGui::Text(ICON_FA_BOXES " Zone objects");
        state->fontManager_->FontL.Pop();
        ImGui::Separator();

        //Object list
        auto& zone = state->zoneManager_->ZoneFiles[state->selectedZone].Zone;
        ImGui::BeginChild("##Zone object list", ImVec2(0, 0), true);

        u32 index = 0;
        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 200.0f);
        ImGui::SetColumnWidth(1, 300.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 0.75f); //Increase spacing to differentiate leaves from expanded contents.
        for (auto& object : zone.ObjectsHierarchical)
        {
            auto objectClass = state->zoneManager_->GetObjectClass(object.Self->ClassnameHash);
            if (!objectClass.Show)
                continue;

            //Todo: Use a formatting lib/func here. This is bad
            if (ImGui::TreeNodeEx((string(objectClass.LabelIcon) + object.Self->Classname + "##" + std::to_string(index)).c_str(),
                object.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None))
            {
                Vec3 position = object.Self->Bmin + ((object.Self->Bmax - object.Self->Bmin) / 2.0f);
                ImGui::NextColumn();
                ImGui::Text(" | {%.3f, %.3f, %.3f}", position.x, position.y, position.z);
                ImGui::NextColumn();

                for (auto& childObject : object.Children)
                {
                    auto childObjectClass = state->zoneManager_->GetObjectClass(childObject.Self->ClassnameHash);
                    if (!childObjectClass.Show)
                        continue;

                    if (ImGui::TreeNodeEx((string(objectClass.LabelIcon) + childObject.Self->Classname + "##" + std::to_string(index)).c_str(),
                        childObject.Children.size() == 0 ? ImGuiTreeNodeFlags_Leaf : ImGuiTreeNodeFlags_None))
                    {

                        ImGui::TreePop();
                    }
                    Vec3 childPosition = childObject.Self->Bmin + ((childObject.Self->Bmax - childObject.Self->Bmin) / 2.0f);
                    ImGui::NextColumn();
                    ImGui::Text(" | {%.3f, %.3f, %.3f}", childPosition.x, childPosition.y, childPosition.z);
                    ImGui::NextColumn();
                    index++;
                }
                ImGui::TreePop();
            }
            else
            {
                Vec3 position = object.Self->Bmin + ((object.Self->Bmax - object.Self->Bmin) / 2.0f);
                ImGui::NextColumn();
                ImGui::Text(" | {%.3f, %.3f, %.3f}", position.x, position.y, position.z);
                ImGui::NextColumn();
            }

            index++;
        }
        ImGui::PopStyleVar();
        ImGui::Columns(1);

        ImGui::EndChild();
    }

    ImGui::End();
}