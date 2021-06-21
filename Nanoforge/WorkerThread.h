#pragma once
#include "gui/GuiState.h"
#include "Log.h"
#include <future>

void WorkerThread(GuiState* state)
{
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);

    //Scan contents of packfiles
    state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
    state->PackfileVFS->ScanPackfilesAndLoadCache();
    Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());

    //Load localization strings from rfglocatext files
    state->Localization->LoadLocalizationData();
    
    state->ClearStatus();
}