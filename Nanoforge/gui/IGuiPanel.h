#pragma once
#include "GuiState.h"


//Interface sub-guis must implement to be used by MainGui
class IGuiPanel
{
public:
    virtual ~IGuiPanel() {}
    virtual void Update(GuiState* state, bool* open) = 0;

    bool Open = true;
    string MenuPos; //String representing position in main menu bar
    bool FirstDraw = true;

protected:

};