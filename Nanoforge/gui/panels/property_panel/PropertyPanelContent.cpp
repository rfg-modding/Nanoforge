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
    if (!state->SelectedObject.Valid())
    {
        ImGui::Text("%s Select a zone object to see it's properties", ICON_FA_EXCLAMATION_CIRCLE);
    }
    else
    {
        ObjectHandle selected = state->SelectedObject;

        string name = selected.GetProperty("Name").Get<string>();
        if (name == "")
            name = selected.GetProperty("Classname").Get<string>();

        //Object name
        state->FontManager->FontMedium.Push();
        ImGui::Text(name);
        state->FontManager->FontMedium.Pop();

        //Object class name
        ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
        ImGui::Text(selected.GetProperty("Classname").Get<string>().c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        u32 handle = selected.GetProperty("Handle").Get<u32>();
        u32 num = selected.GetProperty("Num").Get<u32>();
        u32 flags = selected.GetProperty("Flags").Get<u16>();
        ImGui::InputScalar("Handle", ImGuiDataType_U32, &handle);
        ImGui::InputScalar("Num", ImGuiDataType_U32, &num);
        ImGui::InputScalar("Flags", ImGuiDataType_U16, &flags);
        if (ImGui::Button("Copy scriptx ref to clipboard"))
        {
            ImGui::LogToClipboard();
            ImGui::LogText("<object>%X\n", handle);
            ImGui::LogText("    <object_number>%d</object_number>\n", num);
            ImGui::LogText("</object>");
            ImGui::LogFinish();
        }

        std::vector<ObjectHandle>& properties = selected.GetProperty("Properties").GetObjectList();
        for (ObjectHandle prop : properties)
        {
            string typeName = prop.GetProperty("TypeName").Get<string>();
            const f32 indent = 15.0f;
            if (ImGui::CollapsingHeader(prop.GetProperty("Name").Get<string>().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Indent(indent);
                if (typeName == "String")
                {
                    string data = prop.GetProperty("String").Get<string>();
                    if (ImGui::InputText("String", &data))
                        prop.GetProperty("String").Set<string>(data);
                }
                else if (typeName == "Bool")
                {
                    bool data = prop.GetProperty("Bool").Get<bool>();
                    if (ImGui::Checkbox("Bool", &data))
                        prop.GetProperty("Bool").Set<bool>(data);
                }
                else if (typeName == "Float")
                {
                    f32 data = prop.GetProperty("Float").Get<f32>();
                    if (ImGui::InputFloat("Float", &data))
                        prop.GetProperty("Float").Set<f32>(data);
                }
                else if (typeName == "Uint")
                {
                    u32 data = prop.GetProperty("Uint").Get<u32>();
                    if (ImGui::InputScalar("Number", ImGuiDataType_U32, &data))
                        prop.GetProperty("Uint").Set<u32>(data);
                }
                else if (typeName == "BoundingBox")
                {
                    Vec3 bmin = prop.GetProperty("Bmin").Get<Vec3>();
                    Vec3 bmax = prop.GetProperty("Bmax").Get<Vec3>();
                    if (ImGui::InputFloat3("Min", (f32*)&bmin))
                        prop.GetProperty("Bmin").Set<Vec3>(bmin);
                    if (ImGui::InputFloat3("Max", (f32*)&bmax))
                        prop.GetProperty("Bmax").Set<Vec3>(bmax);
                }
                else if (typeName == "Vec3")
                {
                    Vec3 data = prop.GetProperty("Vector").Get<Vec3>();
                    if (ImGui::InputFloat3("Vector", (f32*)&data))
                        prop.GetProperty("Vector").Set<Vec3>(data);
                }
                else if (typeName == "Matrix33")
                {
                    Mat3 data = prop.GetProperty("Matrix33").Get<Mat3>();
                    if (ImGui::InputFloat3("Right", (f32*)&data.rvec))
                        prop.GetProperty("Matrix33").Set<Mat3>(data);
                    if (ImGui::InputFloat3("Up", (f32*)&data.uvec))
                        prop.GetProperty("Matrix33").Set<Mat3>(data);
                    if (ImGui::InputFloat3("Forward", (f32*)&data.fvec))
                        prop.GetProperty("Matrix33").Set<Mat3>(data);
                }
                else if (typeName == "Op")
                {
                    ImGui::Text("Orientation:");
                    Mat3 orient = prop.GetProperty("Orient").Get<Mat3>();
                    if (ImGui::InputFloat3("Right", (f32*)&orient.rvec))
                        prop.GetProperty("Orient").Set<Mat3>(orient);
                    if (ImGui::InputFloat3("Up", (f32*)&orient.uvec))
                        prop.GetProperty("Orient").Set<Mat3>(orient);
                    if (ImGui::InputFloat3("Forward", (f32*)&orient.fvec))
                        prop.GetProperty("Orient").Set<Mat3>(orient);

                    ImGui::Text("Position:");
                    Vec3 pos = prop.GetProperty("Position").Get<Vec3>();
                    if (ImGui::InputFloat3("Position", (f32*)&pos))
                        prop.GetProperty("Position").Set<Vec3>(pos);
                }
                else if (typeName == "DistrictFlags")
                {
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
                    //auto* districtFlagsProp = static_cast<DistrictFlagsProperty*>(prop);
                    //u32 flags = (u32)districtFlagsProp->Data;

                    //bool allowCough = ((flags & (u32)DistrictFlags::AllowCough) != 0);
                    //bool allowAmbEdfCivilianDump = ((flags & (u32)DistrictFlags::AllowAmbEdfCivilianDump) != 0);
                    //bool playCapstoneUnlockedLines = ((flags & (u32)DistrictFlags::PlayCapstoneUnlockedLines) != 0);
                    //bool disableMoraleChange = ((flags & (u32)DistrictFlags::DisableMoraleChange) != 0);
                    //bool disableControlChange = ((flags & (u32)DistrictFlags::DisableControlChange) != 0);

                    ////Draws checkbox for flag and updates bitflags stored in flags
                    //#define DrawDistrictFlagsCheckbox(text, value, flagEnum) \
                    //    if (ImGui::Checkbox(text, &value)) \
                    //    { \
                    //        if (value) \
                    //            flags |= (u32)flagEnum; \
                    //        else \
                    //            flags &= (~(u32)flagEnum); \
                    //    } \

                    //DrawDistrictFlagsCheckbox("Allow cough", allowCough, DistrictFlags::AllowCough);
                    //DrawDistrictFlagsCheckbox("Allow edf civilian dump", allowAmbEdfCivilianDump, DistrictFlags::AllowAmbEdfCivilianDump);
                    //DrawDistrictFlagsCheckbox("Play capstone unlocked lines", playCapstoneUnlockedLines, DistrictFlags::PlayCapstoneUnlockedLines);
                    //DrawDistrictFlagsCheckbox("Disable morale change", disableMoraleChange, DistrictFlags::DisableMoraleChange);
                    //DrawDistrictFlagsCheckbox("Disable control change", disableControlChange, DistrictFlags::DisableControlChange);

                    //districtFlagsProp->Data = (DistrictFlags)flags;
                }
                else if (typeName == "List")
                {
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
                }
                else if (typeName == "NavpointData")
                {
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
                    //auto* propNavpointData = static_cast<NavpointDataProperty*>(prop);
                    //ImGui::InputScalar("Type", ImGuiDataType_U32, &propNavpointData->NavpointType);
                    //ImGui::InputScalar("Unknown0", ImGuiDataType_U32, &propNavpointData->UnkFlag1); //Note: This might actually be type
                    //ImGui::InputFloat("Radius", &propNavpointData->Radius);
                    //ImGui::InputFloat("Speed", &propNavpointData->Speed);
                    //ImGui::InputScalar("Unknown1", ImGuiDataType_U32, &propNavpointData->UnkFlag2);
                    //ImGui::InputScalar("Unknown2", ImGuiDataType_U32, &propNavpointData->UnkFlag3);
                    //ImGui::InputScalar("Unknown3", ImGuiDataType_U32, &propNavpointData->UnkVar1);
                }
                else if (typeName == "Constraint")
                {
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
                }
                else
                {
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_CIRCLE " Unsupported property type");
                }

                ImGui::Unindent(indent);
            }
        }
    }
}