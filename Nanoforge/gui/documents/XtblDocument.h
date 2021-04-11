#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/xtbl/Xtbl.h"
#include <vector>

class XtblManager;

class XtblDocument final : public IDocument
{
public:
    XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer);
    ~XtblDocument();

    void Update(GuiState* state) override;

private:
    void DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault = false);
    void DrawXtblNodeEntry(Handle<XtblNode> node); //Draw xtbl node in entry list
    void DrawXtblEntry(Handle<XtblDescription> desc, const char* nameOverride = nullptr, Handle<XtblNode> nodeOverride = nullptr);

    string filename_;
    string parentName_;
    string vppName_;
    bool inContainer_;

    Handle<XtblFile> xtbl_ = nullptr;
    Handle<XtblNode> selectedNode_ = nullptr;
    XtblManager* xtblManager_ = nullptr;
};