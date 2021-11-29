#pragma once
#include "gui/IGuiPanel.h"

class GuiState;

class StatusBar : public IGuiPanel
{
public:
    StatusBar();
    ~StatusBar();

    void Update(GuiState* state, bool* open) override;

private:

};