#pragma once
#include "gui/GuiState.h"
#include "rfg/Localization.h"
#include "application/Config.h"
#include "util/Profiler.h"
#include "Log.h"
#include <future>

static bool WorkerRunning = false;

void DataFolderParseThread(GuiState* state)
{
    PROFILER_FUNCTION();
    TRACE();
    WorkerRunning = true;
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);

    //Scan contents of packfiles
    state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
    auto dataPath = state->Config->GetVariable("Data path");
    state->PackfileVFS->Init(std::get<string>(dataPath->Value), state->CurrentProject);
    state->PackfileVFS->ScanPackfilesAndLoadCache();
    Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());

    //Load localization strings from rfglocatext files
    state->Localization->LoadLocalizationData();

    state->ClearStatus();
    WorkerRunning = false;
}