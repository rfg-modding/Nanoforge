#pragma once
#include "FileHandle.h"
#include "FileCache.h"
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
    void Init(const string& packfileFolderPath);
    ~PackfileVFS();

    //Scans metadata of all vpps in the data folder and loads the global file cache
    void ScanPackfilesAndLoadCache();
    //Todo: Add support for C&C vpp_pc files. Supports C&C str2_pc files since they're small enough that full extract is reasonable
    //Gets files based on the provided search pattern. Searches str2_pc files if recursive is true. 
    //NOTE: Does not support vpp_pc files which are compressed + condensed (does support str2_pc files with those flags)
    std::vector<FileHandle> GetFiles(const std::vector<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const std::initializer_list<string>& searchFilters, bool recursive, bool oneResultPerFilter = false);
    std::vector<FileHandle> GetFiles(const string& filter, bool recursive, bool oneResultPerFilter = false);

    //Todo: Ensure these stay valid if we start changing packfiles_ after init. May want some handle class that can return packfiles
    //Attempt to get a packfile. Returns nullptr if it fails to find the packfile
    Packfile3* GetPackfile(const string& name);
    //Get a container packfile (a .str2_pc file that's inside a .vpp_pc). Note: Caller must call container Cleanup() function and free it themselves
    Packfile3* GetContainer(const string& name, const string& parentName);
    //If true this class is ready for use by guis / other code
    bool Ready() const { return ready_; }

    //Gets the path of a file in the cache. The file will be extracted and cached if it's not already cached. Arguments:
    //packfileName: the name of the .vpp_pc file the target file is in
    //filename1: Either the target file or the str2_pc file that contains it
    //filename2: Either the target name or an empty string ""
    string GetFile(const string& packfileName, const string& filename1, const string& filename2 = "");
    //Adds file to global cache. Arguments follow same rules as ::GetFile()
    void AddFileToCache(const string& packfileName, const string& filename1, const string& filename2);

    std::vector<Packfile3> packfiles_ = {};

private:
    //Check if target string is a search match based on the search filter and type
    bool CheckSearchMatch(s_view target, s_view filter, SearchType searchType = SearchType::Direct);
    

    //Global file cache
    FileCache globalFileCache_;
    //RFG data folder path
    std::string packfileFolderPath_;
    //Global file cache path. Extracted files are put in this folder to avoid repeat extractions
    string globalCachePath_ = ".\\Cache\\";
    //If true this class is ready for use by guis / other code
    bool ready_ = false;
};