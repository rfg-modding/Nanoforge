#pragma once
#include "gui/GuiState.h"
#include "Log.h"
#include <future>

void WorkerThread(GuiState* state, bool reload)
{
    state->SetStatus(ICON_FA_SYNC " Waiting for init signal", Working);

    //We only need to scan the packfiles once. Packfile data is independent from our current territory
    if (!reload)
    {
        state->FileTreeLocked = true;
        state->FileTreeNeedsRegen = true;
        //Scan contents of packfiles
        state->SetStatus(ICON_FA_SYNC " Scanning packfiles", Working);
        state->PackfileVFS->ScanPackfilesAndLoadCache();
        Log->info("Loaded {} packfiles", state->PackfileVFS->packfiles_.size());
        state->FileTreeLocked = false;
    }
    state->ClearStatus();
}