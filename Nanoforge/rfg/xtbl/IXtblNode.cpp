#include "IXtblNode.h"
#include "XtblDescription.h"
#include "Xtbl.h"

void IXtblNode::DeleteSubnodes()
{
    //Break circular references between parent and child nodes then clear Subnodes
    for (auto& subnode : Subnodes)
    {
        subnode->Parent = nullptr;
        subnode->DeleteSubnodes();
    }
    Subnodes.clear();
}

std::optional<string> IXtblNode::GetSubnodeValueString(const string& subnodeName)
{
    for (auto& subnode : Subnodes)
    {
        //Todo: Add Subtype value for references and check that the reference subtype is String
        if (subnode->Type != XtblType::String && subnode->Type != XtblType::Reference)
            continue;

        if (subnode->Name == subnodeName)
            return std::get<string>(subnode->Value);
    }
    return {};
}

Handle<IXtblNode> IXtblNode::GetSubnode(const string& subnodeName)
{
    for (auto& subnode : Subnodes)
    {
        if (subnode->Name == subnodeName)
            return subnode;
    }
    return nullptr;
}

string IXtblNode::GetPath()
{
    if (Parent)
        return Parent->GetPath() + "/" + Name;
    else
        return Name;
}