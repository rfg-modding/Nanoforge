#pragma once
#include "common/Typedefs.h"

class Project;
class Config;

//Draw window used to create a new Nanoforge project
bool DrawNewProjectWindow(bool* open, Project* project, Config* config);