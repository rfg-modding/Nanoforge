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
    void DrawXtblCategory(GuiState* state, Handle<XtblCategory> category, bool openByDefault = false);
    void DrawXtblNodeEntry(GuiState* state, Handle<XtblNode> node); //Draw xtbl node in entry list
    void DrawXtblNode(GuiState* state, Handle<XtblNode> node, Handle<XtblFile> xtbl, bool rootNode = false, const char* nameOverride = nullptr);

    string Filename;
    string ParentName;
    string VppName;
    bool InContainer;

    Handle<XtblFile> Xtbl = nullptr;
    Handle<XtblNode> selectedNode_ = nullptr;
};