#pragma once
#include "common/Typedefs.h"
#include "common/String.h"

class GuiState;

const string& GetChangelogText();
void DrawChangelog(GuiState* state);