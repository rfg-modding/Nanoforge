#pragma once
#include "FileHandle.h"
#include <RfgTools++\formats\packfiles\Packfile3.h>
#include <RfgTools++\formats\zones\ZonePc36.h>
#include <RfgTools++\formats\asm\AsmFile5.h>
#include <vector>

//Enum used internally by PackfileVFS during file searches
enum class SearchType
{
    Direct, //Find filenames that exactly match search string
    AnyStart, //Find targets that end with the search string
    AnyEnd //Find starts that start with the search string
};

//Interface for interacting with RFG packfiles and their contents
class PackfileVFS
{
public:
    PackfileVFS(const string& packfileFolderPath) : packfileFolderPath_(packfileFolderPath) {}
    ~PackfileVFS();

    void ScanPackfiles();
    //Todo: Add support for C&C vpp_pc files. Supports C&C str2_pc files since they're small enough that full extract is reasonable
    //Gets files based on the provided search pattern. Searches str2_pc files if recursive is true. 
    //NOTE: Does not support vpp_pc files which are compressed + condensed (does support str2_pc files with those flags)
    std::vector<FileHandle> GetFiles(const std::vector<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const std::initializer_list<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const string& filter, bool recursive, bool oneResultPerFilter = false);

    //Todo: Ensure these stay valid if we start changing packfiles_ after init. May want some handle class that can return packfiles
    //Attempt to get a packfile. Returns nullptr if it fails to find the packfile
    Packfile3* GetPackfile(const string& name);

    std::vector<Packfile3> packfiles_ = {};

private:
    //Check if target string is a search match based on the search filter and type
    bool CheckSearchMatch(s_view target, s_view filter, SearchType searchType = SearchType::Direct);

    //Todo: Make externally configurable
    std::string packfileFolderPath_;

};