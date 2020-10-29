#pragma once
#include "common/Typedefs.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include <span>
#include <imgui.h>

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
    bool CreateFailed = false;
    u32 SelectedIndex = 0;
    //Todo: Add checkered background option
    //Image column background color
    ImVec4 ImageBackground = ImGui::GetStyle().Colors[ImGuiCol_ChildBg];

    //Extract state
    enum PegExtractType { All, SingleFile };
    PegExtractType ExtractType;
    u32 ExtractIndex;
};