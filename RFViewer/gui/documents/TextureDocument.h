#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"
#include <span>

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
//Convert PegFormat enum to string
string PegFormatToString(PegFormat input);

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
    static f32 imageViewSizeMultiplier = 0.85f;
    //ImGui::SliderFloat("Image view multiplier", &imageViewSizeMultiplier, 0.05f, 1.0f);

    const f32 columnZeroWidth = 300.0f;
    ImGui::Columns(2);
    ImGui::ColorEdit4("Background color", (float*)&data->imageBackground, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::SetColumnWidth(0, columnZeroWidth);

    //Texture entry list

    //Save cursor y at start of list so we can align the image column to it
    f32 baseY = ImGui::GetCursorPosY();
    //Calculate total height of texture list. Used if less than the limit to avoid having a bunch of empty space
    f32 entryHeight = ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.0f);
    f32 listHeightTotal = data->Peg.Entries.size() * entryHeight;

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_IMAGES " Textures");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    if (ImGui::BeginChild("##EntryList", ImVec2(columnZeroWidth, std::min(listHeightTotal, ImGui::GetWindowHeight() / 2.0f))))
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

    //Info about the selected texture
    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_INFO_CIRCLE " Texture info");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    if (data->selectedIndex < data->Peg.Entries.size())
    {
        PegEntry10& entry = data->Peg.Entries[data->selectedIndex];
        gui::LabelAndValue("Name:", entry.Name);
        gui::LabelAndValue("Data offset:", std::to_string(entry.DataOffset));
        gui::LabelAndValue("Width:", std::to_string(entry.Width));
        gui::LabelAndValue("Height:", std::to_string(entry.Height));
        gui::LabelAndValue("Bitmap format:", PegFormatToString(entry.BitmapFormat));
        gui::LabelAndValue("Source width:", std::to_string(entry.SourceWidth));
        gui::LabelAndValue("Anim tiles width:", std::to_string(entry.AnimTilesWidth));
        gui::LabelAndValue("Anim tiles height:", std::to_string(entry.AnimTilesHeight));
        gui::LabelAndValue("Num frames:", std::to_string(entry.NumFrames));
        gui::LabelAndValue("Flags:", std::to_string((int)entry.Flags));
        gui::LabelAndValue("Filename offset:", std::to_string(entry.FilenameOffset));
        gui::LabelAndValue("Source height:", std::to_string(entry.SourceHeight));
        gui::LabelAndValue("Frame size:", std::to_string(entry.FrameSize));
    }
    else
    {
        ImGui::TextWrapped("%s No sub-texture selected. Click a texture in the above list to see info about it.", ICON_FA_EXCLAMATION_CIRCLE);
    }

    //Header info
    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_FILE_INVOICE " Header info");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    gui::LabelAndValue("Signature:", std::to_string(data->Peg.Signature));
    gui::LabelAndValue("Version:", std::to_string(data->Peg.Version));
    gui::LabelAndValue("Platform:", std::to_string(data->Peg.Platform));
    gui::LabelAndValue("Directory block size:", std::to_string(data->Peg.DirectoryBlockSize));
    gui::LabelAndValue("Data block size:", std::to_string(data->Peg.DataBlockSize));
    gui::LabelAndValue("Num bitmaps", std::to_string(data->Peg.NumberOfBitmaps));
    gui::LabelAndValue("Flags:", std::to_string(data->Peg.Flags));
    gui::LabelAndValue("Total entries:", std::to_string(data->Peg.TotalEntries));
    gui::LabelAndValue("Align value:", std::to_string(data->Peg.AlignValue));


    //Draw the selected image in column 1 (to the right of the texture list and info)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
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


    //Todo: General:
        //Todo: Texture export

    //Todo: Editing pipeline:
        //Todo: File caching system like what OGE has. Make global cache. Equivalent purpose of OGE CacheManager (call this version Cache or something, manager isn't accurate name)
        //Todo: Project system. Tracks changes and has it's own cache which is preferred over the global cache, holds edited files
        //Todo: Texture import + saving
        //Todo: Auto update .str2_pc which holds edited textures
        //Todo: Auto update .asm_pc files when .str2_pc files edited
        //Todo: Generate modinfo from changes. Put modinfo.xml and any files the mod needs in a folder ready for use with the MM

    ImGui::Columns(1);
    ImGui::End();
}

void TextureDocument_OnClose(GuiState* state, Document& doc)
{
    TextureDocumentData* data = (TextureDocumentData*)doc.Data;

    //Release DX11 resource views
    for (void* imageHandle : data->ImageTextures)
    {
        if (!imageHandle)
            continue;

        ID3D11ShaderResourceView* asSrv = (ID3D11ShaderResourceView*)imageHandle;
        asSrv->Release();
    }

    //Cleanup peg resources
    data->Peg.Cleanup();

    //Free cached cpu and gpu file bytes (these are the entire files so possibly quite large)
    delete data->CpuFileBytes.data();
    delete data->GpuFileBytes.data();

    //Free document data
    delete data;
}

DXGI_FORMAT PegFormatToDxgiFormat(PegFormat input)
{
    if (input == PegFormat::PC_DXT1)
        return DXGI_FORMAT_BC1_UNORM; //DXT1
    else if (input == PegFormat::PC_DXT3)
        return DXGI_FORMAT_BC2_UNORM; //DXT2/3
    else if (input == PegFormat::PC_DXT5)
        return DXGI_FORMAT_BC3_UNORM; //DXT4/5
    else if (input == PegFormat::PC_8888)
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    else
        THROW_EXCEPTION("Unknown or unsupported format '{}' passed to PegFormatToDxgiFormat()", input);
}

string PegFormatToString(PegFormat input)
{
    switch (input)
    {
    case PegFormat::None:
        return "None";
    case PegFormat::BM_1555:
        return "BM_1555";
    case PegFormat::BM_888:
        return "BM_888";
    case PegFormat::BM_8888:
        return "BM_8888";
    case PegFormat::PS2_PAL4:
        return "PS2_PAL4";
    case PegFormat::PS2_PAL8:
        return "PS2_PAL8";
    case PegFormat::PS2_MPEG32:
        return "PS2_MPEG32";
    case PegFormat::PC_DXT1:
        return "PC_DXT1";
    case PegFormat::PC_DXT3:
        return "PC_DXT3";
    case PegFormat::PC_DXT5:
        return "PC_DXT5";
    case PegFormat::PC_565:
        return "PC_565";
    case PegFormat::PC_1555:
        return "PC_1555";
    case PegFormat::PC_4444:
        return "PC_4444";
    case PegFormat::PC_888:
        return "PC_888";
    case PegFormat::PC_8888:
        return "PC_8888";
    case PegFormat::PC_16_DUDV:
        return "PC_16_DUDV";
    case PegFormat::PC_16_DOT3_COMPRESSED:
        return "PC_16_DOT3_COMPRESSED";
    case PegFormat::PC_A8:
        return "PC_A8";
    case PegFormat::XBOX2_DXN:
        return "XBOX2_DXN";
    case PegFormat::XBOX2_DXT3A:
        return "XBOX2_DXT3A";
    case PegFormat::XBOX2_DXT5A:
        return "XBOX2_DXT5A";
    case PegFormat::XBOX2_CTX1:
        return "XBOX2_CTX1";
    case PegFormat::PS3_DXT5N:
        return "PS3_DXT5N";
    default:
        return "Invalid peg format code";
    }
}