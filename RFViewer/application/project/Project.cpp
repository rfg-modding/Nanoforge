#include "Project.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "common/filesystem/File.h"
#include "util/RfgUtil.h"
#include "Log.h"
#include <tinyxml2/tinyxml2.h>
#include <spdlog/fmt/fmt.h>

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
    //Create output path
    std::filesystem::create_directories(outputPath);

    //Create modinfo.xml and fill out basic info
    tinyxml2::XMLDocument modinfo;
    auto* modBlock = modinfo.NewElement("Mod");
    auto* author = modBlock->InsertNewChildElement("Author");
    auto* description = modBlock->InsertNewChildElement("Description");
    auto* changes = modBlock->InsertNewChildElement("Changes");
    modBlock->SetAttribute("Name", Name.c_str());
    author->SetText(Author.c_str());
    description->SetText(Description.c_str());
    
    //Loop through edits
    for (auto& edit : Edits)
    {
        //Split edited file cache path
        std::vector<s_view> split = String::SplitString(edit.EditedFile, "\\");
        bool inContainer = split.size() == 3;

        //Todo: Define IAction interface that each edit/action will be implemented as. Put this behavior in that
        //Update modinfo, repack files if necessary, and copy edited files to output
        if (edit.EditType == "TextureEdit")
        {
            if (inContainer)
            {
                //Todo: **SECOND** Need to split this step into a separate loop to support editing multiple cpegs in one str2_pc
                //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
                //Todo: Determine what data needs to be stored for the following loop to complete this step
                //Copy edited str2_pc contents to temp folder
                //Copy unedited str2_pc contents to temp folder. We need all the files to repack the str2
                //Repack str2_pc from temp files, delete temp files, and parse freshly packed str2
                //Update asm_pc from freshly packed str2
                //Resave asm_pc to project cache
                //Copy new str2 and asm files to output folder

                //Todo: **THIRD** Write this step. Only step that can be done immediately
                //Add <Replace> blocks for str2 and asm file
            }
            else
            {
                //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
                //Copy edited files from project cache to output folder
                string cpuFilename = string(split.back());
                string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(cpuFilename);
                std::filesystem::copy_options copyOptions = std::filesystem::copy_options::overwrite_existing;
                std::filesystem::copy_file(GetCachePath() + edit.EditedFile, outputPath + cpuFilename, copyOptions);
                std::filesystem::copy_file(fmt::format("{}{}\\{}", GetCachePath(), split[0], gpuFilename), outputPath + gpuFilename, copyOptions);

                //Add <Replace> blocks for the cpu file and the gpu file
                auto* replaceCpuFile = changes->InsertNewChildElement("Replace");
                replaceCpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), cpuFilename).c_str());
                replaceCpuFile->SetAttribute("NewFile", string(split.back()).c_str());

                auto* replaceGpuFile = changes->InsertNewChildElement("Replace");
                replaceGpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), gpuFilename).c_str());
                replaceGpuFile->SetAttribute("NewFile", gpuFilename.c_str());
            }
        }
    }

    //Save modinfo.xml
    modinfo.InsertFirstChild(modBlock);
    modinfo.SaveFile((outputPath + "modinfo.xml").c_str());

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
