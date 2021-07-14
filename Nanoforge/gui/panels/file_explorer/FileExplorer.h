#pragma once
#include "common/Typedefs.h"
#include "gui/GuiState.h"
#include "gui/IGuiPanel.h"
#include "common/filesystem/Path.h"
#include "Log.h"
#include "common/timing/Timer.h"
#include "FileExplorerNode.h"
#include <vector>
#include <regex>
#include <future>

//File explorer gui that lets users navigate packfiles and their subfiles without needing manually unpack them
class FileExplorer : public IGuiPanel
{
public:
    FileExplorer();
    ~FileExplorer();

    void Update(GuiState* state, bool* open) override;

private:
    //Update search bar and check which nodes meet search term
    void UpdateSearchBar(GuiState* state);
    //Ran by the search thread which runs file explorer searches in the background
    void SearchThread();
    //Generates file tree. Done once at startup and when files are added/removed (very rare)
    //Pre-generating data like this makes rendering and interacting with the file tree simpler
    void GenerateFileTree(GuiState* state);
    //Draws and imgui tree node for the provided FileExplorerNode
    void DrawFileNode(GuiState* state, FileExplorerNode& node);
    //Called when file is single clicked from the explorer. Used to update property panel content
    void SingleClickedFile(GuiState* state, FileExplorerNode& node);
    //Called when a file is double clicked in the file explorer. Attempts to open a tool/viewer for the provided file
    void DoubleClickedFile(GuiState* state, FileExplorerNode& node);
    //Returns true if extension is a format that is supported by Nanoforge. This includes partial support (viewing only)
    bool FormatSupported(const string& ext);
    //Updates node search results. Stores result in FileExplorerNode::MatchesSearchTerm and FileExplorerNode::AnyChildNodeMatchesSearchTerm
    void UpdateNodeSearchResultsRecursive(FileExplorerNode& node);
    //Get icon for node based on node type and extension
    string GetNodeIcon(FileExplorerNode& node);
    //Get color of node based on file extension
    ImVec4 GetNodeColor(FileExplorerNode& node);
    //Returns true if the node text matches the current search term
    bool DoesNodeFitSearch(FileExplorerNode& node);
    //Returns true if any of the child nodes of node match the current search term
    bool AnyChildNodesFitSearch(FileExplorerNode& node);

    //Tree for RFG files
    std::vector<FileExplorerNode> FileTree;

    //Data used for file tree searches
    //Buffer used by search text bar
    string SearchTerm = "";
    //Search string that's passed directly to string comparison functions. Had * at start or end of search term removed
    string SearchTermPatched = "";
    //If true, search term changed and need to recheck all nodes for search match
    bool SearchChanged = true;
    enum FileSearchType
    {
        Match,
        MatchStart,
        MatchEnd
    };
    FileSearchType SearchType = FileSearchType::Match;
    bool HideUnsupportedFormats = false;
    bool RegexSearch = false;
    bool CaseSensitive = false;
    std::regex SearchRegex {""};
    bool HadRegexError = false;
    string LastRegexError;
    string VppName;
    bool FileTreeNeedsRegen = true;

private:

    //Search thread data. Searches are run on another thread to avoid lagging the main thread.
    Timer searchChangeTimer_ { true };
    std::mutex searchThreadMutex_;
    std::future<void> searchThreadFuture_;
    bool runningSearchThread_ = false;
    bool searchThreadForceStop_ = false;
};