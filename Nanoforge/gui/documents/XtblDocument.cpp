#include "XtblDocument.h"
#include "Log.h"

void XtblDocument_DrawXtblNode(GuiState* state, Handle<Document> doc, XtblNode& node, bool rootNode = false);

void XtblDocument_Init(GuiState* state, Handle<Document> doc)
{
    XtblDocumentData* data = (XtblDocumentData*)doc->Data;

    bool result = state->Xtbls->ParseXtbl(data->VppName, data->Filename);
    if (!result)
    {
        Log->error("Failed to parse {}. Closing xtbl document.", data->Filename);
        doc->Open = false;
        return;
    }

    auto optional = state->Xtbls->GetXtbl(data->VppName, data->Filename);
    if (!optional)
    {
        Log->error("Failed to get {} from XtblManager. Closing xtbl document.", data->Filename);
        doc->Open = false;
        return;
    }

    data->Xtbl = optional.value();
}

void XtblDocument_Update(GuiState* state, Handle<Document> doc)
{
    if (!ImGui::Begin(doc->Title.c_str(), &doc->Open))
    {
        ImGui::End();
        return;
    }

    XtblDocumentData* data = (XtblDocumentData*)doc->Data;
    Handle<XtblFile> xtbl = data->Xtbl;

    const f32 columnZeroWidth = 300.0f;
    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnZeroWidth);

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_CODE " Entries");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    //Save cursor y at start of list so we can align the entry
    f32 baseY = ImGui::GetCursorPosY();
    //Calculate total height of entry list. Used if less than the limit to avoid having a bunch of empty space
    f32 entryHeight = ImGui::GetFontSize() + (ImGui::GetStyle().FramePadding.y * 2.0f);
    f32 listHeightTotal = xtbl->Entries.size() * entryHeight;

    //Todo: Organize entries by category rather than showing them as one big list
    //Xtbl entry list
    if (ImGui::BeginChild("##EntryList"))
    {
        for (u32 i = 0; i < xtbl->Entries.size(); i++)
        {
            XtblNode& entry = xtbl->Entries[i];
            bool selected = data->SelectedIndex == i;

            //Try to get <Name></Name> value
            string name = entry.Name;
            auto nameValue = entry.GetSubnodeValue("Name");
            if (nameValue)
                name = nameValue.value();

            string selectableName = fmt::format("{}##{}", name, i);

            if (ImGui::Selectable(selectableName.c_str(), selected))
            {
                data->SelectedIndex = i;
            }
        }
        ImGui::EndChild();
    }

    //Draw the selected entry values in column 1 (to the right of the entry list)
    ImGui::NextColumn();
    ImGui::SetCursorPosY(baseY);
    if (xtbl->Entries.size() != 0 && ImGui::BeginChild("##EntryView", ImVec2(ImGui::GetColumnWidth(), ImGui::GetWindowHeight())))
    {
        if (data->SelectedIndex >= xtbl->Entries.size())
            data->SelectedIndex = 0;

        XtblNode& entry = xtbl->Entries[data->SelectedIndex];
        for(auto& subnode : entry.Subnodes)
            XtblDocument_DrawXtblNode(state, doc, subnode, true);

        ImGui::EndChild();
    }

    ImGui::Columns(1);
    ImGui::End();
}

void XtblDocument_OnClose(GuiState* state, Handle<Document> doc)
{
    XtblDocumentData* data = (XtblDocumentData*)doc->Data;

    //Free document data
    delete data;
}

void XtblDocument_DrawXtblNode(GuiState* state, Handle<Document> doc, XtblNode& node, bool rootNode)
{
    string name = node.Name;
    if (rootNode) //Try to get <Name></Name> value if this is a root node and use that as the node name
    {
        auto nameValue = node.GetSubnodeValue("Name");
        if (nameValue)
            name = nameValue.value();
    }

    if (node.HasSubnodes())
    {
        if (ImGui::TreeNode(name.c_str()))
        {
            for (auto& subnode : node.Subnodes)
                XtblDocument_DrawXtblNode(state, doc, subnode);

            ImGui::TreePop();
        }
    }
    else
    {
        //Todo: Get description for each value and display the correct imgui element to edit that value (e.g. InputFloat for floats, InputText for strings)
        gui::LabelAndValue(name + ":", node.Value);
    }
}