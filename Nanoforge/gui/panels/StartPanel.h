#pragma once
#include "common/Typedefs.h"
#include "gui/IGuiPanel.h"
#include "util/TaskScheduler.h"

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
    bool openProjectRequested_ = false;
    bool openRecentProjectRequested_ = false;
    string openRecentProjectRequestData_;

    bool dataPathValid_ = false;
    bool needDataPathParse_ = false;
    string missingVppName_; //Used in data folder path validation
    Handle<Task> dataFolderParseTask_ = Task::Create("Parse data folder");
};
