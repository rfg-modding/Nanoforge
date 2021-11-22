#include "XtblDescription.h"
#include "Xtbl.h"

bool XtblDescription::Parse(tinyxml2::XMLElement* node, Handle<XtblDescription> self, XtblFile& file)
{
    //First read node value (the string between the <>), <Name>, and <Type>
    auto* name = node->FirstChildElement("Name");
    auto* type = node->FirstChildElement("Type");
    if (!name)
    {
        LOG_ERROR("Failed to get <Name> from xtbl description");
        return false;
    }
    if (!type)
    {
        LOG_ERROR("Failed to get <Type> from xtbl description \"{}\"", name->GetText() ? name->GetText() : "<Name not set>");
        return false;
    }

    Name = name->GetText() ? name->GetText() : "";
    //Parse <Type>. Can throw if invalid value passed to it
    try
    {
        if (!type->GetText())
            throw std::runtime_error("<Type> node was empty in <TableDescription> section!");

        Type = XtblTypeFromString(type->GetText());
    }
    catch (std::runtime_error& ex)
    {
        LOG_ERROR("Failed to parse <Type> from xtbl description \"{}\". Exception message: \"{}\"", name->GetText(), ex.what());
        return false;
    }

    auto* displayName = node->FirstChildElement("Display_Name");
    if (displayName && displayName->GetText())
        DisplayName = displayName->GetText();
    else
        DisplayName = Name;

    //If Type == TableDescription, then this is the root node. Parse all subnodes. Doesn't have other values
    if (Type == XtblType::TableDescription || Type == XtblType::List)
    {
        //Parse all <Element> nodes inside the root node
        auto* nextNode = node->FirstChildElement("Element");
        while (nextNode)
        {
            Handle<XtblDescription> nextDesc = CreateHandle<XtblDescription>();
            if (!nextDesc->Parse(nextNode, nextDesc, file))
                return false;

            nextDesc->Parent = self;
            Subnodes.push_back(nextDesc);
            nextNode = nextNode->NextSiblingElement("Element");
        }
    }
    else //Otherwise this is a subnode. Read normal values
    {
        //Possible values in a description. Not all will be present
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
        if (description && description->GetText())
            Description = description->GetText();

        if (required && required->GetText())
            Required = String::EqualIgnoreCase(string(required->GetText()), "True") ? true : false;

        if (unique && unique->GetText())
            Unique = String::EqualIgnoreCase(string(unique->GetText()), "True") ? true : false;

        if (def && def->GetText())
            Default = def->GetText();

        if (maxCount && maxCount->GetText())
            MaxCount = std::stoi(maxCount->GetText());

        if (extension && extension->GetText())
            Extension = extension->GetText();

        if (startingPath && startingPath->GetText())
            StartingPath = startingPath->GetText();

        if (showPreload && showPreload->GetText())
            ShowPreload = String::EqualIgnoreCase(string(showPreload->GetText()), "True") ? true : false;

        if (minValue && minValue->GetText())
            Min = minValue->FloatText();

        if (maxValue && maxValue->GetText())
            Max = maxValue->FloatText();

        if (maxChildren && maxChildren->GetText())
            MaxChildren = maxChildren->IntText();

        if (numDisplayRows && numDisplayRows->GetText())
            NumDisplayRows = numDisplayRows->IntText();

        //Parse references if present
        if (reference)
        {
            auto* refFile = reference->FirstChildElement("File");
            auto* refType = reference->FirstChildElement("Type");
            auto* refOpenSeparate = reference->FirstChildElement("OpenSeparate");
            if (!refFile || !refType)
            {
                LOG_ERROR("Invalid <Reference> detected. Doesn't have all required data.");
                return false;
            }

            const char* fileText = refFile->GetText();
            const char* pathText = refType->GetText();
            const char* openSeparateText = refOpenSeparate ? refOpenSeparate->GetText() : nullptr;
            Reference = file.References.emplace_back(CreateHandle<XtblReference>());
            Reference->Used = true;
            Reference->File = fileText ? fileText : "";
            Reference->File = Reference->File.substr(Reference->File.find_first_of('\\') + 1); //Strip additional paths from filename. Likely remnant of dev tools working with loose files in folders
            Reference->Path = pathText ? pathText : "";
            if (refOpenSeparate)
                Reference->OpenSeparate = String::EqualIgnoreCase(string(openSeparateText ? openSeparateText : ""), "True") ? true : false;
            else
                Reference->OpenSeparate = false;

            //Replace . with / in reference variable path
            for (u32 i = 0; i < Reference->Path.size(); i++)
                if (Reference->Path[i] == '.')
                    Reference->Path[i] = '/';
        }

        //If type is Element it means it has subnodes
        if (Type == XtblType::Element || Type == XtblType::Grid || Type == XtblType::ComboElement)
        {
            //Parse all <Element> nodes inside current node
            auto* nextNode = node->FirstChildElement("Element");
            while (nextNode)
            {
                Handle<XtblDescription> nextDesc = CreateHandle<XtblDescription>();
                if (!nextDesc->Parse(nextNode, nextDesc, file))
                    return false;

                nextDesc->Parent = self;
                Subnodes.push_back(nextDesc);
                nextNode = nextNode->NextSiblingElement("Element");
            }
        }

        //Read choices if present
        auto* nextChoice = node->FirstChildElement("Choice");
        while (nextChoice)
        {
            const char* choiceText = nextChoice->GetText();
            if(choiceText)
                Choices.push_back(choiceText);

            nextChoice = nextChoice->NextSiblingElement("Choice");
        }

        //Get default choice
        tinyxml2::XMLElement* defaultChoice = node->FirstChildElement("Default");
        if (defaultChoice && defaultChoice->GetText())
            DefaultChoice = defaultChoice->GetText();
        else
            DefaultChoice = Choices.size() > 0 ? Choices[0] : "";

        //Read flags if present
        auto* nextFlag = node->FirstChildElement("Flag");
        while (nextFlag)
        {
            const char* nextFlagText = nextFlag->GetText();
            if(nextFlagText)
                Flags.push_back(nextFlagText);

            nextFlag = nextFlag->NextSiblingElement("Flag");
        }
    }

    return true;
}

