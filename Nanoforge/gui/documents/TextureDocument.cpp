#include "TextureDocument.h"
#include "Log.h"
#include "gui/GuiState.h"
#include "render/backend/DX11Renderer.h"
#include "common/string/String.h"
#include "gui/util/WinUtil.h"
#include "PegHelpers.h"
#include "util/RfgUtil.h"
#include "application/project/Project.h"
#include "util/Profiler.h"

TextureDocument::TextureDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);

    //Get path to cpu file and gpu file in cache
    auto maybeCpuFilePath = InContainer ?
        state->PackfileVFS->GetFilePath(VppName, ParentName, Filename) :
        state->PackfileVFS->GetFilePath(VppName, Filename);
    auto maybeGpuFilePath = InContainer ?
        state->PackfileVFS->GetFilePath(VppName, ParentName, gpuFileName) :
        state->PackfileVFS->GetFilePath(VppName, gpuFileName);

    //Error handling for when cpu or gpu file aren't found
    if (!maybeCpuFilePath)
    {
        Log->error("Texture document constructor encountered error! Failed to find texture cpu file: \"{}\" in \"{}\"", Filename, InContainer ? VppName + "/" + ParentName : VppName);
        Open = false;
        return;
    }
    if (!maybeGpuFilePath)
    {
        Log->error("Texture document constructor encountered error! Failed to find texture gpu file: \"{}\" in \"{}\"", gpuFileName, InContainer ? VppName + "/" + ParentName : VppName);
        Open = false;
        return;
    }

    CpuFilePath = maybeCpuFilePath.value();
    GpuFilePath = maybeGpuFilePath.value();

    //Parse peg
    BinaryReader cpuFileReader(CpuFilePath);
    BinaryReader gpuFileReader(GpuFilePath);
    Peg.Read(cpuFileReader);

    //Fill texture list with nullptrs. When a sub-image of the peg is opened it'll be rendered from this list.
    //If the index of the sub-image is a nullptr then it'll be loaded from the peg gpu file
    for (u32 i = 0; i < Peg.Entries.size(); i++)
        ImageTextures.push_back(nullptr);
}

TextureDocument::~TextureDocument()
{
    //Release DX11 resource views
    for (void* imageHandle : ImageTextures)
    {
        if (!imageHandle)
            continue;

        ID3D11ShaderResourceView* asSrv = (ID3D11ShaderResourceView*)imageHandle;
        asSrv->Release();
    }

    //Cleanup peg resources
    Peg.Cleanup();
}

void TextureDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Controls max size of selected image in gui relative to the size of it's column
    static f32 imageViewSizeMultiplier = 0.85f;
    //ImGui::SliderFloat("Image view multiplier", &imageViewSizeMultiplier, 0.05f, 1.0f);

    const f32 columnZeroWidth = 300.0f;
    ImGui::Columns(2);
    ImGui::ColorEdit4("Background color", (float*)&ImageBackground, ImGuiColorEditFlags_NoInputs);
    ImGui::Separator();
    ImGui::SetColumnWidth(0, columnZeroWidth);


    //Save cursor y at start of list so we can align the image column to it
    f32 baseY = ImGui::GetCursorPosY();
    //Calculate total height of texture list. Used if less than the limit to avoid having a bunch of empty space
    f32 entryHeight = ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.0f);
    f32 listHeightTotal = Peg.Entries.size() * entryHeight;

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_IMAGES " Textures");
    state->FontManager->FontL.Pop();
    ImGui::Separator();
    if (ImGui::Button("Export all..."))
    {
        Log->info("Exporting all textures from {}", Filename);
        ExtractType = PegExtractType::All;
        ExtractIndex = 0;
        PickPegExportFolder(state);
    }

    //Texture entry list
    if (ImGui::BeginChild("##EntryList", ImVec2(columnZeroWidth, std::min(listHeightTotal, ImGui::GetWindowHeight() / 2.0f))))
    {
        for (u32 i = 0; i < Peg.Entries.size(); i++)
        {
            PegEntry10& entry = Peg.Entries[i];
            bool selected = SelectedIndex == i;
            if (ImGui::Selectable(entry.Name.c_str(), selected))
            {
                SelectedIndex = i;
                CreateFailed = false; //Reset so the next texture can attempt at least one load
            }
            //Right click context menu
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::Button("Export..."))
                {
                    Log->info("Exporting {} from {}", entry.Name, Filename);
                    ExtractType = PegExtractType::SingleFile;
                    ExtractIndex = i;
                    PickPegExportFolder(state);
                    ImGui::CloseCurrentPopup();
                }
                else if (ImGui::Button("Replace..."))
                {
                    if (state->CurrentProject->Loaded())
                    {
                        Log->info("Replacing {} in {}", entry.Name, Filename);
                        ImportIndex = i;
                        PickPegImportTexture(state);
                        ImGui::CloseCurrentPopup();
                    }
                    else
                    {
                        Log->error("You need to have a project open to edit files. You can open a project via File > Open project.");
                        ShowMessageBox("You need to have a project open to edit files. You can open an existing project or create a new one with `File > Open project` and `File > New project`", "Can't package mod", MB_OK);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndPopup();
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
    if (SelectedIndex < Peg.Entries.size())
    {
        PegEntry10& entry = Peg.Entries[SelectedIndex];
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
    gui::LabelAndValue("Signature:", std::to_string(Peg.Signature));
    gui::LabelAndValue("Version:", std::to_string(Peg.Version));
    gui::LabelAndValue("Platform:", std::to_string(Peg.Platform));
    gui::LabelAndValue("Directory block size:", std::to_string(Peg.DirectoryBlockSize));
    gui::LabelAndValue("Data block size:", std::to_string(Peg.DataBlockSize));
    gui::LabelAndValue("Num bitmaps", std::to_string(Peg.NumberOfBitmaps));
    gui::LabelAndValue("Flags:", std::to_string(Peg.Flags));
    gui::LabelAndValue("Total entries:", std::to_string(Peg.TotalEntries));
    gui::LabelAndValue("Align value:", std::to_string(Peg.AlignValue));


    //Draw the selected image in column 1 (to the right of the texture list and info)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImageBackground);
    if (Peg.Entries.size() != 0 && ImGui::BeginChild("##ImageView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        if (SelectedIndex >= Peg.Entries.size())
            SelectedIndex = 0;

        PegEntry10& entry = Peg.Entries[SelectedIndex];
        ImTextureID entryTexture = ImageTextures[SelectedIndex];
        if (!entryTexture && !CreateFailed)
        {
            BinaryReader gpuFileReader(GpuFilePath);
            Peg.ReadTextureData(gpuFileReader, entry);
            DXGI_FORMAT format = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat);
            ImTextureID id = state->Renderer->TextureDataToHandle(entry.RawData, format, entry.Width, entry.Height);
            if (!id)
            {
                Log->error("Failed to create texture resource for texture entry {} in peg file {}", entry.Name, Title);
                CreateFailed = true;
            }
            else
            {
                ImageTextures[SelectedIndex] = id;
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
}

void TextureDocument::Save(GuiState* state)
{
    if (state->CurrentProject->Loaded())
    {
        //Read all texture data from unedited gpu file
        BinaryReader gpuFileOriginal(GpuFilePath);
        Peg.ReadAllTextureData(gpuFileOriginal, false); //Read all texture data and don't overwrite edited files

        //Base output path relative to project root
        string pathBaseRelative = VppName + "\\";
        if (InContainer)
            pathBaseRelative += ParentName + "\\";

        //Absolute base output path
        string pathBaseAbsolute = state->CurrentProject->GetCachePath() + pathBaseRelative;

        //Create base path folders
        std::filesystem::create_directories(pathBaseAbsolute);

        //Full output path for cpu & gpu files
        string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(Filename);
        string cpuFilePathOut = pathBaseAbsolute + Filename;
        string gpuFilePathOut = pathBaseAbsolute + gpuFilename;

        //Create binary writers and output edited peg file
        BinaryWriter cpuFileOut(cpuFilePathOut);
        BinaryWriter gpuFileOut(gpuFilePathOut);
        Peg.Write(cpuFileOut, gpuFileOut);

        //Rescan project cache to see the files we just saved
        state->CurrentProject->RescanCache();

        //Add edit to project and resave the project file
        state->CurrentProject->AddEdit(FileEdit{ "TextureEdit", pathBaseRelative + Filename });
        state->CurrentProject->Save();

        Log->info("Saved \"{}\"", Filename);
    }
    else
    {
        Log->error("You need to have a project open to edit files. You can open a project via File > Open project.");
        ShowMessageBox("You need to have a project open to edit files. You can open an existing project or create a new one with `File > Open project` and `File > New project`", "Can't package mod", MB_OK);
    }
}

void TextureDocument::PickPegExportFolder(GuiState* state)
{
    //Open folder browser and return result
    auto result = OpenFolder();
    if (!result)
        return;

    //If result is valid export the texture(s)
    Log->info("Extract all window selection: \"{}\"", result.value());
    if (ExtractType == PegExtractType::All)
        PegHelpers::ExportAll(Peg, GpuFilePath, result.value() + "\\");
    else if (ExtractType == PegExtractType::SingleFile)
        PegHelpers::ExportSingle(Peg, GpuFilePath, SelectedIndex, result.value() + "\\");
}

void TextureDocument::PickPegImportTexture(GuiState* state)
{
    //Open file browser and return result
    auto result = OpenFile("dds");
    if (!result)
        return;

    if (std::filesystem::exists(result.value()))
    {
        //Import texture
        PegHelpers::ImportTexture(Peg, ImportIndex, result.value());

        //If there's a shader resource for the texture we're replacing then destroy it. Will be recreated by drawing code below
        void* imageHandle = ImageTextures[ImportIndex];
        if (imageHandle)
        {
            ID3D11ShaderResourceView* asSrv = (ID3D11ShaderResourceView*)imageHandle;
            asSrv->Release();
            ImageTextures[ImportIndex] = nullptr;
        }
        UnsavedChanges = true;
    }
    else
    {
        Log->error("Invalid path \"{}\" selected for texture import.", result.value());
    }
}
