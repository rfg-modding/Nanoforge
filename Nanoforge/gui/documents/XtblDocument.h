#pragma once
#include "common/Typedefs.h"
#include "IDocument.h"
#include "rfg/xtbl/Xtbl.h"
#include <vector>

class XtblManager;

class XtblDocument final : public IDocument
{
public:
    XtblDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer, Handle<IXtblNode> startingNode = nullptr);
    ~XtblDocument();

    void Update(GuiState* state) override;

    Handle<IXtblNode> SelectedNode = nullptr;
private:
    void DrawXtblCategory(Handle<XtblCategory> category, bool openByDefault = false);
    void DrawXtblNodeEntry(Handle<IXtblNode> node); //Draw xtbl node in entry list
    //Void xtbl to project cache
    void Save();

    string filename_;
    string parentName_;
    string vppName_;
    bool inContainer_;

    Handle<XtblFile> xtbl_ = nullptr;
    XtblManager* xtblManager_ = nullptr;
    GuiState* state_ = nullptr;
};