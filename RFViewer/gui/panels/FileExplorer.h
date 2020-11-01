#pragma once
#include "gui/GuiState.h"
#include "common/filesystem/Path.h"
#include "Log.h"
#include "common/timing/Timer.h"
#include "file_explorer/FileExplorerNode.h"

//Tree for RFG files
extern std::vector<FileExplorerNode> FileExplorer_FileTree;
//The last node to be clicked in the file explorer
extern FileExplorerNode* FileExplorer_SelectedNode;

//Note: This update function is in a .cpp since we already need it for the extern vars above anyway
//Per-frame update function of the file explorer. Draws the file explorer node tree
void FileExplorer_Update(GuiState* state, bool* open);