#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "TextureDocumentState.h"
#include "PegHelpers.h"
#include "util/RfgUtil.h"
#include "application/project/Project.h"

void TextureDocument_Init(GuiState* state, std::shared_ptr<Document> doc)
{
    //Get parent packfile
    TextureDocumentData* data = (TextureDocumentData*)doc->Data;

    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(data->Filename);

    //Parse peg and cache bytes for later manipulation
    data->CpuFilePath = data->InContainer ?
                         state->PackfileVFS->GetFile(data->VppName, data->ParentName, data->Filename) :
                         state->PackfileVFS->GetFile(data->VppName, data->Filename);
    data->GpuFilePath = data->InContainer ?
                         state->PackfileVFS->GetFile(data->VppName, data->ParentName, gpuFileName) :
                         state->PackfileVFS->GetFile(data->VppName, gpuFileName);

    BinaryReader cpuFileReader(data->CpuFilePath);
    BinaryReader gpuFileReader(data->GpuFilePath);
    data->Peg.Read(cpuFileReader, gpuFileReader);

    //Fill texture list with nullptrs. When a sub-image of the peg is opened it'll be rendered from this list.
    //If the index of the sub-image is a nullptr then it'll be loaded from the peg gpu file
    for (u32 i = 0; i < data->Peg.Entries.size(); i++)
    {
        data->ImageTextures.push_back(nullptr);
    }

    //Add an icon for png files 
    igfd::ImGuiFileDialog::Instance()->SetExtentionInfos(".png,.dds,.bmp,.jpg,.jpeg", ImVec4(1, 1, 1, 1.0), ICON_FA_FILE_IMAGE);
}

