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
    auto* name = project.NewElement("Name");
    auto* description = project.NewElement("Description");
    auto* author = project.NewElement("Author");

    name->SetText(Name.c_str());
    description->SetText(Description.c_str());
    author->SetText(Author.c_str());

    projectBlock->InsertFirstChild(name);
    projectBlock->InsertFirstChild(description);
    projectBlock->InsertFirstChild(author);
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
    Name = name->GetText();
    Description = description->GetText();
    Author = author->GetText();

    return true;
}
