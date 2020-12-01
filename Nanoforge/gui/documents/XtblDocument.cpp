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
    
    gui::LabelAndValue("Name: ", xtbl->Name);
    gui::LabelAndValue("Num elements: ", std::to_string(xtbl->Entries.size()));
    gui::LabelAndValue("Num descriptions: ", std::to_string(xtbl->TableDescription.Subnodes.size()));
    gui::LabelAndValue("Element name: ", xtbl->TableDescription.Name);

    //Todo: Replace with list of entries in column on left and selected entry data in column to the right. Just like texture documents
    //Todo: Sort entries by category in entry list
    for (auto& entry : xtbl->Entries)
        XtblDocument_DrawXtblNode(state, doc, entry, true);

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
        gui::LabelAndValue(name + ":", node.Value);
    }
}