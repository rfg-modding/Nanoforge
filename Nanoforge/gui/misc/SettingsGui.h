#pragma once
#include "common/Typedefs.h"

class Config;
class ImGuiFontManager;

void DrawSettingsGui(bool* open, Config* config, ImGuiFontManager* fonts);