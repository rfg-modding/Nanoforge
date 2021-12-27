#include "XtblNodes.h"
#include "rfg/xtbl/IXtblNode.h"
#include "rfg/xtbl/XtblDescription.h"
#include "render/imgui/ImGuiConfig.h"
#include "render/imgui/imgui_ext.h"
#include "rfg/xtbl/Xtbl.h"
#include <imgui.h>
#include "Log.h"

//Use descriptions to draw nodes so non-existant optional nodes are drawn
//Also handles drawing description tooltips and checkboxes for optional nodes
//Returns true if edits were performed
bool DrawNodeByDescription(GuiState* guiState, Handle<XtblFile> xtbl, Handle<XtblDescription> desc, IXtblNode* parent,
    const char* nameOverride, IXtblNode* nodeOverride)
{
    //Use lambda to get node since we need this logic multiple times
    string descPath = desc->GetPath();
    auto getNode = [&]() -> IXtblNode*
    {
        return nodeOverride ? nodeOverride : xtbl->GetSubnode(descPath.substr(descPath.find_last_of('/') + 1), parent);
    };
    IXtblNode* node = getNode();
    bool nodePresent = (node != nullptr && node->Enabled);
    bool editedThisFrame = false; //Used for document unsaved change tracking

    //IDs used by dear imgui elements
    string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
    string checkboxId = fmt::format("##{}", (u64)desc.get()); //Using pointer address as ID since it's unique

    //Draw checkbox for optional elements
    if (desc->Required.has_value() && !desc->Required.value() || !nodePresent)
    {
        if (ImGui::Checkbox(checkboxId.c_str(), &nodePresent))
        {
            //When checkbox is set to true ensure elements exist in node
            if (nodePresent)
            {
                //Ensure stringXml and it's subnodes exist. Newly created optional subnodes will created but disabled by default
                xtbl->EnsureEntryExists(desc, parent, false);
                node = getNode();
                node->Enabled = true;
            }
            else
            {
                node->Enabled = false;
            }

            editedThisFrame = true;
            node->Edited = true;
        }
        ImGui::SameLine();
    }

    //If an optional element isn't present draw it's display name
    if (!nodePresent)
    {
        ImGui::Text(nameNoId);
        if (desc->Description.has_value() && desc->Description != "") //Draw tooltip if not empty
            gui::TooltipOnPrevious(desc->Description.value(), nullptr);

        return false;
    }

    //If a node is present draw it.
    if (nodePresent)
        if (node->DrawEditor(guiState, xtbl, parent, nameOverride))
            editedThisFrame = true;

    //Draw tooltip if not empty
    if (desc->Description.has_value() && desc->Description != "")
        gui::TooltipOnPrevious(desc->Description.value(), nullptr);

    return editedThisFrame;
}

//Create node from XtblType using default value for that type
IXtblNode* CreateDefaultNode(XtblType type)
{
    IXtblNode* node = nullptr;
    switch (type)
    {
    case XtblType::Element:
        node = new ElementXtblNode();
        break;
    case XtblType::String:
        node = new StringXtblNode();
        break;
    case XtblType::Int:
        node = new IntXtblNode();
        break;
    case XtblType::Float:
        node = new FloatXtblNode();
        break;
    case XtblType::Vector:
        node = new VectorXtblNode();
        break;
    case XtblType::Color:
        node = new ColorXtblNode();
        break;
    case XtblType::Selection:
        node = new SelectionXtblNode();
        break;
    case XtblType::Flags:
        node = new FlagsXtblNode();
        break;
    case XtblType::List:
        node = new ListXtblNode();
        break;
    case XtblType::Filename:
        node = new FilenameXtblNode();
        break;
    case XtblType::ComboElement:
        node = new ComboElementXtblNode();
        break;
    case XtblType::Reference:
        node = new ReferenceXtblNode();
        break;
    case XtblType::Grid:
        node = new GridXtblNode();
        break;
    case XtblType::TableDescription:
        node = new TableDescriptionXtblNode();
        break;
    case XtblType::Flag:
        node = new FlagXtblNode();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Passed an invalid XtblType value \"{}\"", type);
    }

    node->InitDefault();
    return node;
}

//Create node from XtblDescription using default value from that description stringXml
IXtblNode* CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes)
{
    IXtblNode* node = nullptr;
    switch (desc->Type)
    {
    case XtblType::Element:
        node = new ElementXtblNode();
        if (addSubnodes)
        {
            for (auto& subdesc : desc->Subnodes)
            {
                auto subnode = CreateDefaultNode(subdesc);
                if (subnode)
                {
                    subnode->Parent = node;
                    node->Subnodes.push_back(subnode);
                }
            }
        }
        break;
    case XtblType::String:
        node = new StringXtblNode();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Int:
        node = new IntXtblNode();
        node->Value = std::stoi(desc->Default.has_value() ? desc->Default.value() : "0");
        node->InitDefault();
        break;
    case XtblType::Float:
        node = new FloatXtblNode();
        node->Value = std::stof(desc->Default.has_value() ? desc->Default.value() : "0.0");
        break;
    case XtblType::Vector:
        node = new VectorXtblNode();
        node->InitDefault();
        break;
    case XtblType::Color:
        node = new ColorXtblNode();
        node->InitDefault();
        break;
    case XtblType::Selection:
        node = new SelectionXtblNode();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Flags:
        node = new FlagsXtblNode();
        //Ensure there's a subnode for every flag listed in <TableDescription>
        if (addSubnodes)
        {
            for (auto& flagDesc : desc->Flags)
            {
                //Check if flag has a subnode
                IXtblNode* flagNode = nullptr;
                for (auto& flag : node->Subnodes)
                {
                    auto value = std::get<XtblFlag>(flag->Value);
                    if (value.Name == flagDesc)
                    {
                        flagNode = flag;
                        break;
                    }
                }

                //If flag isn't present create a subnode for it
                if (!flagNode)
                {
                    auto subnode = CreateDefaultNode(XtblType::Flag);
                    subnode->Name = "Flag";
                    subnode->Type = XtblType::Flag;
                    subnode->Parent = node;
                    subnode->CategorySet = false;
                    subnode->HasDescription = true;
                    subnode->Enabled = true;
                    subnode->Value = XtblFlag{ .Name = flagDesc, .Value = false };
                    node->Subnodes.push_back(subnode);
                }
            }
        }
        break;
    case XtblType::List:
        node = new ListXtblNode();
        node->InitDefault();
        break;
    case XtblType::Filename:
        node = new FilenameXtblNode();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::ComboElement:
        node = new ComboElementXtblNode();
        node->InitDefault();
        break;
    case XtblType::Reference:
        node = new ReferenceXtblNode();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Grid:
        node = new GridXtblNode();
        node->InitDefault();
        break;
    case XtblType::TableDescription:
        node = new TableDescriptionXtblNode();
        node->InitDefault();
        if (addSubnodes)
        {
            for (auto& subdesc : desc->Subnodes)
            {
                auto subnode = CreateDefaultNode(subdesc);
                if (subnode)
                {
                    subnode->Parent = node;
                    node->Subnodes.push_back(subnode);
                }
            }
        }
        break;
    case XtblType::Flag:
        node = new FlagXtblNode();
        node->InitDefault();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Passed an invalid XtblType value \"{}\"", desc->Type);
    }

    node->Name = desc->Name;
    node->Type = desc->Type;
    node->HasDescription = true;
    return node;
}