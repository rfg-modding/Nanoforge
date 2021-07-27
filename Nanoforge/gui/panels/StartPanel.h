#pragma once
#include "common/Typedefs.h"
#include "gui/IGuiPanel.h"

class StartPanel : public IGuiPanel
{
public:
    StartPanel();
    ~StartPanel();

    void Update(GuiState* state, bool* open) override;

private:
    void DrawDataPathSelector(GuiState* state);
    void DrawRecentProjectsList(GuiState* state);

    bool showSettingsWindow_ = false;
    bool showNewProjectWindow_ = false;
    bool dataPathValid_ = false;
    string missingVppName_; //Used in data folder path validation
};
