#pragma once
#include "Common/Typedefs.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"
#include "XtblType.h"
#include "IXtblNode.h"
#include "XtblDescription.h"
#include "XtblManager.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include "gui/GuiState.h"
#include "gui/documents/XtblDocument.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

/*This file has all the IXtblNode implementations. One implementation per XtblType enum value.*/
static f32 NodeGuiWidth = 240.0f;

//Create node from XtblType using default value for that type
static Handle<IXtblNode> CreateDefaultNode(XtblType type);
static Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes = false);

//Use descriptions to draw nodes so non-existant optional nodes are drawn
//Also handles drawing description tooltips and checkboxes for optional nodes
static void DrawNodeByDescription(GuiState* guiState, Handle<XtblFile> xtbl, Handle<XtblDescription> desc, Handle<IXtblNode> rootNode, 
                                  const char* nameOverride = nullptr, Handle<IXtblNode> nodeOverride = nullptr)
{
    //Use lambda to get node since we need this logic multiple times
    string descPath = desc->GetPath();
    auto getNode = [&]() -> Handle<IXtblNode>
    {
        return nodeOverride ? nodeOverride : xtbl->GetSubnode(descPath.substr(descPath.find_first_of('/') + 1), rootNode);
    };
    Handle<IXtblNode> node = getNode();
    bool nodePresent = (node != nullptr && node->Enabled);

    //IDs used by dear imgui elements
    string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
    string checkboxId = fmt::format("##{}", (u64)desc.get()); //Using pointer address as ID since it's unique

    //Draw checkbox for optional elements
    if (desc->Required.has_value() && !desc->Required.value())
    {
        if (ImGui::Checkbox(checkboxId.c_str(), &nodePresent))
        {
            //When checkbox is set to true ensure elements exist in node
            if (nodePresent)
            {
                //Ensure stringXml and it's subnodes exist. Newly created optional subnodes will created but disabled by default
                xtbl->EnsureEntryExists(desc, rootNode, false);
                node = getNode();
                node->Enabled = nodePresent;
                return;
            }
            else
            {
                node->Enabled = false;
            }
            
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

        return;
    }

    //If a node is present draw it.
    if(nodePresent)
        node->DrawEditor(guiState, xtbl, rootNode, nameOverride);

    //Draw tooltip if not empty
    if (desc->Description.has_value() && desc->Description != "")
        gui::TooltipOnPrevious(desc->Description.value(), nullptr);
}

//A node that contains many other nodes. Usually the root node of an stringXml is an element node. E.g. each <Vehicle> block is vehicles.xtbl is an element node.
class ElementXtblNode : public IXtblNode
{
public:
    ~ElementXtblNode()
    {
    
    }
    
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description.has_value() && desc->Description != "")
                gui::TooltipOnPrevious(desc->Description.value(), nullptr);
            for (auto& subnode : Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subdesc->DisplayName.c_str(), subnode);
            }

            ImGui::TreePop();
        }
    }
    
    virtual void InitDefault()
    {
    
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Make another xml element and write edited subnodes to it
        auto* elementXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(elementXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write subnodes to it
        auto* elementXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(elementXml);
            if (!result)
            {
                Log->error("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            elementXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with a string value
class StringXtblNode : public IXtblNode
{
public:
    ~StringXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Todo: Add way to change names and auto-update any references. Likely would do this in the XtblDocument stringXml sidebar. 
        //Name nodes uneditable for the time being since they're use by xtbl references
        if (nameNoId == "Name")
        {
            return;
        }
        else if (nameNoId == "Description" && (xtbl->Name == "dlc01_tweak_table.xtbl" || xtbl->Name == "online_tweak_table.xtbl" || xtbl->Name == "tweak_table.xtbl"))
        {
            //Todo: Come up with a better way of handling this. Maybe get give all strings big wrapped blocks so they're fully visible
            //Special case for tweak table descriptions
            ImGui::Text("Description:");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
            ImGui::TextWrapped(std::get<string>(Value).c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            if (ImGui::InputText(name, std::get<string>(Value)))
                Edited = true;
        }
    }

    virtual void InitDefault()
    {
        Value = "";
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* stringXml = xml->InsertNewChildElement(Name.c_str());
        stringXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            stringXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with a signed integer value
class IntXtblNode : public IXtblNode
{
public:
    ~IntXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (desc->Min && desc->Max)
        {
            if (ImGui::SliderInt(name.c_str(), &std::get<i32>(Value), (i32)desc->Min.value(), (i32)desc->Max.value()))
                Edited = true;
        }
        else
        {
            if(ImGui::InputInt(name.c_str(), &std::get<i32>(Value)))
                Edited = true;
        }
    }

    virtual void InitDefault()
    {
        Value = 0;
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* intXml = xml->InsertNewChildElement(Name.c_str());
        intXml->SetText(std::to_string(std::get<i32>(Value)).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            intXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with a floating point value
class FloatXtblNode : public IXtblNode
{
public:
    ~FloatXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (desc->Min && desc->Max)
        {
            if(ImGui::SliderFloat(name.c_str(), &std::get<f32>(Value), desc->Min.value(), desc->Max.value()))
                Edited = true;
        }
        else
        {
            if(ImGui::InputFloat(name.c_str(), &std::get<f32>(Value)))
                Edited = true;
        }
    }

    virtual void InitDefault()
    {
        Value = 0.0f;
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* floatXml = xml->InsertNewChildElement(Name.c_str());
        floatXml->SetText(std::to_string(std::get<f32>(Value)).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            floatXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with a 3d vector value
class VectorXtblNode : public IXtblNode
{
public:
    ~VectorXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if(ImGui::InputFloat3(name.c_str(), (f32*)&std::get<Vec3>(Value)))
            Edited = true;
    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element with <X> <Y> <Z> sub elements
        auto* vectorXml = xml->InsertNewChildElement(Name.c_str());
        auto* x = vectorXml->InsertNewChildElement("X");
        auto* y = vectorXml->InsertNewChildElement("Y");
        auto* z = vectorXml->InsertNewChildElement("Z");

        //Write values to xml
        Vec3 vec = std::get<Vec3>(Value);
        x->SetText(std::to_string(vec.x).c_str());
        y->SetText(std::to_string(vec.y).c_str());
        z->SetText(std::to_string(vec.z).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            vectorXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with a RGB color value
class ColorXtblNode : public IXtblNode
{
public:
    ~ColorXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if(ImGui::ColorPicker3(name.c_str(), (f32*)&std::get<Vec3>(Value)))
            Edited = true;
    }

    virtual void InitDefault()
    {
        Value = Vec3(0.0f, 0.0f, 0.0f);
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element with <R> <G> <B> sub elements
        auto* colorXml = xml->InsertNewChildElement(Name.c_str());
        auto* r = colorXml->InsertNewChildElement("R");
        auto* g = colorXml->InsertNewChildElement("G");
        auto* b = colorXml->InsertNewChildElement("B");

        //Write values to xml as u8s [0, 255]
        Vec3 vec = std::get<Vec3>(Value);
        r->SetText(std::to_string((u8)(vec.x * 255.0f)).c_str());
        g->SetText(std::to_string((u8)(vec.y * 255.0f)).c_str());
        b->SetText(std::to_string((u8)(vec.z * 255.0f)).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            colorXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with preset options described in xtbl description block
class SelectionXtblNode : public IXtblNode
{
public:
    ~SelectionXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);
        string& nodeValue = std::get<string>(Value);

        //Select the first choice if one hasn't been selected
        if (nodeValue == "")
            nodeValue = desc->Choices[0];

        //Draw combo with all possible choices
        if (ImGui::BeginCombo(name.c_str(), std::get<string>(Value).c_str()))
        {
            for (auto& choice : desc->Choices)
            {
                bool selected = choice == nodeValue;
                if (ImGui::Selectable(choice.c_str(), &selected))
                {
                    Value = choice;
                    Edited = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {
        Value = "";
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* selectionXml = xml->InsertNewChildElement(Name.c_str());
        selectionXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            selectionXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with set of flags that can be true or false. Flags are described in xtbl description block
class FlagsXtblNode : public IXtblNode
{
public:
    ~FlagsXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description.has_value() && desc->Description.value() != "")
                gui::TooltipOnPrevious(desc->Description.value(), nullptr);
            for (auto& subnode : Subnodes)
            {
                auto& value = std::get<XtblFlag>(subnode->Value);
                if(ImGui::Checkbox(value.Name.c_str(), &value.Value))
                    Edited = true;
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write enabled flags to it
        auto* flagsXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& flag : Subnodes)
        {
            //Skip disabled flags
            XtblFlag flagValue = std::get<XtblFlag>(flag->Value);
            if (!flagValue.Value)
                continue;

            //Write flag to xml
            auto* flagXml = flagsXml->InsertNewChildElement("Flag");
            flagXml->SetText(flagValue.Name.c_str());
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            flagsXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node containing a list of other nodes
class ListXtblNode : public IXtblNode
{
public:
    ~ListXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Draw subnodes
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description.has_value() && desc->Description.value() != "")
                gui::TooltipOnPrevious(desc->Description.value(), nullptr);

            for (auto& subnode : Subnodes)
            {
                //Try to get <Name></Name> value
                string subnodeName = subnode->Name;
                auto nameValue = subnode->GetSubnodeValueString("Name");
                if (nameValue)
                    subnodeName = nameValue.value();

                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subnodeName.c_str(), subnode);
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Make another xml element and write edited subnodes to it
        auto* listXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Don't write unedited node unless it's the Name node which is used for identification of list elements
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(listXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write edited subnodes to it
        auto* listXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(listXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            listXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node with the name of an RFG asset file
class FilenameXtblNode : public IXtblNode
{
public:
    ~FilenameXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //Todo: Get a list of files with correct format for this node and list those instead of having the player type names out
        if(ImGui::InputText(name, std::get<string>(Value)))
            Edited = true;
    }

    virtual void InitDefault()
    {
        Value = "";
    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* filenameXml = xml->InsertNewChildElement(Name.c_str());
        filenameXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            filenameXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node that can have multiple value types. Similar to a union in C/C++
class ComboElementXtblNode : public IXtblNode
{
public:
    ~ComboElementXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        //This type of xtbl node acts like a C/C++ Union, so multiple types can be chosen
        if (ImGui::TreeNode(name.c_str()))
        {
            if (desc->Description.has_value() && desc->Description.value() != "")
                gui::TooltipOnPrevious(desc->Description.value(), nullptr);

            //Get current type. Only one type from the desc is present
            u32 currentType = 0xFFFFFFFF;
            for (u32 i = 0; i < desc->Subnodes.size(); i++)
                for (auto& subnode : Subnodes)
                    if (String::EqualIgnoreCase(desc->Subnodes[i]->Name, subnode->Name))
                        currentType = i;

            //Draw a radio button for each type
            for (u32 i = 0; i < desc->Subnodes.size(); i++)
            {
                bool active = i == currentType;
                if (ImGui::RadioButton(desc->Subnodes[i]->Name.c_str(), active))
                {
                    Edited = true;
                    //If different type is selected activate new node and delete current one
                    if (!active)
                    {
                        //Delete current nodes
                        for (auto& subnode : Subnodes)
                        {
                            subnode->DeleteSubnodes();
                            subnode->Parent = nullptr;
                        }
                        Subnodes.clear();

                        //Create default node
                        auto subdesc = desc->Subnodes[i];
                        auto subnode = CreateDefaultNode(subdesc, true);
                        subnode->Name = subdesc->Name;
                        subnode->Type = subdesc->Type;
                        subnode->Parent = Parent->GetSubnode(Name);
                        subnode->CategorySet = false;
                        subnode->HasDescription = true;
                        subnode->Enabled = true;
                        Subnodes.push_back(subnode);
                    }
                }
                if (i != desc->Subnodes.size() - 1)
                    ImGui::SameLine();
            }

            //Draw current type
            for (auto& subnode : Subnodes)
            {
                //Gets parent node name and current node name in path
                string subdescPath = subnode->GetPath();
                subdescPath = subdescPath.substr(String::FindNthCharacterFromBack(subdescPath, '/', 2) + 1);
                Handle<XtblDescription> subdesc = xtbl->GetValueDescription(subdescPath, desc);
                if (subdesc)
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, subdesc->DisplayName.c_str());
            }

            ImGui::TreePop();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* comboElementXml = xml->InsertNewChildElement(Name.c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            comboElementXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return Subnodes[0]->WriteXml(comboElementXml); //Each combo element node should only have 1 subnode
    }
};

//Node which references the value of nodes in another xtbl
class ReferenceXtblNode : public IXtblNode
{
public:
    ~ReferenceXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        //Get referenced xtbl
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);
        if (!desc->Reference)
        {
            gui::LabelAndValue(nameNoId + ":", std::get<string>(Value) + " [Error: Null reference]");
            return;
        }
        auto refXtbl = guiState->Xtbls->GetOrCreateXtbl(xtbl->VppName, desc->Reference->File);
        if (!refXtbl)
            return;

        //Find referenced values
        std::vector<Handle<IXtblNode>> referencedNodes = {};
        auto split = String::SplitString(desc->Reference->Path, "/");
        string optionPath = desc->Reference->Path.substr(desc->Reference->Path.find_first_of('/') + 1);
        for (auto& subnode : refXtbl->Entries)
        {
            if (!String::EqualIgnoreCase(subnode->Name, split[0]))
                continue;

            //Get list of matching subnodes. Some files like human_team_names.xtbl use lists instead of separate elements
            auto options = refXtbl->GetSubnodes(optionPath, subnode);
            if (options.size() == 0)
                continue;

            for (auto& option : options)
                referencedNodes.push_back(option);
        }

        //Exit early if there are no referenced nodes or the reference type isn't string (all other types aren't yet supported)
        if (referencedNodes.size() == 0 || referencedNodes[0]->Type != XtblType::String)
        {
            gui::LabelAndValue(nameNoId + ":", std::get<string>(Value) + " [Error: Unsupported reference type]");
            return;
        }
        string& nodeValue = std::get<string>(Value);

        //Find largest reference name to align combo buttons
        ImVec2 maxReferenceSize = { 0.0f, 0.0f };
        for (auto& node : referencedNodes)
        {
            ImVec2 size = ImGui::CalcTextSize(std::get<string>(node->Value).c_str());
            if (maxReferenceSize.x < size.x)
                maxReferenceSize.x = size.x;
            if (maxReferenceSize.y < size.y)
                maxReferenceSize.y = size.y;
        }

        //Offset used to make combo buttons line up with text
        static f32 comboButtonWidth = 26.5f;
        static f32 comboButtonHeight = 25.0f;
        static f32 comboButtonOffsetY = -5.0f;

        //Select the first option if one hasn't been selected
        if (nodeValue == "")
            nodeValue = std::get<string>(referencedNodes[0]->Value);

        //Draw combo with an option for each referenced value
        if (ImGui::BeginCombo(name.c_str(), std::get<string>(Value).c_str()))
        {
            for (auto& option : referencedNodes)
            {
                //Draw option
                string variableValue = std::get<string>(option->Value);
                bool selected = variableValue == nodeValue;
                if (ImGui::Selectable(variableValue.c_str(), &selected, 0, maxReferenceSize))
                {
                    nodeValue = variableValue;
                    Edited = true;
                }

                //Draw button to jump to xtbl being referenced
                ImGui::SameLine();
                ImGui::SetCursorPos({ ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + comboButtonOffsetY });
                if (ImGui::Button(fmt::format("{}##{}", ICON_FA_EXTERNAL_LINK_ALT, (u64)option.get()).c_str(), ImVec2(comboButtonWidth, comboButtonHeight)))
                {
                    //Find parent element of variable
                    Handle<IXtblNode> documentRoot = option;
                    while (String::Contains(documentRoot->GetPath(), "/") && documentRoot->Parent)
                    {
                        documentRoot = documentRoot->Parent;
                    }

                    //Jump to referenced xtbl and select current node
                    auto document = std::dynamic_pointer_cast<XtblDocument>(guiState->GetDocument(refXtbl->Name));
                    if (document)
                    {
                        document->SelectedNode = documentRoot;
                        ImGui::SetWindowFocus(document->Title.c_str());
                    }
                    else
                    {
                        document = CreateHandle<XtblDocument>(guiState, refXtbl->Name, refXtbl->VppName, refXtbl->VppName, false, documentRoot);
                        guiState->CreateDocument(refXtbl->Name, document);
                    }
                }
            }

            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* referenceXml = xml->InsertNewChildElement(Name.c_str());
        referenceXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            referenceXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node which is a table of values with one or more rows and columns
class GridXtblNode : public IXtblNode
{
public:
    ~GridXtblNode()
    {

    }

    //Todo: Optimize this. Some grids cause serious performance issues. E.g. Open player.xtbl and scroll down to the bottom of the grid
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        Handle<XtblDescription> desc = xtbl->GetValueDescription(GetPath());
        string nameNoId = nameOverride ? nameOverride : desc->DisplayName;
        string name = nameNoId + fmt::format("##{}", (u64)this);

        ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_BordersOuter
                                | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable 
                                | ImGuiTableFlags_Hideable;

        ImGui::Text("%s:", nameNoId.c_str());
        if (ImGui::BeginTable(name.c_str(), desc->Subnodes[0]->Subnodes.size(), flags))
        {
            //Setup columns
            ImGui::TableSetupScrollFreeze(0, 1);
            for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                ImGui::TableSetupColumn(subdesc->Name.c_str(), ImGuiTableColumnFlags_None, NodeGuiWidth * 1.14f);

            //Fill table data
            ImGui::TableHeadersRow();
            for (auto& subnode : Subnodes)
            {
                ImGui::TableNextRow();
                for (auto& subdesc : desc->Subnodes[0]->Subnodes)
                {
                    ImGui::TableNextColumn();
                    DrawNodeByDescription(guiState, xtbl, subdesc, rootNode, nullptr, subnode->GetSubnode(subdesc->Name));
                }
            }

            ImGui::EndTable();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Make another xml element and write edited subnodes to it
        auto* gridXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(gridXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make another xml element and write edited subnodes to it
        auto* gridXml = xml->InsertNewChildElement(Name.c_str());
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(gridXml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            gridXml->SetAttribute("__NanoforgeEdited", Edited);
        }

        return true;
    }
};

//Node found in xtbl description block found at the end of xtbl files
class TableDescriptionXtblNode : public IXtblNode
{
public:
    ~TableDescriptionXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Write subnodes to modinfo
        for (auto& subnode : Subnodes)
        {
            if (!subnode->Edited)
                continue;

            //Stop early if any node fails to write
            bool result = subnode->WriteModinfoEdits(xml);
            if (!result)
            {
                Log->error("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }
        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            xml->SetAttribute("__NanoforgeEdited", Edited);
        }

        //Write subnodes to xml
        for (auto& subnode : Subnodes)
        {
            //Stop early if any node fails to write
            bool result = subnode->WriteXml(xml);
            if (!result)
            {
                Log->error("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
                return false;
            }
        }
        return true;
    }
};

//Flag node. Contained by FlagsXtblNode
class FlagXtblNode : public IXtblNode
{
public:
    ~FlagXtblNode()
    {

    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr)
    {
        
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //Handled by FlagsXtblNode
        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Handled by FlagsXtblNode
        return true;
    }
};

//Create node from XtblType using default value for that type
static Handle<IXtblNode> CreateDefaultNode(XtblType type)
{
    Handle<IXtblNode> node = nullptr;
    switch (type)
    {
    case XtblType::Element:
        node = CreateHandle<ElementXtblNode>();
        break;
    case XtblType::String:
        node = CreateHandle<StringXtblNode>();
        break;
    case XtblType::Int:
        node = CreateHandle<IntXtblNode>();
        break;
    case XtblType::Float:
        node = CreateHandle<FloatXtblNode>();
        break;
    case XtblType::Vector:
        node = CreateHandle<VectorXtblNode>();
        break;
    case XtblType::Color:
        node = CreateHandle<ColorXtblNode>();
        break;
    case XtblType::Selection:
        node = CreateHandle<SelectionXtblNode>();
        break;
    case XtblType::Flags:
        node = CreateHandle<FlagsXtblNode>();
        break;
    case XtblType::List:
        node = CreateHandle<ListXtblNode>();
        break;
    case XtblType::Filename:
        node = CreateHandle<FilenameXtblNode>();
        break;
    case XtblType::ComboElement:
        node = CreateHandle<ComboElementXtblNode>();
        break;
    case XtblType::Reference:
        node = CreateHandle<ReferenceXtblNode>();
        break;
    case XtblType::Grid:
        node = CreateHandle<GridXtblNode>();
        break;
    case XtblType::TableDescription:
        node = CreateHandle<TableDescriptionXtblNode>();
        break;
    case XtblType::Flag:
        node = CreateHandle<FlagXtblNode>();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", type);
    }

    node->InitDefault();
    return node;
}

//Create node from XtblDescription using default value from that description stringXml
static Handle<IXtblNode> CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes)
{
    Handle<IXtblNode> node = nullptr;
    switch (desc->Type)
    {
    case XtblType::Element:
        node = CreateHandle<ElementXtblNode>();
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
        node = CreateHandle<StringXtblNode>();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Int:
        node = CreateHandle<IntXtblNode>();
        node->Value = std::stoi(desc->Default.has_value() ? desc->Default.value() : "0");
        node->InitDefault();
        break;
    case XtblType::Float:
        node = CreateHandle<FloatXtblNode>();
        node->Value = std::stof(desc->Default.has_value() ? desc->Default.value() : "0.0");
        break;
    case XtblType::Vector:
        node = CreateHandle<VectorXtblNode>();
        node->InitDefault();
        break;
    case XtblType::Color:
        node = CreateHandle<ColorXtblNode>();
        node->InitDefault();
        break;
    case XtblType::Selection:
        node = CreateHandle<SelectionXtblNode>();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Flags:
        node = CreateHandle<FlagsXtblNode>();
        //Ensure there's a subnode for every flag listed in <TableDescription>
        for (auto& flagDesc : desc->Flags)
        {
            //Check if flag has a subnode
            Handle<IXtblNode> flagNode = nullptr;
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
        break;
    case XtblType::List:
        node = CreateHandle<ListXtblNode>();
        node->InitDefault();
        break;
    case XtblType::Filename:
        node = CreateHandle<FilenameXtblNode>();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::ComboElement:
        node = CreateHandle<ComboElementXtblNode>();
        node->InitDefault();
        break;
    case XtblType::Reference:
        node = CreateHandle<ReferenceXtblNode>();
        node->Value = desc->Default.has_value() ? desc->Default.value() : "";
        break;
    case XtblType::Grid:
        node = CreateHandle<GridXtblNode>();
        node->InitDefault();
        break;
    case XtblType::TableDescription:
        node = CreateHandle<TableDescriptionXtblNode>();
        node->InitDefault();
        break;
    case XtblType::Flag:
        node = CreateHandle<FlagXtblNode>();
        node->InitDefault();
        break;
    case XtblType::None:
    default:
        THROW_EXCEPTION("Invalid value for XtblType \"{}\" passed to CreateDefaultNode() in XtblNodes.h", desc->Type);
    }

    node->Name = desc->Name;
    node->Type = desc->Type;
    node->HasDescription = true;
    return node;
}