#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/xtbl/Xtbl.h"
#include <vector>


class XtblDocument final : public IDocument
{
public:
    XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer);
    ~XtblDocument();

    void Update(GuiState* state) override;

private:
    void DrawXtblNode(GuiState* state, Handle<XtblNode> node, Handle<XtblFile> xtbl, bool rootNode = false);

    string Filename;
    string ParentName;
    string VppName;
    bool InContainer;

    Handle<XtblFile> Xtbl = nullptr;
    u32 SelectedIndex = 0;
};