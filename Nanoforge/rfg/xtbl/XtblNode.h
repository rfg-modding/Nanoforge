#pragma once
#include "Common/Typedefs.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>

//Represent an xml element in an xtbl file
class XtblNode
{
public:
    string Name;
    string Value;
    std::vector<XtblNode> Subnodes;

    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
    bool Parse(tinyxml2::XMLElement* node)
    {
        //Each XtblNode has a Name and Value. E.g. <Unique_ID>110</Unique_ID>. Value = 110 and Name = Unique_ID
        Name = node->Value();

        //Get node value if there are no subnodes and there is a value. 
        //XMLElement::NoChildren() refers to nodes like <Visuals></Visuals> (not even any text) which is why !node->FirstChildElement() is used to check for subnodes instead.
        if (!node->FirstChildElement() && node->GetText())
        {
            Value = node->GetText();
        }
        else //Recursively parse subnodes if there are any
        {
            auto* subnode = node->FirstChildElement();
            while (subnode)
            {
                XtblNode xtblSubnode;
                if (xtblSubnode.Parse(subnode))
                    Subnodes.push_back(xtblSubnode);

                subnode = subnode->NextSiblingElement();
            }
        }

        return true;
    }
    
    std::optional<string> GetSubnodeValue(const string& subnodeName)
    {
        for (auto& subnode : Subnodes)
        {
            if (subnode.Name == subnodeName)
                return subnode.Value;
        }
        return {};
    }
};