#pragma once
#include "common/Typedefs.h"
#include <vector>

//Data path for game vpps
extern string Settings_PackfileFolderPath;
//Name of the vpp_pc file that zone data is loaded from at startup
extern string Settings_TerritoryFilename;
//Most recently opened projects for main menu bar and welcome screen
extern std::vector<string> Settings_RecentProjects;

void Settings_Read();
void Settings_Write();
void Settings_AddRecentProjectPathUnique(const string& path);