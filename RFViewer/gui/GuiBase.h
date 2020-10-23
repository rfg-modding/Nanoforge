#pragma once
#include "GuiState.h"

/*
    Interface sub-guis should provide to be used by MainGui. Not using the standard abstract base class method 
    since it isn't necessary yet and it should be easy to switch to if necessary.
*/

//Function signature for gui update functions
using GuiUpdateFunc = void(GuiState* state, bool* open);

class GuiPanel
{
public:
    GuiUpdateFunc* Update = nullptr;
    string MenuPos;
    bool Open = false;
};