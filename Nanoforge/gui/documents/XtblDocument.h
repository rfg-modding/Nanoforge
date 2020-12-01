#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"

struct XtblDocumentData
{
    string Filename;
    string ParentName;
    string VppName;
    bool InContainer;

    Handle<XtblFile> Xtbl = nullptr;
    u32 SelectedIndex = 0;
};

void XtblDocument_Init(GuiState* state, Handle<Document> doc);
void XtblDocument_Update(GuiState* state, Handle<Document> doc);
void XtblDocument_OnClose(GuiState* state, Handle<Document> doc);