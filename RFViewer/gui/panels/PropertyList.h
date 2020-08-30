#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"
#include "RfgTools++/formats/zones/properties/primitive/StringProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/BoolProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/FloatProperty.h"
#include "RfgTools++/formats/zones/properties/primitive/UintProperty.h"
#include "RfgTools++/formats/zones/properties/compound/Vec3Property.h"
#include "RfgTools++/formats/zones/properties/compound/Matrix33Property.h"
#include "RfgTools++/formats/zones/properties/compound/BoundingBoxProperty.h"
#include "RfgTools++/formats/zones/properties/compound/OpProperty.h"
#include "RfgTools++/formats/zones/properties/special/DistrictFlagsProperty.h"
#include "RfgTools++/formats/zones/properties/compound/ListProperty.h"
#include "RfgTools++/formats/zones/properties/special/NavpointDataProperty.h"

void PropertyList_Update(GuiState* state)
{
    if (!ImGui::Begin("Properties", &state->Visible))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_WRENCH " Properties");
    state->FontManager->FontL.Pop();

    if (!state->SelectedObject)
    {
        ImGui::Text("%s Select a zone object to see it's properties", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        ZoneObjectNode36& selected = *state->SelectedObject;
        for (IZoneProperty* prop : selected.Self->Properties)
        {
            //Todo: Add support for these types
            //Ignore these types for now since they're not yet supported
            if (prop->DataType == ZonePropertyType::NavpointData || prop->DataType == ZonePropertyType::List || prop->DataType == ZonePropertyType::ConstraintTemplate)
                continue;

            ImGui::Separator();
            state->FontManager->FontMedium.Push();
            ImGui::Text(prop->Name);
            //state->FontManager->FontMedium.Pop();
            //ImGui::Separator();

            switch (prop->DataType)
            {
            case ZonePropertyType::None:
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();
                break;
            case ZonePropertyType::String:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[String]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", ((StringProperty*)prop)->Data);
                break;
            case ZonePropertyType::Bool:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Bool]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", ((BoolProperty*)prop)->Data ? "true" : "false");
                break;
            case ZonePropertyType::Float:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Float]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", std::to_string(((FloatProperty*)prop)->Data));
                break;
            case ZonePropertyType::Uint:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Uint]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", std::to_string(((UintProperty*)prop)->Data));
                break;
            case ZonePropertyType::BoundingBox:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Bounding box]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Min:", ((BoundingBoxProperty*)prop)->Min.String());
                gui::LabelAndValue("    - Max:", ((BoundingBoxProperty*)prop)->Max.String());
                break;
            //case ZonePropertyType::ConstraintTemplate: //Todo: Support this type
            //    ImGui::SameLine();
            //    ImGui::TextColored(gui::TertiaryTextColor, "[Constraint template]");
            //    state->FontManager->FontMedium.Pop();
            //    ImGui::Separator();


            //    break;
            case ZonePropertyType::Matrix33:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Matrix33]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Rvec:", ((Matrix33Property*)prop)->Data.rvec.String());
                gui::LabelAndValue("    - Uvec:", ((Matrix33Property*)prop)->Data.uvec.String());
                gui::LabelAndValue("    - Fvec:", ((Matrix33Property*)prop)->Data.fvec.String());
                break;
            case ZonePropertyType::Vec3:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Vec3]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", ((Vec3Property*)prop)->Data.String());
                break;
            case ZonePropertyType::DistrictFlags:
                {
                    ImGui::SameLine();
                    ImGui::TextColored(gui::TertiaryTextColor, "[DistrictFlags]");
                    state->FontManager->FontMedium.Pop();
                    ImGui::Separator();

                    u32 flags = (u32)((DistrictFlagsProperty*)prop)->Data;
                    gui::LabelAndValue("    - AllowCough:", (flags & (u32)DistrictFlags::AllowCough) != 0 ? "true" : "false");
                    gui::LabelAndValue("    - AllowAmbEdfCivilianDump:", (flags & (u32)DistrictFlags::AllowAmbEdfCivilianDump) != 0 ? "true" : "false");
                    gui::LabelAndValue("    - PlayCapstoneUnlockedLines:", (flags & (u32)DistrictFlags::PlayCapstoneUnlockedLines) != 0 ? "true" : "false");
                    gui::LabelAndValue("    - DisableMoraleChange:", (flags & (u32)DistrictFlags::DisableMoraleChange) != 0 ? "true" : "false");
                    gui::LabelAndValue("    - DisableControlChange:", (flags & (u32)DistrictFlags::DisableControlChange) != 0 ? "true" : "false"); 
                }
                break;
            //case ZonePropertyType::NavpointData:
            //    ImGui::SameLine();
            //    ImGui::TextColored(gui::TertiaryTextColor, "[NavpointData]");
            //    state->FontManager->FontMedium.Pop();
            //    ImGui::Separator();


            //    break;
            //case ZonePropertyType::List: //Todo: Support this type
            //    ImGui::SameLine();
            //    ImGui::TextColored(gui::TertiaryTextColor, "[List]");
            //    state->FontManager->FontMedium.Pop();
            //    ImGui::Separator();


            //    break;
            case ZonePropertyType::Op:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Orient & position]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Position:", ((OpProperty*)prop)->Position.String());
                gui::LabelAndValue("    - Orient.Rvec:", ((OpProperty*)prop)->Orient.rvec.String());
                gui::LabelAndValue("    - Orient.Uvec:", ((OpProperty*)prop)->Orient.uvec.String());
                gui::LabelAndValue("    - Orient.Fvec:", ((OpProperty*)prop)->Orient.fvec.String());
                break;
            default:
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();
                break;
            }
        }
    }

    ImGui::End();
}