#include "IXtblNode.h"
#include "XtblDescription.h"
#include "Xtbl.h"
#include "XtblNodes.h"
#include "nodes/UnsupportedXtblNode.h"

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

IXtblNode* IXtblNode::GetSubnode(const string& subnodeName)
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

IXtblNode* IXtblNode::DeepCopy(IXtblNode* parent)
{
    IXtblNode* copy = CreateDefaultNode(Type);
    copy->Name = Name;
    copy->Value = Value;
    copy->Type = Type;
    copy->CategorySet = CategorySet;
    copy->Enabled = Enabled;
    copy->HasDescription = HasDescription;
    copy->Edited = Edited;
    copy->Parent = parent;

    //Copy subnodes to copy
    for (auto& subnode : Subnodes)
    {
        //Only UnsupportedXtblNode needs custom behavior so a special case is used here instead of making it a virtual function
        if(subnode->Type == XtblType::Unsupported)
            copy->Subnodes.push_back(new UnsupportedXtblNode((((UnsupportedXtblNode*)subnode)->element_)));
        else
            copy->Subnodes.push_back(subnode->DeepCopy(copy));
    }

    return copy;
}

void IXtblNode::CalculateEditorValues(Handle<XtblFile> xtbl, const char* nameOverride)
{
    if (editorValuesInitialized_)
        return;

    desc_ = xtbl->GetValueDescription(GetPath());
    nameNoId_ = nameOverride ? nameOverride : desc_->DisplayName;
    name_ = nameNoId_.value() + fmt::format("##{}", (u64)this);
    editorValuesInitialized_ = true;
}
