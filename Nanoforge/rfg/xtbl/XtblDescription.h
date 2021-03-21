#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "Common/string/String.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>

namespace tinyxml2
{
    class XMLElement;
}

//Reference to another xtbl
class XtblReference
{
public:
    bool Used = false;
    string File;
    string Type;
    bool OpenSeparate;
};

//Description of an xtbl element. Read from <TableDescription></TableDescription> block
class XtblDescription
{
public:
    string Name;
    XtblType Type;
    string DisplayName;
    string Description;
    bool Required;
    bool Unique;
    string Default;
    i32 MaxCount;

    std::vector<XtblDescription> Subnodes;
    std::vector<string> Choices;
    std::vector<string> Flags;
    XtblReference Reference;
    string Extension;
    string StartingPath;
    bool ShowPreload;
    f32 Min;
    f32 Max;
    i32 MaxChildren;
    i32 NumDisplayRows;

    bool Parse(tinyxml2::XMLElement* node)
    {
        //First read node value (the string between the <>), <Name>, and <Type>
        auto* name = node->FirstChildElement("Name");
        auto* type = node->FirstChildElement("Type");
        if (!name)
        {
            Log->error("Failed to get <Name> from xtbl description");
            return false;
        }
        if (!type)
        {
            Log->error("Failed to get <Type> from xtbl description \"{}\"", name->GetText());
            return false;
        }

        Name = name->GetText();
        //Parse <Type>. Can throw if invalid value passed to it
        try
        {
            Type = XtblTypeFromString(type->GetText());
        }
        catch (std::runtime_error& ex)
        {
            Log->error("Failed to parse <Type> from xtbl description \"{}\"", name->GetText());
            return false;
        }

        //If Type == TableDescription, then this is the root node. Parse all subnodes. Doesn't have other values
        if (Type == XtblType::TableDescription || Type == XtblType::List)
        {
            //Parse all <Element> nodes inside the root node
            auto* nextNode = node->FirstChildElement("Element");
            while (nextNode)
            {
                XtblDescription nextDesc;
                if (!nextDesc.Parse(nextNode))
                    return false;

                Subnodes.push_back(nextDesc);
                nextNode = nextNode->NextSiblingElement("Element");
            }
        }
        else //Otherwise this is a subnode. Read normal values
        {
            //Possible values in a description. Not all will be present 
            auto* displayName = node->FirstChildElement("Display_Name");
            auto* description = node->FirstChildElement("Description");
            auto* required = node->FirstChildElement("Required");
            auto* unique = node->FirstChildElement("Unique");
            auto* def = node->FirstChildElement("Default");
            auto* maxCount = node->FirstChildElement("MaxCount");
            auto* extension = node->FirstChildElement("Extension");
            auto* startingPath = node->FirstChildElement("StartingPath");
            auto* showPreload = node->FirstChildElement("ShowPreload");
            auto* minValue = node->FirstChildElement("MinValue");
            auto* maxValue = node->FirstChildElement("MaxValue");
            auto* maxChildren = node->FirstChildElement("Max_Children");
            auto* numDisplayRows = node->FirstChildElement("Num_Display_Rows");
            auto* reference = node->FirstChildElement("Reference");

            //Parse values
            if (displayName)
                DisplayName = displayName->GetText();
            else
                DisplayName = Name;
            if (description)
                Description = description->GetText();
            if (required)
                Required = String::EqualIgnoreCase(string(required->GetText()), "True") ? true : false;
            if (unique)
                Unique = String::EqualIgnoreCase(string(unique->GetText()), "True") ? true : false;
            if (def)
                Default = def->GetText();
            if (maxCount)
                MaxCount = std::stoi(maxCount->GetText());
            if (extension)
                Extension = extension->GetText();
            if (startingPath)
                StartingPath = startingPath->GetText();
            if (showPreload)
                ShowPreload = String::EqualIgnoreCase(string(showPreload->GetText()), "True") ? true : false;
            if (minValue)
                Min = minValue->FloatText();
            if (maxValue)
                Max = maxValue->FloatText();
            if (maxChildren)
                MaxChildren = maxChildren->IntText();
            if (numDisplayRows)
                NumDisplayRows = numDisplayRows->IntText();

            //Parse references if present
            if (reference)
            {
                auto* refFile = reference->FirstChildElement("File");
                auto* refType = reference->FirstChildElement("Type");
                auto* refOpenSeparate = reference->FirstChildElement("OpenSeparate");

                if (!refFile || !refType || !refOpenSeparate)
                {
                    Log->error("Invalid <Reference> detected. Doesn't have all required data.");
                    return false;
                }

                Reference.Used = true;
                Reference.File = refFile->GetText();
                Reference.Type = refType->GetText();
                Reference.OpenSeparate = String::EqualIgnoreCase(string(refOpenSeparate->GetText()), "True") ? true : false;
            }

            //If type is Element it means it has subnodes
            if (Type == XtblType::Element)
            {
                //Parse all <Element> nodes inside current node
                auto* nextNode = node->FirstChildElement("Element");
                while (nextNode)
                {
                    XtblDescription nextDesc;
                    if (!nextDesc.Parse(nextNode))
                        return false;

                    Subnodes.push_back(nextDesc);
                    nextNode = nextNode->NextSiblingElement("Element");
                }
            }

            //Read choices if present
            auto* nextChoice = node->FirstChildElement("Choice");
            while (nextChoice)
            {
                Choices.push_back(nextChoice->GetText());
                nextChoice = nextChoice->NextSiblingElement("Choice");
            }

            //Read flags if present
            auto* nextFlag = node->FirstChildElement("Flag");
            while (nextFlag)
            {
                Flags.push_back(nextFlag->GetText());
                nextFlag = nextFlag->NextSiblingElement("Flag");
            }
        }

        return true;
    }

    //Get description of xtbl value
    std::optional<XtblDescription> GetValueDescription(const string& valuePath)
    {
        //Split path into components
        std::vector<s_view> split = String::SplitString(valuePath, "/");

        //If path only has one component and matches current description then return this
        if (split.size() == 1 && String::EqualIgnoreCase(Name, split[0]))
            return *this;

        //Otherwise loop through subnodes and try to find next component of the path
        for (auto& subnode : Subnodes)
            if (String::EqualIgnoreCase(subnode.Name, split[0]))
                return subnode.GetValueDescription(valuePath.substr(valuePath.find_first_of("/") + 1));

        return {};
    }
};