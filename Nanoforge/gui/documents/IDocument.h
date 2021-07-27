#pragma once
#include "common/Typedefs.h"
#include <memory>

class GuiState;

class IDocument
{
public:
    virtual ~IDocument() {}
    virtual void Update(GuiState* state) = 0;
    virtual void Save(GuiState* state) = 0;

    string Title;
    bool FirstDraw = true;
    bool Open = true;
    bool UnsavedChanges = false;
    //Set to true if document is closed with unsaved changes and "Don't save changes" is selected.
    //Used for documents that need to do extra cleanup in their destructor
    bool ResetOnClose = false;
};