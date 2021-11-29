#include "PropertyPanelContent.h"
#include "gui/panels/file_explorer/FileExplorer.h"
#include "render/imgui/imgui_ext.h"
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
#include "RfgTools++/formats/zones/ZonePc36.h"
#include "util/Profiler.h"
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"
#include "rfg/PackfileVFS.h"
#include "render/imgui/ImGuiFontManager.h"
#include "RfgTools++/formats/packfiles/Packfile3.h"

void DrawPackfileData(Handle<Packfile3> packfile)
{
    PROFILER_FUNCTION();
    gui::LabelAndValue("Name:", packfile->Name());
    gui::LabelAndValue("Version:", std::to_string(packfile->Header.Version));
    gui::LabelAndValue("Flags:", std::to_string(packfile->Header.Flags));
    gui::LabelAndValue("Compressed:", packfile->Compressed ? "True" : "False");
    gui::LabelAndValue("Condensed:", packfile->Condensed ? "True" : "False");
    gui::LabelAndValue("Num subfiles:", std::to_string(packfile->Header.NumberOfSubfiles));
    gui::LabelAndValue("Total size:", std::to_string((f32)packfile->Header.FileSize / 1000.0f) + "KB");
    gui::LabelAndValue("Subfile data size:", std::to_string((f32)packfile->Header.DataSize / 1000.0f) + "KB");
    gui::LabelAndValue("Compressed subfile data size:", std::to_string((f32)packfile->Header.CompressedDataSize / 1000.0f) + "KB");
}

void PropertyPanel_VppContent(GuiState* state)
{
    PROFILER_FUNCTION();
    if (!state->FileExplorer_SelectedNode)
        return;

    //Cache packfile, only find again if selected node changes
    static FileExplorerNode* lastSelectedNode = nullptr;
    static Handle<Packfile3> packfile = nullptr;
    if (state->FileExplorer_SelectedNode != lastSelectedNode)
    {
        lastSelectedNode = state->FileExplorer_SelectedNode;
        packfile = state->PackfileVFS->GetPackfile(state->FileExplorer_SelectedNode->Filename);
    }

    //Draw packfile data if we've got it
    if (packfile)
        DrawPackfileData(packfile);
    else //Else draw an error message
        ImGui::Text("%s Failed to get packfile info.", ICON_FA_EXCLAMATION_CIRCLE);
}

void PropertyPanel_Str2Content(GuiState* state)
{
    PROFILER_FUNCTION();
    if (!state->FileExplorer_SelectedNode)
        return;

    //Cache container, only find again if selected node changes
    static FileExplorerNode* lastSelectedNode = nullptr;
    static Handle<Packfile3> container = nullptr;
    if (state->FileExplorer_SelectedNode != lastSelectedNode)
    {
        lastSelectedNode = state->FileExplorer_SelectedNode;
        container = state->PackfileVFS->GetContainer(state->FileExplorer_SelectedNode->Filename, state->FileExplorer_SelectedNode->ParentName);
        if (!container)
            return;
    }

    //Draw container data if we've got it
    if (container)
        DrawPackfileData(container);
    else //Else draw an error message
        ImGui::Text("%s Failed to get container info.", ICON_FA_EXCLAMATION_CIRCLE);
}

void PropertyPanel_ZoneObject(GuiState* state)
{
    PROFILER_FUNCTION();
    if (!state->SelectedObject)
    {
        ImGui::Text("%s Select a zone object to see it's properties", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        ZoneObjectNode36& selected = *state->SelectedObject;
        gui::LabelAndValue("Handle:", std::to_string(selected.Self->Handle));
        gui::LabelAndValue("Num:", std::to_string(selected.Self->Num));
        if (ImGui::Button("Copy scriptx ref to clipboard"))
        {
            ImGui::LogToClipboard();
            ImGui::LogText("<object>%X\n", selected.Self->Handle);
            ImGui::LogText("    <object_number>%d</object_number>\n", selected.Self->Num);
            ImGui::LogText("</object>");
            ImGui::LogFinish();
        }

        for (IZoneProperty* prop : selected.Self->Properties)
        {
            //Todo: Add support for these types
            //Ignore these types for now since they're not yet supported
            if (!prop || prop->DataType == ZonePropertyType::NavpointData || prop->DataType == ZonePropertyType::List || prop->DataType == ZonePropertyType::ConstraintTemplate)
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

                gui::LabelAndValue("    - Value:", static_cast<StringProperty*>(prop)->Data);
                break;
            case ZonePropertyType::Bool:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Bool]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", static_cast<BoolProperty*>(prop)->Data ? "true" : "false");
                break;
            case ZonePropertyType::Float:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Float]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", std::to_string(static_cast<FloatProperty*>(prop)->Data));
                break;
            case ZonePropertyType::Uint:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Uint]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", std::to_string(static_cast<UintProperty*>(prop)->Data));
                break;
            case ZonePropertyType::BoundingBox:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Bounding box]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Min:", static_cast<BoundingBoxProperty*>(prop)->Min.String());
                gui::LabelAndValue("    - Max:", static_cast<BoundingBoxProperty*>(prop)->Max.String());
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

                gui::LabelAndValue("    - Rvec:", static_cast<Matrix33Property*>(prop)->Data.rvec.String());
                gui::LabelAndValue("    - Uvec:", static_cast<Matrix33Property*>(prop)->Data.uvec.String());
                gui::LabelAndValue("    - Fvec:", static_cast<Matrix33Property*>(prop)->Data.fvec.String());
                break;
            case ZonePropertyType::Vec3:
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[Vec3]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                gui::LabelAndValue("    - Value:", static_cast<Vec3Property*>(prop)->Data.String());
                break;
            case ZonePropertyType::DistrictFlags:
            {
                ImGui::SameLine();
                ImGui::TextColored(gui::TertiaryTextColor, "[DistrictFlags]");
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();

                u32 flags = static_cast<u32>(static_cast<DistrictFlagsProperty*>(prop)->Data);
                gui::LabelAndValue("    - AllowCough:", (flags & static_cast<u32>(DistrictFlags::AllowCough)) != 0 ? "true" : "false");
                gui::LabelAndValue("    - AllowAmbEdfCivilianDump:", (flags & static_cast<u32>(DistrictFlags::AllowAmbEdfCivilianDump)) != 0 ? "true" : "false");
                gui::LabelAndValue("    - PlayCapstoneUnlockedLines:", (flags & static_cast<u32>(DistrictFlags::PlayCapstoneUnlockedLines)) != 0 ? "true" : "false");
                gui::LabelAndValue("    - DisableMoraleChange:", (flags & static_cast<u32>(DistrictFlags::DisableMoraleChange)) != 0 ? "true" : "false");
                gui::LabelAndValue("    - DisableControlChange:", (flags & static_cast<u32>(DistrictFlags::DisableControlChange)) != 0 ? "true" : "false");
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

                gui::LabelAndValue("    - Position:", static_cast<OpProperty*>(prop)->Position.String());
                gui::LabelAndValue("    - Orient.Rvec:", static_cast<OpProperty*>(prop)->Orient.rvec.String());
                gui::LabelAndValue("    - Orient.Uvec:", static_cast<OpProperty*>(prop)->Orient.uvec.String());
                gui::LabelAndValue("    - Orient.Fvec:", static_cast<OpProperty*>(prop)->Orient.fvec.String());
                break;
            default:
                state->FontManager->FontMedium.Pop();
                ImGui::Separator();
                break;
            }
        }
    }
}