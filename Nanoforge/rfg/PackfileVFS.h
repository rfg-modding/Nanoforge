#pragma once
#include "FileHandle.h"
#include "util/TaskScheduler.h"
#include <RfgTools++\formats\packfiles\Packfile3.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\formats\asm\AsmFile5.h>
#include <vector>
#include <future>
#include <mutex>

//Enum used internally by PackfileVFS during file searches
enum class SearchType
{
    Direct, //Find filenames that exactly match search string
    AnyStart, //Find targets that end with the search string
    AnyEnd //Find starts that start with the search string
};

class Project;

//Interface for interacting with RFG packfiles and their contents
class PackfileVFS
{
public:
    void Init(const string& packfileFolderPath, Project* project);

    //Scans metadata of all vpps in the data folder and loads the global file cache
    void ScanPackfilesAndLoadCache();

    //Gets files based on the provided search pattern. Searches str2_pc files if recursive is true.
    std::vector<FileHandle> GetFiles(const std::vector<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const std::initializer_list<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const string& filter, bool recursive, bool oneResultPerFilter = false);
    //Overload that only searches in a single packfile
    std::vector<FileHandle> GetFiles(const string& packfileName, const string& filter, bool recursive, bool oneResultPerFilter = false);
    //Extract a file using it's path. E.g. "humans.vpp_pc/rfg_PC.str2_pc/mason_head.cpeg_pc". Caller must free span memory if successful
    std::optional<std::span<u8>> GetFileBytes(const string& filePath);
    std::optional<string> GetFileString(const string& filePath);

    //Todo: Ensure these stay valid if we start changing packfiles_ after init. May want some handle class that can return packfiles
    //Attempt to get a packfile. Returns nullptr if it fails to find the packfile
    Packfile3* GetPackfile(const std::string_view name);
    //Get a container packfile (a .str2_pc file that's inside a .vpp_pc). Note: Caller must call container Cleanup() function and free it themselves
    Packfile3* GetContainer(const std::string_view name, const std::string_view parentName);

    //If true this class is ready for use by guis / other code
    bool Ready() const { return ready_; }

    std::vector<Packfile3> packfiles_ = {};

private:
    //Check if target string is a search match based on the search filter and type
    bool CheckSearchMatch(s_view target, s_view filter, SearchType searchType = SearchType::Direct);

    //The current project
    Project* project_ = nullptr;
    //RFG data folder path
    std::string packfileFolderPath_;
    //If true this class is ready for use by guis / other code
    bool ready_ = false;
};