#include "Project.h"
#include "common/filesystem/Path.h"
#include "Log.h"
#include <tinyxml2/tinyxml2.h>

bool Project::Load(const string& projectFilePath)
{
    Path = Path::GetParentDirectory(projectFilePath);
    ProjectFilename = Path::GetFileName(projectFilePath);
    Cache.Load(GetCachePath());
    return LoadProjectFile(projectFilePath);
}

bool Project::Save()
{
    tinyxml2::XMLDocument project;
    auto* projectBlock = project.NewElement("Project");
    auto* name = projectBlock->InsertNewChildElement("Name");
    auto* description = projectBlock->InsertNewChildElement("Description");
    auto* author = projectBlock->InsertNewChildElement("Author");
    auto* edits = projectBlock->InsertNewChildElement("Edits");

    //Set name, desc, and author
    name->SetText(Name.c_str());
    description->SetText(Description.c_str());
    author->SetText(Author.c_str());

    //Set edits
    for (auto& edit : Edits)
    {
        auto* editElement = edits->InsertNewChildElement("Edit");
        auto* editType = editElement->InsertNewChildElement("Type");
        auto* editPath = editElement->InsertNewChildElement("Path");
        
        editType->SetText(edit.EditType.c_str());
        editPath->SetText(edit.EditedFile.c_str());
    }

    project.InsertFirstChild(projectBlock);
    project.SaveFile((Path + "\\" + ProjectFilename).c_str());

    return true;
}

bool Project::PackageMod(const string& outputPath)
{
    return true;
}

string Project::GetCachePath()
{
    return Path + "\\cache\\";
}

void Project::RescanCache()
{
    Cache.Load(GetCachePath());
}

void Project::AddEdit(FileEdit edit)
{
    Edits.push_back(edit);
}

bool Project::LoadProjectFile(const string& projectFilePath)
{
    if (!std::filesystem::exists(projectFilePath))
        return false;

    tinyxml2::XMLDocument project;
    project.LoadFile(projectFilePath.c_str());

    //Find <Project> block. Everything is stored in this
    auto* projectBlock = project.FirstChildElement("Project");
    if (!projectBlock)
    {
        Log->info("<Project> block not found in project file \"{}\"", projectFilePath);
        return false;
    }

    //Find <Name>, <Description>, and <Author> elements. Required even if empty
    auto* name = projectBlock->FirstChildElement("Name");
    auto* description = projectBlock->FirstChildElement("Description");
    auto* author = projectBlock->FirstChildElement("Author");
    auto* edits = projectBlock->FirstChildElement("Edits");
    if(!name)
    {
        Log->info("<Name> block not found in project file \"{}\"", projectFilePath);
        return false;
    }
    if (!description)
    {
        Log->info("<Description> block not found in project file \"{}\"", projectFilePath);
        return false;
    }
    if (!author)
    {
        Log->info("<Author> block not found in project file \"{}\"", projectFilePath);
        return false;
    }
    if (!edits)
    {
        Log->info("<Edits> block not found in project file \"{}\"", projectFilePath);
        return false;
    }
    Name = name->GetText();
    Description = description->GetText();
    Author = author->GetText();

    //Get edits list
    auto* edit = edits->FirstChildElement("Edit");
    while (edit)
    {
        auto* editType = edit->FirstChildElement("Type");
        auto* editPath = edit->FirstChildElement("Path");
        if (!editType)
        {
            Log->info("<Type> block not found in <Edit> block of project file \"{}\"", projectFilePath);
            return false;
        }
        if (!editPath)
        {
            Log->info("<Path> block not found in <Edit> block of project file \"{}\"", projectFilePath);
            return false;
        }

        Edits.push_back(FileEdit{ editType->GetText(), editPath->GetText() });
        edit = edit->NextSiblingElement("Edit");
    }

    return true;
}
