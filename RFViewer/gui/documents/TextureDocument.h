#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "RfgTools++/formats/textures/PegFile10.h"

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
};

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
    auto cpuFile = parent->ExtractSingleFile(data->Filename);
    auto gpuFile = parent->ExtractSingleFile(gpuFileName);
    if (!cpuFile)
    {
        Log->error("Failed to extract cpu file in TextureDocument_Init() for TextureDocument '{}'", data->Filename);
        return;
    }
    if (!gpuFile)
    {
        Log->error("Failed to extract cpu file in TextureDocument_Init() for TextureDocument '{}'", data->Filename);
        return;
    }

    //Todo: Extract the peg to the filesystem and interact with that copy instead of caching it's bytes. Caching bytes is a ram killer because some gpeg/gvbm files are huge
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
    ImGui::Text("Texture document");
    gui::LabelAndValue("Filename:", data->Filename);
    gui::LabelAndValue("Parent name:", data->ParentName);
    gui::LabelAndValue("Extraction path:", data->ExtractionPath);

    ImGui::End();
}