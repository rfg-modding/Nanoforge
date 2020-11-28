#pragma once
#include "Common/Typedefs.h"
#include <vector>

//Represent an xml element in an xtbl file
class XtblNode
{
public:
    string Name;
    string Value;
    std::vector<XtblNode> Subnodes;

    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
};