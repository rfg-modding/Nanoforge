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

        string name = selected.Property("Name").Get<string>();
        if (name == "")
            name = selected.Property("Type").Get<string>();

        //Object name
        state->FontManager->FontMedium.Push();
        ImGui::Text(name);
        state->FontManager->FontMedium.Pop();

        //Object class name
        ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
        ImGui::Text(selected.Property("Type").Get<string>().c_str());
        ImGui::PopStyleColor();
        ImGui::Separator();

        u32 handle = selected.Property("Handle").Get<u32>();
        u32 num = selected.Property("Num").Get<u32>();
        u32 flags = selected.Property("Flags").Get<u16>();
        gui::LabelAndValue("Type:", selected.Get<string>("Type"));
        gui::LabelAndValue("Handle:", std::to_string(handle));
        gui::LabelAndValue("Num:", std::to_string(num));
        gui::LabelAndValue("Flags:", std::to_string(flags));
        if (ImGui::Button("Copy scriptx reference"))
        {
            ImGui::LogToClipboard();
            ImGui::LogText("<object>%X\n", handle);
            ImGui::LogText("    <object_number>%d</object_number>\n", num);
            ImGui::LogText("</object>");
            ImGui::LogFinish();
        }
        
        static std::vector<string> HiddenProperties = //These ones aren't drawn
        {
            "Handle", "Num", "Flags", "ClassnameHash", "RfgPropertyNames", "BlockSize", "ParentHandle", "SiblingHandle", "ChildHandle",
            "NumProps", "PropBlockSize", "Type"
        };
        for (PropertyHandle prop : selected.Properties())
        {
            if (std::ranges::find(HiddenProperties, prop.Name()) != HiddenProperties.end())
                continue; //Skip hidden properties

            bool propertyEdited = false;
            const f32 indent = 15.0f;
            ImGui::PushID(fmt::format("{}_{}", selected.UID(), prop.Index()).c_str());
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
                bool data = prop.Get<bool>();
                if (ImGui::Checkbox(prop.Name().c_str(), &data))
                {
                    prop.Set<bool>(data);
                    propertyEdited = true;
                }
            }
            else if (prop.IsType<string>())
            {
                string data = prop.Get<string>();
                if (ImGui::InputText(prop.Name().c_str(), &data))
                {
                    prop.Set<string>(data);
                    propertyEdited = true;
                }
            }
            else if (prop.IsType<std::vector<ObjectHandle>>())
            {
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
                Vec3 data = prop.Get<Vec3>();
                if (ImGui::InputFloat3(prop.Name().c_str(), (f32*)&data))
                {
                    prop.Set<Vec3>(data);
                    propertyEdited = true;
                }
            }
            else if (prop.IsType<Mat3>())
            {
                ImGui::Text(prop.Name() + ":");
                ImGui::Indent(indent);

                Mat3 data = prop.Get<Mat3>();
                if (ImGui::InputFloat3("Right", (f32*)&data.rvec))
                {
                    prop.Set<Mat3>(data);
                    propertyEdited = true;
                }
                if (ImGui::InputFloat3("Up", (f32*)&data.uvec))
                {
                    prop.Set<Mat3>(data);
                    propertyEdited = true;
                }
                if (ImGui::InputFloat3("Forward", (f32*)&data.fvec))
                {
                    prop.Set<Mat3>(data);
                    propertyEdited = true;
                }
                ImGui::Unindent(indent);
            }
            else if (prop.IsType<ObjectHandle>())
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
                state->CurrentTerritoryUpdateDebugDraw = true; //Update in case value related to debug draw is changed
            }
        }
    }
}