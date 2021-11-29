#pragma once
#include "common/Typedefs.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "IDocument.h"
#include "imgui.h"
#include <vector>

class TextureDocument final : public IDocument
{
public:
    TextureDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer);
    ~TextureDocument();

    void Update(GuiState* state) override;
    void Save(GuiState* state) override;

private:
    void PickPegExportFolder(GuiState* state);
    void PickPegImportTexture();

    string Filename;
    string ParentName;
    string VppName;
    string ExtractionPath;
    string CpuFilePath;
    string GpuFilePath;
    PegFile10 Peg;
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
    PegExtractType ExtractType = PegExtractType::All;
    u32 ExtractIndex = 0;
    u32 ImportIndex = 0;
};