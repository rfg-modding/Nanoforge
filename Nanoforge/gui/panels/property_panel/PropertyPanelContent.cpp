#include "PropertyPanelContent.h"
#include "gui/panels/file_explorer/FileExplorer.h"
#include "render/imgui/imgui_ext.h"
#include "RfgTools++/formats/zones/ZoneFile.h"
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
        ImGui::Text("Zone object disabled");
        //ZoneObjectNode36& selected = *state->SelectedObject;

        ////Attempt to find a human friendly name for the object
        //string name = "";
        //auto* displayName = selected.Self->GetProperty<StringProperty>("display_name");
        //auto* chunkName = selected.Self->GetProperty<StringProperty>("chunk_name");
        //auto* animationType = selected.Self->GetProperty<StringProperty>("animation_type");
        //auto* activityType = selected.Self->GetProperty<StringProperty>("activity_type");
        //auto* raidType = selected.Self->GetProperty<StringProperty>("raid_type");
        //auto* courierType = selected.Self->GetProperty<StringProperty>("courier_type");
        //auto* spawnSet = selected.Self->GetProperty<StringProperty>("spawn_set");
        //auto* itemType = selected.Self->GetProperty<StringProperty>("item_type");
        //auto* dummyType = selected.Self->GetProperty<StringProperty>("dummy_type");
        //auto* weaponType = selected.Self->GetProperty<StringProperty>("weapon_type");
        //auto* regionKillType = selected.Self->GetProperty<StringProperty>("region_kill_type");
        //auto* deliveryType = selected.Self->GetProperty<StringProperty>("delivery_type");
        //auto* squadDef = selected.Self->GetProperty<StringProperty>("squad_def");
        //auto* missionInfo = selected.Self->GetProperty<StringProperty>("mission_info");
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

        //if (name == "")
        //    name = selected.Self->Classname;

        //state->FontManager->FontMedium.Push();
        //ImGui::Text(name);
        //state->FontManager->FontMedium.Pop();
        //ImGui::Separator();

        //ImGui::InputScalar("Handle", ImGuiDataType_U32, &selected.Self->Handle);
        //ImGui::InputScalar("Num", ImGuiDataType_U32, &selected.Self->Num);
        //ImGui::InputScalar("Flags", ImGuiDataType_U16, &selected.Self->Flags);
        //if (ImGui::Button("Copy scriptx ref to clipboard"))
        //{
        //    ImGui::LogToClipboard();
        //    ImGui::LogText("<object>%X\n", selected.Self->Handle);
        //    ImGui::LogText("    <object_number>%d</object_number>\n", selected.Self->Num);
        //    ImGui::LogText("</object>");
        //    ImGui::LogFinish();
        //}

        ImGui::Text("Zone properties disabled");
        //for (IZoneProperty* prop : selected.Self->Properties)
        //{
        //    //Todo: Add support for these types
        //    //Ignore these types for now since they're not yet supported
        //    if (!prop || prop->DataType == ZonePropertyType::List || prop->DataType == ZonePropertyType::ConstraintTemplate)
        //        continue;

        //    const f32 indent = 15.0f;
        //    if (ImGui::CollapsingHeader(prop->Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        //    {
        //        ImGui::Indent(indent);
        //        switch (prop->DataType)
        //        {
        //            break;
        //        case ZonePropertyType::String:
        //            ImGui::InputText("String", static_cast<StringProperty*>(prop)->Data);
        //            break;
        //        case ZonePropertyType::Bool:
        //            ImGui::Checkbox("Bool", &static_cast<BoolProperty*>(prop)->Data);
        //            break;
        //        case ZonePropertyType::Float:
        //            ImGui::InputFloat("Float", &static_cast<FloatProperty*>(prop)->Data);
        //            break;
        //        case ZonePropertyType::Uint:
        //            ImGui::InputScalar("Number", ImGuiDataType_U32, &static_cast<UintProperty*>(prop)->Data);
        //            break;
        //        case ZonePropertyType::BoundingBox:
        //            ImGui::InputFloat3("Min", (f32*)&static_cast<BoundingBoxProperty*>(prop)->Min);
        //            ImGui::InputFloat3("Max", (f32*)&static_cast<BoundingBoxProperty*>(prop)->Max);
        //            break;
        //        //case ZonePropertyType::ConstraintTemplate: //Todo: Support this type
        //        //    ImGui::SameLine();
        //        //    ImGui::TextColored(gui::TertiaryTextColor, "[Constraint template]");
        //        //    state->FontManager->FontMedium.Pop();
        //        //    ImGui::Separator();


        //        //    break;
        //        case ZonePropertyType::Matrix33:
        //            ImGui::InputFloat3("Right", (f32*)&static_cast<Matrix33Property*>(prop)->Data.rvec);
        //            ImGui::InputFloat3("Up", (f32*)&static_cast<Matrix33Property*>(prop)->Data.uvec);
        //            ImGui::InputFloat3("Forward", (f32*)&static_cast<Matrix33Property*>(prop)->Data.fvec);
        //            break;
        //        case ZonePropertyType::Vec3:
        //            ImGui::InputFloat3("Vector", (f32*)&static_cast<Vec3Property*>(prop)->Data);
        //            break;
        //        case ZonePropertyType::DistrictFlags:
        //            {
        //                auto* districtFlagsProp = static_cast<DistrictFlagsProperty*>(prop);
        //                u32 flags = (u32)districtFlagsProp->Data;

        //                bool allowCough = ((flags & (u32)DistrictFlags::AllowCough) != 0);
        //                bool allowAmbEdfCivilianDump = ((flags & (u32)DistrictFlags::AllowAmbEdfCivilianDump) != 0);
        //                bool playCapstoneUnlockedLines = ((flags & (u32)DistrictFlags::PlayCapstoneUnlockedLines) != 0);
        //                bool disableMoraleChange = ((flags & (u32)DistrictFlags::DisableMoraleChange) != 0);
        //                bool disableControlChange = ((flags & (u32)DistrictFlags::DisableControlChange) != 0);

        //                //Draws checkbox for flag and updates bitflags stored in flags
        //                #define DrawDistrictFlagsCheckbox(text, value, flagEnum) \
        //                if (ImGui::Checkbox(text, &value)) \
        //                { \
        //                    if (value) \
        //                        flags |= (u32)flagEnum; \
        //                    else \
        //                        flags &= (~(u32)flagEnum); \
        //                } \

        //                DrawDistrictFlagsCheckbox("Allow cough", allowCough, DistrictFlags::AllowCough);
        //                DrawDistrictFlagsCheckbox("Allow edf civilian dump", allowAmbEdfCivilianDump, DistrictFlags::AllowAmbEdfCivilianDump);
        //                DrawDistrictFlagsCheckbox("Play capstone unlocked lines", playCapstoneUnlockedLines, DistrictFlags::PlayCapstoneUnlockedLines);
        //                DrawDistrictFlagsCheckbox("Disable morale change", disableMoraleChange, DistrictFlags::DisableMoraleChange);
        //                DrawDistrictFlagsCheckbox("Disable control change", disableControlChange, DistrictFlags::DisableControlChange);

        //                districtFlagsProp->Data = (DistrictFlags)flags;
        //            }
        //            break;
        //        case ZonePropertyType::NavpointData:
        //            {
        //                auto* propNavpointData = static_cast<NavpointDataProperty*>(prop);
        //                ImGui::InputScalar("Type", ImGuiDataType_U32, &propNavpointData->NavpointType);
        //                ImGui::InputScalar("Unknown0", ImGuiDataType_U32, &propNavpointData->UnkFlag1); //Note: This might actually be type
        //                ImGui::InputFloat("Radius", &propNavpointData->Radius);
        //                ImGui::InputFloat("Speed", &propNavpointData->Speed);
        //                ImGui::InputScalar("Unknown1", ImGuiDataType_U32, &propNavpointData->UnkFlag2);
        //                ImGui::InputScalar("Unknown2", ImGuiDataType_U32, &propNavpointData->UnkFlag3);
        //                ImGui::InputScalar("Unknown3", ImGuiDataType_U32, &propNavpointData->UnkVar1);
        //            }
        //            break;
        //        //case ZonePropertyType::List: //Todo: Support this type
        //        //    ImGui::SameLine();
        //        //    ImGui::TextColored(gui::TertiaryTextColor, "[List]");
        //        //    state->FontManager->FontMedium.Pop();
        //        //    ImGui::Separator();


        //        //    break;
        //        case ZonePropertyType::Op:
        //            ImGui::Text("Orientation:");
        //            ImGui::InputFloat3("Right", (f32*)&static_cast<OpProperty*>(prop)->Orient.rvec);
        //            ImGui::InputFloat3("Up", (f32*)&static_cast<OpProperty*>(prop)->Orient.uvec);
        //            ImGui::InputFloat3("Forward", (f32*)&static_cast<OpProperty*>(prop)->Orient.fvec);

        //            ImGui::Separator();
        //            ImGui::Text("Position:");
        //            ImGui::InputFloat3("Vector", (f32*)&static_cast<OpProperty*>(prop)->Position);
        //            break;
        //        case ZonePropertyType::None:
        //        default:
        //            break;
        //        }

        //        ImGui::Unindent(indent);
        //    }
        //}
    }
}