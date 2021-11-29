#include "XtblManager.h"
#include "Xtbl.h"
#include "Log.h"

void XtblManager::Init(PackfileVFS* packfileVFS)
{
    TRACE();
    packfileVFS_ = packfileVFS;
    initialized_ = true;
}

void XtblManager::ReloadXtbls()
{
    for(auto& group : XtblGroups)
        for (auto& xtbl : group->Files)
            xtbl->Reload(packfileVFS_);
}

bool XtblManager::ParseXtbl(const string& vppName, const string& xtblName)
{
    //Fail early if XtblManager hasn't been initialized
    if (!Ready())
    {
        Log->error("XtblManager::ParseXtbl() called before XtblManager instance was initialized! Cancelling parse.");
        return false;
    }

    //Return true if the xtbl was already parsed
    if (GetXtbl(vppName, xtblName))
        return true;

    //Get or create xtbl group + create new XtblFile
    Handle<XtblGroup> group = AddGroup(vppName);
    Handle<XtblFile> file = CreateHandle<XtblFile>();
    file->VppName = vppName;
    file->Name = xtblName;

    //Parse
    string path = fmt::format("{}/{}", vppName, xtblName);
    bool parseResult = file->Parse(path, packfileVFS_);

    //If parse successful add xtbl to group for it's vpp, else log an error message
    if (parseResult)
        group->Files.push_back(file);
    else
        Log->error("Failed to parse {}/{}", vppName, xtblName);

    return parseResult;
}

Handle<XtblFile> XtblManager::GetXtbl(const string& vppName, const string& xtblName)
{
    auto group = GetGroup(vppName);
    if (!group)
        return nullptr;

    for (auto& xtbl : group->Files)
        if (xtbl->Name == xtblName)
            return xtbl;

    //Xtbl hasn't been parsed
    return nullptr;
}

Handle<XtblFile> XtblManager::GetOrCreateXtbl(const string& vppName, const string& xtblName)
{
    auto group = GetGroup(vppName);
    if (!group)
        group = AddGroup(vppName);

    for (auto& xtbl : group->Files)
        if (xtbl->Name == xtblName)
            return xtbl;

    //Xtbl hasn't been parsed. Parse it and try to get it again. Will return nullptr if there's a parsing error.
    return ParseXtbl(vppName, xtblName) ? GetXtbl(vppName, xtblName) : nullptr;
}

Handle<XtblManager::XtblGroup> XtblManager::GetGroup(const string& vppName)
{
    for (auto& group : XtblGroups)
        if (group->VppName == vppName)
            return group;

    return nullptr;
}

Handle<XtblManager::XtblGroup> XtblManager::AddGroup(const string& vppName)
{
    auto group = GetGroup(vppName);
    return group ? group : XtblGroups.emplace_back(CreateHandle<XtblManager::XtblGroup>(vppName, std::vector<Handle<XtblFile>>{}));
}