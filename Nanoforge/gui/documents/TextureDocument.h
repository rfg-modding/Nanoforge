#pragma once
#include "common/Typedefs.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "IDocument.h"
#include "imgui.h"
#include <vector>


class TextureDocument final : public IDocument
{
public:
    TextureDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer);
    ~TextureDocument();

    void Update(GuiState* state) override;

private:
    void PickPegExportFolder(GuiState* state);
    void PickPegImportTexture(GuiState* state);

    string Filename;
    string ParentName;
    string VppName;
    string ExtractionPath;
    PegFile10 Peg;
    string CpuFilePath;
    string GpuFilePath;
    std::vector<ImTextureID> ImageTextures;
    bool InContainer;

    //Ui state
    //If true gpu resource creation failed for the selected texture
    bool CreateFailed = false;
    u32 SelectedIndex = 0;
    //Todo: Add checkered background option
    //Image column background color
    ImVec4 ImageBackground = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];

    //Extract state
    enum PegExtractType { All, SingleFile };
    PegExtractType ExtractType;
    u32 ExtractIndex;
    u32 ImportIndex;
};