bool XtblDescription::WriteXml(tinyxml2::XMLElement* xml)
{
    //All descriptions have a name and type
    auto* nameXml = xml->InsertNewChildElement("Name");
    auto* typeXml = xml->InsertNewChildElement("Type");
    nameXml->SetText(Name.c_str());
    typeXml->SetText(to_string(Type).c_str());

    //Write display name if it has one. It's set to Name on load if it's empty so we compare them
    if (DisplayName != Name)
    {
        auto* displayNameXml = xml->InsertNewChildElement("Display_Name");
        displayNameXml->SetText(DisplayName.c_str());
    }

    //Write other data if present
    if (Description.has_value())
    {
        auto* descriptionXml = xml->InsertNewChildElement("Description");
        descriptionXml->SetText(Description.value().c_str());
    }
    if (Required.has_value())
    {
        auto* requiredXml = xml->InsertNewChildElement("Required");
        requiredXml->SetText(Required.value() ? "true" : "false");
    }
    if (Unique.has_value())
    {
        auto* uniqueXml = xml->InsertNewChildElement("Unique");
        uniqueXml->SetText(Unique.value() ? "true" : "false");
    }
    if (Default.has_value())
    {
        auto* defaultXml = xml->InsertNewChildElement("Default");
        defaultXml->SetText(Default.value().c_str());
    }
    if (MaxCount.has_value())
    {
        auto* maxCountXml = xml->InsertNewChildElement("MaxCount");
        maxCountXml->SetText(std::to_string(MaxCount.value()).c_str());
    }
    if (Extension.has_value())
    {
        auto* extensionXml = xml->InsertNewChildElement("Extension");
        extensionXml->SetText(Extension.value().c_str());
    }
    if (StartingPath.has_value())
    {
        auto* startingPathXml = xml->InsertNewChildElement("StartingPath");
        startingPathXml->SetText(StartingPath.value().c_str());
    }
    if (ShowPreload.has_value())
    {
        auto* showPreloadXml = xml->InsertNewChildElement("ShowPreload");
        showPreloadXml->SetText(ShowPreload.value() ? "true" : "false");
    }
    if (Min.has_value())
    {
        auto* minXml = xml->InsertNewChildElement("MinValue");
        minXml->SetText(std::to_string(Min.value()).c_str());
    }
    if (Max.has_value())
    {
        auto* maxXml = xml->InsertNewChildElement("MaxValue");
        maxXml->SetText(std::to_string(Max.value()).c_str());
    }
    if (MaxChildren.has_value())
    {
        auto* maxChildrenXml = xml->InsertNewChildElement("Max_Children");
        maxChildrenXml->SetText(std::to_string(MaxChildren.value()).c_str());
    }
    if (NumDisplayRows.has_value())
    {
        auto* numDisplayRowsXml = xml->InsertNewChildElement("Num_Display_Rows");
        numDisplayRowsXml->SetText(std::to_string(NumDisplayRows.value()).c_str());
    }
    for (auto& choice : Choices)
    {
        auto* choiceXml = xml->InsertNewChildElement("Choice");
        choiceXml->SetText(choice.c_str());
    }
    for (auto& flag : Flags)
    {
        auto* flagXml = xml->InsertNewChildElement("Flag");
        flagXml->SetText(flag.c_str());
    }
    if (Reference)
    {
        auto* referenceXml = xml->InsertNewChildElement("Reference");
        auto* fileXml = referenceXml->InsertNewChildElement("File");
        auto* referenceTypeXml = referenceXml->InsertNewChildElement("Type");
        auto* openSeparateXml = referenceXml->InsertNewChildElement("OpenSeparate");

        fileXml->SetText(Reference->File.c_str());
        referenceTypeXml->SetText(Reference->Path.c_str());
        openSeparateXml->SetText(Reference->OpenSeparate ? "true" : "false");
    }

    //Write subdesc data
    for (auto& subdesc : Subnodes)
    {
        auto* subXml = xml->InsertNewChildElement("Element");
        subdesc->WriteXml(subXml);
    }

    return false;
}

//Returns the path of the value. This is this nodes name prepended with the names of it's parents
string XtblDescription::GetPath()
{
    if (Parent)
        return Parent->GetPath() + "/" + Name;
    else
        return Name;
}