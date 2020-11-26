#include "Project.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "common/filesystem/File.h"
#include "util/RfgUtil.h"
#include "Log.h"
#include <tinyxml2/tinyxml2.h>
#include <spdlog/fmt/fmt.h>
#include "RfgTools++/RfgTools++/formats/packfiles/Packfile3.h"
#include "RfgTools++/RfgTools++/formats/asm/AsmFile5.h"
#include "rfg/PackfileVFS.h"

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

void Project::PackageMod(const string& outputPath, PackfileVFS* vfs)
{
    WorkerRunning = true;
    WorkerResult = std::async(std::launch::async, &Project::PackageModThread, this, std::ref(outputPath), vfs);
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

bool Project::PackageModThread(const string& outputPath, PackfileVFS* vfs)
{
    //Reset public thread state
    WorkerState = "";
    WorkerPercentage = 0.0f;
    PackagingCancelled = false;

    //Create output path
    std::filesystem::create_directories(outputPath);
    std::filesystem::copy_options copyOptions = std::filesystem::copy_options::overwrite_existing;

    //Create modinfo.xml and fill out basic info
    tinyxml2::XMLDocument modinfo;
    auto* modBlock = modinfo.NewElement("Mod");
    auto* author = modBlock->InsertNewChildElement("Author");
    auto* description = modBlock->InsertNewChildElement("Description");
    auto* changes = modBlock->InsertNewChildElement("Changes");
    modBlock->SetAttribute("Name", Name.c_str());
    author->SetText(Author.c_str());
    description->SetText(Description.c_str());

    //Each texture edit has 1-3 stages (in general):
    // - Repack str2_pc files (if texture is in str2_pc)
    // - Update asm_pc files (if texture is in str2_pc)
    // - Copy edited files to output folder
    f32 numSteps = (f32)(Edits.size() * 3); //This will need to be done a better way once more edit types are supported
    f32 stepSize = 1.0f / numSteps;

    //Loop through edits
    for (auto& edit : Edits)
    {
        //Check if cancel button was pressed
        if (PackagingCancelled)
        {
            WorkerRunning = false;
            return false;
        }

        //Split edited file cache path
        std::vector<s_view> split = String::SplitString(edit.EditedFile, "\\");
        bool inContainer = split.size() == 3;

        //Todo: Define IAction interface that each edit/action will be implemented as. Put this behavior in that
        //Update modinfo, repack files if necessary, and copy edited files to output
        if (edit.EditType == "TextureEdit")
        {
            //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
            if (inContainer)
            {
                //Copy edited str2_pc contents to temp folder
                string parentPath = edit.EditedFile.substr(0, edit.EditedFile.find_last_of("\\"));
                string tempPath = Path + "\\Temp\\" + parentPath + "\\";
                string str2Filename = string(split[1]);
                WorkerState = fmt::format("Repacking {}...", str2Filename);
                string packOutputPath = tempPath + "\\repack\\" + str2Filename;
                std::filesystem::create_directories(tempPath);
                std::filesystem::copy(GetCachePath() + parentPath + "\\", tempPath, copyOptions);

                //Copy unedited str2_pc contents to temp folder. We need all the files to repack the str2
                for (auto& entry : std::filesystem::directory_iterator(".\\Cache\\" + parentPath + "\\")) //Todo: De-hardcode global cache path
                {
                    //Skip file if it's already in the temp folder
                    if (entry.is_directory() || std::filesystem::exists(tempPath + entry.path().filename().string()))
                        continue;

                    //Otherwise copy into temp folder
                    std::filesystem::copy_file(entry.path(), tempPath + entry.path().filename().string(), copyOptions);
                }

                //Repack str2_pc from temp files
                std::filesystem::create_directory(tempPath + "\\repack\\");
                Packfile3::Pack(tempPath, packOutputPath, true, true);

                //Check if cancel button was pressed. Added to middle of edit step since they can sometimes take quite a while
                if (PackagingCancelled)
                {
                    WorkerRunning = false;
                    return false;
                }

                //Parse freshly packed str2
                Packfile3 newStr2(packOutputPath);
                newStr2.ReadMetadata();
                newStr2.SetName(str2Filename);

                //Get current asm_pc file
                string asmName;
                AsmFile5* currentAsm = nullptr;
                Packfile3* parentVpp = vfs->GetPackfile(Path::GetFileName(split[0]));
                for (auto& asmFile : parentVpp->AsmFiles)
                {
                    if (asmFile.HasContainer(Path::GetFileNameNoExtension(str2Filename)))
                    {
                        asmName = asmFile.Name;
                        currentAsm = &asmFile;
                    }
                }
                if (!currentAsm)
                {
                    Log->error("Failed to find asm_pc file for str2_pc file \"{}\" in mod packaging thread!", str2Filename);
                    return false;
                }
                WorkerState = fmt::format("Updating {}...", asmName);
                WorkerPercentage += stepSize;

                //Check if cancel button was pressed. Added to middle of edit step since they can sometimes take quite a while
                if (PackagingCancelled)
                {
                    WorkerRunning = false;
                    return false;
                }

                //Update asm_pc from freshly packed str2
                string asmOutputPath = GetCachePath() + string(split[0]) + "\\" + asmName;
                AsmFile5 newAsmFile;
                //If asm is already in the project cache use that
                if (std::filesystem::exists(asmOutputPath))
                {
                    BinaryReader reader(asmOutputPath);
                    newAsmFile.Read(reader, currentAsm->Name);
                }
                else //Otherwise use version from packfiles
                {
                    newAsmFile = *currentAsm;
                }

                //Get container
                AsmContainer* asmContainer = newAsmFile.GetContainer(Path::GetFileNameNoExtension(str2Filename));
                if (!asmContainer)
                {
                    Log->error("Failed to find container for str2_pc file \"{}\" in asmFile \"{}\" in in mod packaging thread!", str2Filename, asmName);
                    return false;
                }

                //Todo: Support adding/remove files from str2s. This assumes its the same files but edited
                //Update container values
                asmContainer->CompressedSize = newStr2.Header.CompressedDataSize;

                //Update primitive sizes
                for (u32 i = 0; i < asmContainer->PrimitiveSizes.size(); i++)
                {
                    asmContainer->PrimitiveSizes[i] = newStr2.Entries[i].DataSize; //Uncompressed size
                }

                //Update primitive values
                u32 primIndex = 0;
                u32 primSizeIndex = 0;
                while (primIndex < asmContainer->Primitives.size())
                {
                    AsmPrimitive& curPrim = asmContainer->Primitives[primIndex];

                    //Todo: This assumption blocks support of adding/remove files from str2s or reordering them
                    //If DataSize = 0 assume this primitive has no gpu file
                    if (curPrim.DataSize == 0)
                    {
                        curPrim.HeaderSize = newStr2.Entries[primSizeIndex].DataSize; //Uncompressed size
                        primIndex++;
                        primSizeIndex++;
                    }
                    else //Otherwise assume primitive has cpu and gpu file
                    {
                        curPrim.HeaderSize = newStr2.Entries[primSizeIndex].DataSize; //Cpu file uncompressed size
                        curPrim.DataSize = newStr2.Entries[primSizeIndex + 1].DataSize; //Gpu file uncompressed size
                        primIndex++;
                        primSizeIndex += 2;
                    }
                }

                //Resave asm_pc to project cache
                newAsmFile.Write(asmOutputPath);

                WorkerState = fmt::format("Copying edited files to output folder...");
                WorkerPercentage += stepSize;

                //Copy new str2 and asm files to output folder. Also copy asm to project folder
                std::filesystem::copy_file(packOutputPath, outputPath + str2Filename, copyOptions);
                std::filesystem::copy_file(asmOutputPath, outputPath + asmName, copyOptions);

                //Delete temp files
                for (auto& entry : std::filesystem::recursive_directory_iterator(tempPath))
                    if (!entry.is_directory())
                        std::filesystem::remove(entry);

                //Add <Replace> blocks for str2 and asm file
                auto* replaceStr2 = changes->InsertNewChildElement("Replace");
                replaceStr2->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), str2Filename).c_str());
                replaceStr2->SetAttribute("NewFile", str2Filename.c_str());

                auto* replaceAsm = changes->InsertNewChildElement("Replace");
                replaceAsm->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), asmName).c_str());
                replaceAsm->SetAttribute("NewFile", asmName.c_str());

                WorkerPercentage += stepSize;
            }
            else
            {
                //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
                //Copy edited files from project cache to output folder
                WorkerState = fmt::format("Copying edited files to output folder...");
                string cpuFilename = string(split.back());
                string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(cpuFilename);
                std::filesystem::copy_file(GetCachePath() + edit.EditedFile, outputPath + cpuFilename, copyOptions);
                std::filesystem::copy_file(fmt::format("{}{}\\{}", GetCachePath(), split[0], gpuFilename), outputPath + gpuFilename, copyOptions);

                //Add <Replace> blocks for the cpu file and the gpu file
                auto* replaceCpuFile = changes->InsertNewChildElement("Replace");
                replaceCpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), cpuFilename).c_str());
                replaceCpuFile->SetAttribute("NewFile", string(split.back()).c_str());

                auto* replaceGpuFile = changes->InsertNewChildElement("Replace");
                replaceGpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), gpuFilename).c_str());
                replaceGpuFile->SetAttribute("NewFile", gpuFilename.c_str());

                WorkerPercentage += 3.0f * stepSize;
            }
        }
    }

    //Save modinfo.xml
    modinfo.InsertFirstChild(modBlock);
    modinfo.SaveFile((outputPath + "modinfo.xml").c_str());

    WorkerRunning = false;

    return true;
}
