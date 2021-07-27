#pragma once
#include "common/Typedefs.h"

class Project;
class Config;
class GuiState;

//Draw window used to create a new Nanoforge project
bool DrawNewProjectWindow(bool* open, Project* project, Config* config);
//Modal popup seen when packaging a mod
void DrawModPackagingPopup(bool* open, GuiState* state);
//Open folder selector and try to open a new nanoforge project
bool TryOpenProject(Project* project, Config* config);