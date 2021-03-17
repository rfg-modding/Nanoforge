#pragma once
#include "common/Typedefs.h"
#include <memory>

class GuiState;

class IDocument
{
public:
    virtual ~IDocument() {}
    virtual void Update(GuiState* state) = 0;

    bool Open() { return open_; }

    string Title;
    bool FirstDraw = true;

protected:
    bool open_ = true;
};