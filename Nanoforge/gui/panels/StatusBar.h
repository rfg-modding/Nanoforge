#pragma once
#include "gui/GuiState.h"
#include "gui/IGuiPanel.h"

class StatusBar : public IGuiPanel
{
public:
    StatusBar();
    ~StatusBar();

    void Update(GuiState* state, bool* open) override;

private:

};