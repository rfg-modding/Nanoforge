#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "XtblNode.h"
#include "XtblDescription.h"
#include <vector>

//Represents an xtbl file. This intermediate form is used instead of directly interacting with xml since it's more convenient
class XtblFile
{
public:
    //Generate XtblNode tree by parsing an xtbl file
    bool Parse(const string& path);

    //Filename of the xtbl
    string Name;
    //Name of the vpp_pc file this xtbl is in
    string VppName;
    //Xml nodes inside the <Table></Table> block
    std::vector<XtblNode> Entries;
    //Xml nodes inside the <TableTemplates></TableTemplates> block
    std::vector<XtblNode> Templates;
    //Xml nodes inside the <TableDescription></TableDescription> block. Describes data in <Table>
    XtblDescription TableDescription;
};