#include "PropertyPanelContent.h"
#include "gui/panels/FileExplorer.h"

void DrawPackfileData(GuiState* state, Packfile3* packfile)
{
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
    if (!FileExplorer_SelectedNode)
        return;

    //Cache packfile, only find again if selected node changes
    static FileExplorerNode* lastSelectedNode = nullptr;
    static Packfile3* packfile = nullptr;
    if (FileExplorer_SelectedNode != lastSelectedNode)
    {
        lastSelectedNode = FileExplorer_SelectedNode;
        packfile = state->PackfileVFS->GetPackfile(FileExplorer_SelectedNode->Filename);
    }

    //Draw packfile data if we've got it
    if (packfile)
        DrawPackfileData(state, packfile);
    else //Else draw an error message
        ImGui::Text("%s Failed to get packfile info.", ICON_FA_EXCLAMATION_CIRCLE);
}

void PropertyPanel_Str2Content(GuiState* state)
{
    if (!FileExplorer_SelectedNode)
        return;

    //Cache container, only find again if selected node changes
    static FileExplorerNode* lastSelectedNode = nullptr;
    static Packfile3* container = nullptr;
    if (FileExplorer_SelectedNode != lastSelectedNode)
    {
        lastSelectedNode = FileExplorer_SelectedNode;
        
        //Containers aren't stored long term by the packfileVFS so we must delete it ourselves once done with it
        if (container)
            delete container;

        container = state->PackfileVFS->GetContainer(FileExplorer_SelectedNode->Filename, FileExplorer_SelectedNode->ParentName);
        if (!container)
            return;
    }

    //Draw container data if we've got it
    if (container)
        DrawPackfileData(state, container);
    else //Else draw an error message
        ImGui::Text("%s Failed to get container info.", ICON_FA_EXCLAMATION_CIRCLE);
}