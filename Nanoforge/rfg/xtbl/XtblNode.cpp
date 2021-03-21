#include "XtblNode.h"
#include "XtblDescription.h"
#include "Xtbl.h"

bool XtblNode::Parse(tinyxml2::XMLElement* node, XtblFile& xtbl, Handle<XtblNode> self)
{
    //Each XtblNode has a Name and Value. E.g. <Unique_ID>110</Unique_ID>. Value = 110 and Name = Unique_ID
    Name = node->Value();

    std::optional<XtblDescription> maybeDesc = xtbl.GetValueDescription(GetPath());
    if (Name == "_Editor")
    {
        //Set category of parent node if one is specified
        auto* category = node->FirstChildElement("Category");
        if (category)
        {
            xtbl.SetNodeCategory(Parent, category->GetText());
        }
    }
    else if (maybeDesc)
    {
        XtblDescription desc = maybeDesc.value();
        Type = desc.Type;
        switch (Type)
        {
        case XtblType::String:
            {
                Value = string(node->GetText());
            }
            break;
        case XtblType::Int:
            Value = node->IntText();
            break;
        case XtblType::Float:
            Value = node->FloatText();
            break;
        case XtblType::Vector:
            {
                auto* x = node->FirstChildElement("X");
                auto* y = node->FirstChildElement("Y");
                auto* z = node->FirstChildElement("Z");
                Vec3 vec = { 0.0f, 0.0f, 0.0f };
                if (x)
                    vec.x = x->FloatText();
                if (y)
                    vec.y = y->FloatText();
                if (z)
                    vec.z = z->FloatText();

                Value = vec;
            }
            break;
        case XtblType::Color:
            {
                auto* r = node->FirstChildElement("R");
                auto* g = node->FirstChildElement("G");
                auto* b = node->FirstChildElement("B");
                Vec3 vec = { 0.0f, 0.0f, 0.0f };
                if (r)
                    vec.x = ((f32)r->IntText() / 255.0f);
                if (g)
                    vec.y = ((f32)g->IntText() / 255.0f);
                if (b)
                    vec.z = ((f32)b->IntText() / 255.0f);

                Value = vec;
            }
            break;
        case XtblType::Selection: //This is a string with restricted choices listed in TableDescription
            Value = node->GetText();
            break;
        case XtblType::Filename:
            {
                auto* filename = node->FirstChildElement("Filename");
                if (filename && filename->GetText())
                    Value = filename->GetText();
            }
            break;
        case XtblType::Reference: //References data in another xtbl
            Value = node->GetText();
            //Todo: Support loading data from other xtbls and jumping to specific xtbls and entries that are referenced
            break;

            //Todo: Not yet implemented. Using default behavior for now
        case XtblType::Flags: //Bitflags
            //Todo: Implement
            //break;
        case XtblType::List: //List of a single value (e.g. string, float, int)
            //Todo: Implement
            //break;
        case XtblType::Grid: //List of elements
            //Todo: Implement
            //break;
        case XtblType::ComboElement: //This is similar to a union in c/c++
            //Todo: Implement
            //break;

            //These types require the subnodes to be parsed
        case XtblType::Element:
        case XtblType::TableDescription:
        default:
            if (!node->FirstChildElement() && node->GetText())
            {
                Value = node->GetText();
            }
            else //Recursively parse subnodes if there are any
            {
                auto* subnode = node->FirstChildElement();
                while (subnode)
                {
                    Handle<XtblNode> xtblSubnode = CreateHandle<XtblNode>();
                    xtblSubnode->Parent = self;
                    if (xtblSubnode->Parse(subnode, xtbl, xtblSubnode))
                        Subnodes.push_back(xtblSubnode);

                    subnode = subnode->NextSiblingElement();
                }
            }
            break;
        }
    }
    else //If can't find a description just parse the nodes as they are so we have the data in memory
    {
        Type = XtblType::None;
        auto* subnode = node->FirstChildElement();
        while (subnode)
        {
            Handle<XtblNode> xtblSubnode = CreateHandle<XtblNode>();
            xtblSubnode->Parent = self;
            if (xtblSubnode->Parse(subnode, xtbl, xtblSubnode))
                Subnodes.push_back(xtblSubnode);

            subnode = subnode->NextSiblingElement();
        }
    }

    return true;
}

std::optional<string> XtblNode::GetSubnodeValueString(const string& subnodeName)
{
    for (auto& subnode : Subnodes)
    {
        if (subnode->Type != XtblType::String)
            continue;

        if (subnode->Name == subnodeName)
            return std::get<string>(subnode->Value);
    }
    return {};
}

string XtblNode::GetPath()
{
    if (Parent)
        return Parent->GetPath() + "/" + Name;
    else
        return Name;
}