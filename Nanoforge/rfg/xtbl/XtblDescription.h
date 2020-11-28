#pragma once
#include "Common/Typedefs.h"
#include "Common/string/String.h"
#include "XtblType.h"
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
    XtblType Type; //Todo: If equals "Element", then check for multiple sub-descriptions <Element></Element> within this description
    string DisplayName; //Todo: = Name if Display_Name node not present
    string Description;
    bool Required;
    bool Unique;
    string Default;
    i32 MaxCount;

    std::vector<XtblDescription> Subnodes;
    std::vector<string> Choices;
    XtblReference Reference;

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
        if (Type == XtblType::TableDescription)
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
            auto* displayName = node->FirstChildElement("Display_Name");
            auto* description = node->FirstChildElement("Description");
            auto* required = node->FirstChildElement("Required");
            auto* unique = node->FirstChildElement("Unique");
            auto* def = node->FirstChildElement("Default");
            auto* maxCount = node->FirstChildElement("MaxCount");
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
        }

        return true;
    }
};