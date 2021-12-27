#pragma once
#include "common/Typedefs.h"
#include "common/String.h"

class GuiState;

class IDocument
{
public:
    virtual ~IDocument() {}
    virtual void Update(GuiState* state) = 0;
    virtual void Save(GuiState* state) = 0;
    //Called when the user clicks the close button.
    virtual void OnClose(GuiState* state) = 0;
    //Returns true if the document can be closed immediately. Used to ensure worker threads are stopped before deleting the document.
    virtual bool CanClose() = 0;

    string Title;
    bool FirstDraw = true;
    bool Open = true;
    bool UnsavedChanges = false;
    //Set to true if document is closed with unsaved changes and "Don't save changes" is selected.
    //Used for documents that need to do extra cleanup in their destructor
    bool ResetOnClose = false;
    bool NoWindowPadding = false; //Window padding is disabled if true. Useful for documents with viewports so they can be flush with the window border.
};