#pragma once
#include "Common/Typedefs.h"
#include "Xtbl.h"
#include "rfg/PackfileVFS.h"
#include <vector>
#include <optional>

class XtblManager
{
public:
    //Initialize XtblManager with any data it needs
    void Init(PackfileVFS* packfileVFS) { TRACE(); packfileVFS_ = packfileVFS; initialized_ = true; }
    //Parse an xtbl if it hasn't already been parsed
    bool ParseXtbl(const string& vppName, const string& xtblName);
    //Get xtlb file if it's already been parsed
    Handle<XtblFile> GetXtbl(const string& vppName, const string& xtblName);
    //Get an xtbl file. Will parse the file first if it hasn't been parsed.
    Handle<XtblFile> GetOrCreateXtbl(const string& vppName, const string& xtblName);
    //Returns true if ready for use
    bool Ready() { return initialized_; }

    //A set of xtbl files that are all in the same vpp_pc
    struct XtblGroup
    {
        string VppName;
        std::vector<Handle<XtblFile>> Files;
    };
    //Get the group if it exists
    Handle<XtblGroup> GetGroup(const string& vppName);
    //Add the group if it doesn't exist. Return it
    Handle<XtblGroup> AddGroup(const string& vppName);

    //Groups of xtbl files that are in the same vpp_pc. Xtbls are only parsed when requested
    std::vector<Handle<XtblGroup>> XtblGroups;

private:
    PackfileVFS* packfileVFS_ = nullptr;
    bool initialized_ = false;
};