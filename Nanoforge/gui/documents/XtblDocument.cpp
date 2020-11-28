#include "XtblDocument.h"
#include "Log.h"

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

    ImGui::End();
}

void XtblDocument_OnClose(GuiState* state, Handle<Document> doc)
{
    XtblDocumentData* data = (XtblDocumentData*)doc->Data;

    //Free document data
    delete data;
}