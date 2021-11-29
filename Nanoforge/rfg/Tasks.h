#pragma once
#include "gui/GuiState.h"
#include "rfg/Localization.h"
#include "application/Config.h"
#include "util/TaskScheduler.h"
#include "util/Profiler.h"
#include "rfg/TextureIndex.h"
#include "rfg/PackfileVFS.h"
#include "Log.h"
#include <future>

void DataFolderParseTask(Handle<Task> task, GuiState* state)
{
    PROFILER_FUNCTION();
    TRACE();
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);

    //Scan contents of packfiles
    state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
    auto dataPath = state->Config->GetVariable("Data path");
    state->PackfileVFS->Init(std::get<string>(dataPath->Value), state->CurrentProject);
    state->PackfileVFS->ScanPackfilesAndLoadCache();
    Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());

    //Load localization strings from rfglocatext files
    state->Localization->LoadLocalizationData();

    //Load texture search data
    state->TextureSearchIndex->Load();

    state->ClearStatus();
}