void TextureDocument_Update(GuiState* state, std::shared_ptr<Document> doc)
{
    if (!ImGui::Begin(doc->Title.c_str(), &doc->Open))
    {
        ImGui::End();
        return;
    }

    TextureDocumentData* data = (TextureDocumentData*)doc->Data;
    
    //Controls max size of selected image in gui relative to the size of it's column
    static f32 imageViewSizeMultiplier = 0.85f;
    //ImGui::SliderFloat("Image view multiplier", &imageViewSizeMultiplier, 0.05f, 1.0f);

    const f32 columnZeroWidth = 300.0f;
    ImGui::Columns(2);
    //Todo: Add confirmation when closing an edited file before saving it
    //Todo: Require a project to be opened to even open textures
    if (ImGui::Button("Save"))
    {
        //Read all texture data from unedited gpu file
        BinaryReader gpuFileOriginal(data->GpuFilePath);
        data->Peg.ReadAllTextureData(gpuFileOriginal, false); //Read all texture data and don't overwrite edited files

        //Base output path relative to project root
        string pathBaseRelative = data->VppName + "\\";
        if (data->InContainer)
            pathBaseRelative += data->ParentName + "\\";

        //Absolute base output path
        string pathBaseAbsolute = state->CurrentProject->GetCachePath() + pathBaseRelative;

        //Create base path folders
        std::filesystem::create_directories(pathBaseAbsolute);

        //Full output path for cpu & gpu files
        string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(data->Filename);
        string cpuFilePathOut = pathBaseAbsolute + data->Filename;
        string gpuFilePathOut = pathBaseAbsolute + gpuFilename;

        //Create binary writers and output edited peg file
        BinaryWriter cpuFileOut(cpuFilePathOut);
        BinaryWriter gpuFileOut(gpuFilePathOut);
        data->Peg.Write(cpuFileOut, gpuFileOut);

        //Rescan project cache to see the files we just saved
        state->CurrentProject->RescanCache();
        
        //Add edit to project and resave the project file
        state->CurrentProject->AddEdit(FileEdit{ "TextureEdit", pathBaseRelative + data->Filename });
        state->CurrentProject->Save();

        Log->info("Saved \"{}\"", data->Filename);
    }
    ImGui::ColorEdit4("Background color", (float*)&data->ImageBackground, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::SetColumnWidth(0, columnZeroWidth);


    //Save cursor y at start of list so we can align the image column to it
    f32 baseY = ImGui::GetCursorPosY();
    //Calculate total height of texture list. Used if less than the limit to avoid having a bunch of empty space
    f32 entryHeight = ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.0f);
    f32 listHeightTotal = data->Peg.Entries.size() * entryHeight;

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_IMAGES " Textures");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    if (ImGui::Button("Export all"))
    {
        Log->info("Exporting all textures from {}", data->Filename);
        //Since filter == 0 this window is a directory picker
        igfd::ImGuiFileDialog::Instance()->OpenDialog("PickPegExportFolder", "Choose folder", 0, ".");
        data->ExtractType = TextureDocumentData::PegExtractType::All;
        data->ExtractIndex = 0;
    }
    
    //Texture entry list
    if (ImGui::BeginChild("##EntryList", ImVec2(columnZeroWidth, std::min(listHeightTotal, ImGui::GetWindowHeight() / 2.0f))))
    {
        for (u32 i = 0; i < data->Peg.Entries.size(); i++)
        {
            PegEntry10& entry = data->Peg.Entries[i];
            bool selected = data->SelectedIndex == i;
            if (ImGui::Selectable(entry.Name.c_str(), selected))
            {
                data->SelectedIndex = i;
                data->CreateFailed = false; //Reset so the next texture can attempt at least one load
            }
            //Right click context menu
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::Button("Export"))
                {
                    Log->info("Exporting {} from {}", entry.Name, data->Filename);
                    //Since filter == 0 this window is a directory picker
                    igfd::ImGuiFileDialog::Instance()->OpenDialog("PickPegExportFolder", "Choose folder", 0, ".");
                    data->ExtractType = TextureDocumentData::PegExtractType::SingleFile;
                    data->ExtractIndex = i;
                    ImGui::CloseCurrentPopup();
                }
                else if (ImGui::Button("Replace"))
                {
                    Log->info("Replacing {} in {}", entry.Name, data->Filename);
                    igfd::ImGuiFileDialog::Instance()->OpenDialog("PickImportTexture", "Choose texture", ".dds", ".");
                    data->ImportIndex = i;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();
    }

    //Update file browser for picking a folder to extract files to if it's open
    if (igfd::ImGuiFileDialog::Instance()->FileDialog("PickPegExportFolder"))
    {
        if (igfd::ImGuiFileDialog::Instance()->IsOk)
        {
            Log->info("Extract all window selection: \"{}\"", igfd::ImGuiFileDialog::Instance()->GetCurrentPath());
            if (data->ExtractType == TextureDocumentData::PegExtractType::All)
            {
                PegHelpers::ExportAll(data->Peg, data->GpuFilePath, igfd::ImGuiFileDialog::Instance()->GetCurrentPath() + "\\");
            }
            else if (data->ExtractType == TextureDocumentData::PegExtractType::SingleFile)
            {
                PegHelpers::ExportSingle(data->Peg, data->GpuFilePath, data->SelectedIndex, igfd::ImGuiFileDialog::Instance()->GetCurrentPath() + "\\");
            }
        }
        igfd::ImGuiFileDialog::Instance()->CloseDialog("PickPegExportFolder");
    }
    //Update texture import file picker
    if (igfd::ImGuiFileDialog::Instance()->FileDialog("PickImportTexture"))
    {
        if (igfd::ImGuiFileDialog::Instance()->IsOk)
        {
            if (std::filesystem::exists(igfd::ImGuiFileDialog::Instance()->GetFilePathName()))
            {
                //Import texture
                PegHelpers::ImportTexture(data->Peg, data->ImportIndex, igfd::ImGuiFileDialog::Instance()->GetFilePathName());

                //If there's a shader resource for the texture we're replacing then destroy it. Will be recreated by drawing code below
                void* imageHandle = data->ImageTextures[data->ImportIndex];
                if (imageHandle)
                {
                    ID3D11ShaderResourceView* asSrv = (ID3D11ShaderResourceView*)imageHandle;
                    asSrv->Release();
                    data->ImageTextures[data->ImportIndex] = nullptr;
                }
            }
            else
            {
                Log->error("Invalid path \"{}\" selected for texture import.", igfd::ImGuiFileDialog::Instance()->GetFilePathName());
            }
        }
        igfd::ImGuiFileDialog::Instance()->CloseDialog("PickImportTexture");
    }

    //Info about the selected texture
    ImGui::Separator();
    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_INFO_CIRCLE " Texture info");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    if (data->SelectedIndex < data->Peg.Entries.size())
    {
        PegEntry10& entry = data->Peg.Entries[data->SelectedIndex];
        gui::LabelAndValue("Name:", entry.Name);
        gui::LabelAndValue("Data offset:", std::to_string(entry.DataOffset));
        gui::LabelAndValue("Width:", std::to_string(entry.Width));
        gui::LabelAndValue("Height:", std::to_string(entry.Height));
        gui::LabelAndValue("Bitmap format:", PegHelpers::PegFormatToString(entry.BitmapFormat));
        gui::LabelAndValue("Source width:", std::to_string(entry.SourceWidth));
        gui::LabelAndValue("Anim tiles width:", std::to_string(entry.AnimTilesWidth));
        gui::LabelAndValue("Anim tiles height:", std::to_string(entry.AnimTilesHeight));
        gui::LabelAndValue("Num frames:", std::to_string(entry.NumFrames));
        gui::LabelAndValue("Flags:", std::to_string((int)entry.Flags));
        gui::LabelAndValue("Filename offset:", std::to_string(entry.FilenameOffset));
        gui::LabelAndValue("Source height:", std::to_string(entry.SourceHeight));
        gui::LabelAndValue("Fps:", std::to_string(entry.Fps));
        gui::LabelAndValue("Mip levels:", std::to_string(entry.MipLevels));
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
    ImGui::PushStyleColor(ImGuiCol_ChildBg, data->ImageBackground);
    if (data->Peg.Entries.size() != 0 && ImGui::BeginChild("##ImageView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        if (data->SelectedIndex >= data->Peg.Entries.size())
            data->SelectedIndex = 0;
        
        PegEntry10& entry = data->Peg.Entries[data->SelectedIndex];
        ImTextureID entryTexture = data->ImageTextures[data->SelectedIndex];
        if (!entryTexture && !data->CreateFailed)
        {
            BinaryReader gpuFileReader(data->GpuFilePath);
            data->Peg.ReadTextureData(gpuFileReader, entry);
            DXGI_FORMAT format = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat);
            ImTextureID id = state->Renderer->TextureDataToHandle(entry.RawData, format, entry.Width, entry.Height);
            if (!id)
            {
                Log->error("Failed to create texture resource for texture entry {} in peg file {}", entry.Name, doc->Title);
                data->CreateFailed = true;
            }
            else
            {
                data->ImageTextures[data->SelectedIndex] = id;
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

    ImGui::Columns(1);
    ImGui::End();
}

void TextureDocument_OnClose(GuiState* state, std::shared_ptr<Document> doc)
{
    TextureDocumentData* data = (TextureDocumentData*)doc->Data;

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

    //Free document data
    delete data;
}