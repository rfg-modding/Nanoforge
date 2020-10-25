#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"

struct TextureDocumentData
{
    string Filename;
    string ParentName;
    string VppName;
    string ExtractionPath;
    PegFile10 Peg;
    std::span<u8> CpuFileBytes;
    std::span<u8> GpuFileBytes;
    std::vector<ImTextureID> ImageTextures;
    bool InContainer;

    //Ui state
    //If true gpu resource creation failed for the selected texture
    bool createFailed = false;
    u32 selectedIndex = 0;
    //Todo: Add checkered background option
    //Image column background color
    ImVec4 imageBackground = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];
};

//Todo: Move into some util class/file
//Convert peg format to dxgi format
DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input);

void TextureDocument_Init(GuiState* state, Document& doc)
{
    //Get parent packfile
    TextureDocumentData* data = (TextureDocumentData*)doc.Data;
    PackfileVFS* vfs = state->PackfileVFS;
    Packfile3* parent = data->InContainer ? vfs->GetContainer(data->ParentName, data->VppName) : vfs->GetPackfile(data->ParentName);
    if (!parent)
    {
        Log->error("Failed to get parent in TextureDocument_Init() for TextureDocument '{}'", data->Filename);
        return;
    }

    //Get gpu filename
    string extension = Path::GetExtension(data->Filename);
    string filenameNoExt = Path::GetFileNameNoExtension(data->Filename);
    string gpuFileName = filenameNoExt;
    if (extension == ".cpeg_pc")
        gpuFileName += ".gpeg_pc";
    else if (extension == ".cvbm_pc")
        gpuFileName += ".gvbm_pc";
    else
    {
        Log->error("Invalid texture extension '{}' in TextureDocument_Init() for TextureDocument '{}'", extension, data->Filename);
        return;
    }

    //Get cpu file and gpu file bytes
    auto cpuFile = parent->ExtractSingleFile(String::ToLower(data->Filename), true);
    auto gpuFile = parent->ExtractSingleFile(String::ToLower(gpuFileName), true);
    if (!cpuFile)
    {
        Log->error("Failed to extract cpu file in TextureDocument_Init() for TextureDocument '{}'", data->Filename);
        return;
    }
    if (!gpuFile)
    {
        Log->error("Failed to extract gpu file in TextureDocument_Init() for TextureDocument '{}'", data->Filename);
        return;
    }

    //Todo: Extract the peg to the filesystem and interact with that copy instead of caching it's bytes. Caching in RAM is unsustainable because some gpeg/gvbm files are huge
    //Parse peg and cache bytes for later manipulation
    data->CpuFileBytes = cpuFile.value();
    data->GpuFileBytes = gpuFile.value();
    BinaryReader cpuFileReader(cpuFile.value());
    BinaryReader gpuFileReader(gpuFile.value());
    data->Peg.Read(cpuFileReader, gpuFileReader);

    //Fill texture list with nullptrs. When a sub-image of the peg is opened it'll be rendered from this list.
    //If the index of the sub-image is a nullptr then it'll be loaded from the peg gpu file
    for (u32 i = 0; i < data->Peg.Entries.size(); i++)
    {
        data->ImageTextures.push_back(nullptr);
    }
}

void TextureDocument_Update(GuiState* state, Document& doc)
{
    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }

    TextureDocumentData* data = (TextureDocumentData*)doc.Data;
    
    //Controls max size of selected image in gui relative to the size of it's column
    static f32 imageViewSizeMultiplier = 0.75f;
    //ImGui::SliderFloat("Image view multiplier", &imageViewSizeMultiplier, 0.05f, 1.0f);

    ImGui::Columns(2);
    ImGui::ColorEdit4("Background color", (float*)&data->imageBackground, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::SetColumnWidth(0, 300.0f);
    //Texture entry list
    if (ImGui::BeginChild("##EntryList"))
    {
        for (u32 i = 0; i < data->Peg.Entries.size(); i++)
        {
            PegEntry10& entry = data->Peg.Entries[i];
            bool selected = data->selectedIndex == i;
            if (ImGui::Selectable(entry.Name.c_str(), selected))
            {
                data->selectedIndex = i;
                data->createFailed = false; //Reset so the next texture can attempt at least one load
            }
        }
        ImGui::EndChild();
    }

    //Selected image display display
    ImGui::NextColumn();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, data->imageBackground);
    if (data->Peg.Entries.size() != 0 && ImGui::BeginChild("##ImageView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        if (data->selectedIndex >= data->Peg.Entries.size())
            data->selectedIndex = 0;
        
        PegEntry10& entry = data->Peg.Entries[data->selectedIndex];
        ImTextureID entryTexture = data->ImageTextures[data->selectedIndex];
        if (!entryTexture && !data->createFailed)
        {
            BinaryReader gpuFileReader(data->GpuFileBytes);
            data->Peg.ReadTextureData(gpuFileReader, entry);
            DXGI_FORMAT format = PegFormatToDxgiFormat(entry.BitmapFormat);
            ImTextureID id = state->Renderer->TextureDataToHandle(entry.RawData, format, entry.Width, entry.Height);
            if (!id)
            {
                Log->error("Failed to create texture resource for texture entry {} in peg file {}", entry.Name, doc.Title);
                data->createFailed = true;
            }
            else
            {
                data->ImageTextures[data->selectedIndex] = id;
            }
        }
        else
        {
            //Scale image to free space while preserving aspect ratio
            ImVec2 maxDim(ImGui::GetColumnWidth(1), ImGui::GetWindowHeight() * imageViewSizeMultiplier);
            f32 maxWidthMultiples = maxDim.x / (f32)entry.Width;
            f32 maxHeightMultiples = maxDim.y / (f32)entry.Height;
            f32 dimMultiplier = std::min(maxWidthMultiples, maxHeightMultiples);

            ImGui::Image(entryTexture, ImVec2((f32)entry.Width * dimMultiplier, (f32)entry.Height * dimMultiplier));
        }

        ImGui::EndChild();
    }
    ImGui::PopStyleColor();

    //Todo: Display selected texture info in properties panel or in the document
    //Todo: Add TextureDocument_OnClose. Should do:
    //Todo: - Free shader resource views for any textures we viewed (ImTextureID)
    //Todo: - Free cpu and gpu file buffers plus run Cleanup() on textures
    //Todo: - Free anything else I'm forgetting...
    //Todo: - Free parent if it's a container

    ImGui::Columns(1);
    ImGui::End();
}

DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input)
{
    if (input == PegFormat::PC_DXT1)
        return DXGI_FORMAT_BC1_UNORM; //DXT1
    else if (input == PegFormat::PC_DXT3)
        return DXGI_FORMAT_BC2_UNORM; //DXT2/3
    else if (input == PegFormat::PC_DXT5)
        return DXGI_FORMAT_BC3_UNORM; //DXT4/5
    else
        THROW_EXCEPTION("Unknown or unsupported format '{}' passed to PegFormatToDxgiFormat()", input);
}