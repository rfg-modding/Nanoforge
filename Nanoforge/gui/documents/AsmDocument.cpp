#include "AsmDocument.h"
#include "render/imgui/imgui_ext.h"
#include "gui/GuiState.h"
#include "Log.h"
#include "RfgTools++/formats/asm/AsmFile5.h"
#include "RfgTools++/formats/packfiles/Packfile3.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "application/Registry.h"

//Todo: Move conversion to/from RFG formats and the editor format into Importer/Exporter classes or namespaces
ObjectHandle AsmFile5ToObject(AsmFile5& asmFile)
{
    Registry& registry = Registry::Get();

    //Create editor object for asm_pc file
    ObjectHandle object = registry.CreateObject(asmFile.Name, "AsmFile");
    object.GetOrCreateProperty("Name").Set(asmFile.Name);
    object.GetOrCreateProperty("Signature").Set(asmFile.Signature);
    object.GetOrCreateProperty("Version").Set(asmFile.Version);
    //object.GetOrCreateProperty("Containers").Set({});
    //Todo: Somehow add a container list to the asmFile object. Ideas:
    //          - Add a list and sub-object list property type. Could probably just be std::vector<u64> or something
    //          - Add a list subobject, then add each container to that, like xml (seems a bit messy)
    //          - Add each one as a subobject (bad if you wanted multiple lists in an object)

    //Set containers
    for (AsmContainer& asmContainer : asmFile.Containers)
    {
        ObjectHandle container = registry.CreateObject(asmContainer.Name, "AsmContainer");
        container.GetOrCreateProperty("Name").Set(asmContainer.Name);
        container.GetOrCreateProperty("Type").Set((u8)asmContainer.Type);
        container.GetOrCreateProperty("Flags").Set((u16)asmContainer.Flags);
        container.GetOrCreateProperty("DataOffset").Set(asmContainer.DataOffset);
        container.GetOrCreateProperty("CompressedSize").Set(asmContainer.CompressedSize);
        //container.GetOrCreateProperty("Primitives").Set({});

        //Set container primitives
        size_t i = 0;
        for (AsmPrimitive& asmPrimitive : asmContainer.Primitives)
        {
            //TODO: Add to container primitives list, however lists are determined to work
            ObjectHandle primitive = registry.CreateObject(asmPrimitive.Name, "AsmPrimitive");
            primitive.GetOrCreateProperty("Name").Set(asmPrimitive.Name);
            primitive.GetOrCreateProperty("Type").Set((u8)asmPrimitive.Type);
            primitive.GetOrCreateProperty("Allocator").Set((u8)asmPrimitive.Allocator);
            primitive.GetOrCreateProperty("Flags").Set((u8)asmPrimitive.Flags);
            primitive.GetOrCreateProperty("SplitExtIndex").Set(asmPrimitive.SplitExtIndex);
            primitive.GetOrCreateProperty("TotalSize").Set(asmContainer.PrimitiveSizes[i]);
            primitive.GetOrCreateProperty("HeaderSize").Set(asmPrimitive.HeaderSize);
            primitive.GetOrCreateProperty("DataSize").Set(asmPrimitive.DataSize);
            i++;
        }
    }

    return object;
}

AsmDocument::AsmDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer)
    : filename_(filename), parentName_(parentName), vppName_(vppName), inContainer_(inContainer)
{
    //Get packfile. All asm_pc files are in .vpp_pc files
    Handle<Packfile3> vpp = state->PackfileVFS->GetPackfile(vppName);

    //Find asm_pc file in parent packfile
    for (auto& asmFile : vpp->AsmFiles)
        if (String::EqualIgnoreCase(asmFile.Name, filename))
        {
            asmFile_ = &asmFile;
            _asmFileObject = AsmFile5ToObject(asmFile);
            break;
        }

    //Report error if asm_pc file isn't found
    if (!asmFile_)
    {
        LOG_ERROR("Failed to find {}. Closing asm document.", filename_);
        Open = false;
        return;
    }
}

AsmDocument::~AsmDocument()
{

}

void AsmDocument::Update(GuiState* state)
{
    if (!asmFile_)
        return;

    const f32 indent = 30.0f;
    ImGui::Separator();
    state->FontManager->FontMedium.Push();
    ImGui::Text(fmt::format("{} {}", ICON_FA_DATABASE, Title.c_str()));
    state->FontManager->FontMedium.Pop();
    ImGui::Separator();

    //Header data
    ImGui::Indent(indent);
    gui::LabelAndValue("Name:", asmFile_->Name);
    gui::LabelAndValue("Signature:", std::to_string(asmFile_->Version));
    gui::LabelAndValue("Version:", std::to_string(asmFile_->Version));
    gui::LabelAndValue("# Containers:", std::to_string(asmFile_->ContainerCount));
    ImGui::Unindent(indent);

    ImGui::Separator();
    state->FontManager->FontMedium.Push();
    ImGui::Text(ICON_FA_BOX " Containers");
    state->FontManager->FontMedium.Pop();
    ImGui::Separator();

    //Search box
    static bool caseSensitive = false;
    if (ImGui::CollapsingHeader("Options"))
    {
        ImGui::Checkbox("Case sensitive", &caseSensitive);
    }
    ImGui::InputText("Search", &search_);
    ImGui::SameLine();
    gui::HelpMarker("This only searches for container names. It doesn't check the names of primitives within them.", ImGui::GetIO().FontDefault);
    ImGui::Separator();

    //Container data
    ImGui::Indent(indent);
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.25f); //Increase spacing to differentiate leaves from expanded contents.
    for (auto& container : asmFile_->Containers)
    {
        string containerName = caseSensitive ? container.Name : String::ToLower(container.Name);
        string search = caseSensitive ? search_ : String::ToLower(search_);
        if (!String::Contains(containerName, search))
            continue;

        if (ImGui::TreeNodeEx(container.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            gui::LabelAndValue("Type: ", to_string(container.Type));
            gui::LabelAndValue("Flags: ", std::to_string((u32)container.Flags));
            gui::LabelAndValue("# Primitives: ", std::to_string(container.PrimitiveCount));
            gui::LabelAndValue("Data offset: ", std::to_string(container.DataOffset));
            gui::LabelAndValue("Compressed size: ", std::to_string(container.CompressedSize));

            //Primitive data
            if (ImGui::TreeNode(fmt::format("Primitives##{}", (u64)&container).c_str()))
            {
                for (auto& primitive : container.Primitives)
                {
                    if (ImGui::TreeNode(primitive.Name.c_str()))
                    {
                        gui::LabelAndValue("Type: ", to_string(primitive.Type));
                        gui::LabelAndValue("Allocator: ", to_string(primitive.Allocator));
                        gui::LabelAndValue("Flags: ", std::to_string((u32)primitive.Flags));
                        gui::LabelAndValue("Split ext index: ", std::to_string(primitive.SplitExtIndex));
                        gui::LabelAndValue("Header size: ", std::to_string(primitive.HeaderSize));
                        gui::LabelAndValue("Data size: ", std::to_string(primitive.DataSize));
                        ImGui::TreePop();
                    }
                }
                ImGui::TreePop();
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopStyleVar();
    ImGui::Unindent(indent);
}

#pragma warning(disable:4100)
void AsmDocument::Save(GuiState* state)
{

}
#pragma warning(default:4100)