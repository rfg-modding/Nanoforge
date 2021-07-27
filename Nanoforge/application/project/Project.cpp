#include "Project.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "common/filesystem/File.h"
#include "util/RfgUtil.h"
#include "rfg/xtbl/XtblManager.h"
#include "Log.h"
#include <spdlog/fmt/fmt.h>
#include "RfgTools++/RfgTools++/formats/packfiles/Packfile3.h"
#include "RfgTools++/RfgTools++/formats/asm/AsmFile5.h"
#include "rfg/PackfileVFS.h"

bool Project::Load(const string& projectFilePath)
{
    Edits.clear();
    Path = Path::GetParentDirectory(projectFilePath);
    ProjectFilename = Path::GetFileName(projectFilePath);
    Cache.Load(GetCachePath());
    loaded_ = LoadProjectFile(projectFilePath);
    return loaded_;
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
        editPath->SetText(edit.EditedFilePath.c_str());
    }

    project.InsertFirstChild(projectBlock);
    project.SaveFile((Path + "\\" + ProjectFilename).c_str());

    return true;
}

void Project::Close()
{
    Name = "";
    Description = "";
    Author = "";
    Path = "";
    ProjectFilename = "";
    UnsavedChanges = false;
    UseTableWorkaround = false;
    Edits.clear();
    WorkerRunning = false;
    WorkerFinished = false;
    PackagingCancelled = false;
    WorkerState = "";
    WorkerPercentage = 0.0f;
    loaded_ = false;
}

void Project::PackageMod(const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager)
{
    WorkerRunning = true;
    WorkerResult = std::async(std::launch::async, &Project::PackageModThread, this, outputPath, vfs, xtblManager);
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

bool Project::PackageModThread(const string& outputPath, PackfileVFS* vfs, XtblManager* xtblManager)
{
    //Reset public thread state
    WorkerState = "";
    WorkerPercentage = 0.0f;
    PackagingCancelled = false;

    //Create output path
    std::filesystem::create_directories(outputPath);
    std::filesystem::copy_options copyOptions = std::filesystem::copy_options::overwrite_existing;

    //Delete files and folders from previous runs
    for(auto& path : std::filesystem::directory_iterator(outputPath))
        std::filesystem::remove_all(path);

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
    f32 numPackagingSteps = (f32)(Edits.size() * 3) + 1; //This will need to be done a better way once more edit types are supported
    f32 packagingStepSize = 1.0f / numPackagingSteps;

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
        std::vector<s_view> split = String::SplitString(edit.EditedFilePath, "\\");
        bool inContainer = split.size() == 3;

        //Todo: Define IAction interface that each edit/action will be implemented as. Put this behavior in that
        //Update modinfo, repack files if necessary, and copy edited files to output
        if (edit.EditType == "TextureEdit")
        {
            //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
            if (inContainer)
            {
                //Copy edited str2_pc contents to temp folder
                string parentPath = edit.EditedFilePath.substr(0, edit.EditedFilePath.find_last_of("\\"));
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
                WorkerPercentage += packagingStepSize;

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
                WorkerPercentage += packagingStepSize;

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

                WorkerPercentage += packagingStepSize;
            }
            else
            {
                //Todo: Put outputted files into subfolder based on vpp they're in to avoid name conflicts
                //Copy edited files from project cache to output folder
                WorkerState = fmt::format("Copying edited files to output folder...");
                string cpuFilename = string(split.back());
                string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(cpuFilename);
                std::filesystem::copy_file(GetCachePath() + edit.EditedFilePath, outputPath + cpuFilename, copyOptions);
                std::filesystem::copy_file(fmt::format("{}{}\\{}", GetCachePath(), split[0], gpuFilename), outputPath + gpuFilename, copyOptions);

                //Add <Replace> blocks for the cpu file and the gpu file
                auto* replaceCpuFile = changes->InsertNewChildElement("Replace");
                replaceCpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), cpuFilename).c_str());
                replaceCpuFile->SetAttribute("NewFile", string(split.back()).c_str());

                auto* replaceGpuFile = changes->InsertNewChildElement("Replace");
                replaceGpuFile->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(split[0]), gpuFilename).c_str());
                replaceGpuFile->SetAttribute("NewFile", gpuFilename.c_str());

                WorkerPercentage += 3.0f * packagingStepSize;
            }
        }
    }

    //Package xtbl edits
    WorkerState = fmt::format("Writing xtbl edits...");
    PackageXtblEdits(changes, vfs, xtblManager, outputPath);
    WorkerPercentage += packagingStepSize;

    //Save modinfo.xml
    string modinfoPath = outputPath + "modinfo.xml";
    modinfo.InsertEndChild(modBlock);
    modinfo.SaveFile(modinfoPath.c_str());

    WorkerPercentage = 1.0f;
    WorkerState = "Done!";
    WorkerRunning = false;
    WorkerFinished = true;

    return true;
}

bool Project::PackageXtblEdits(tinyxml2::XMLElement* changes, PackfileVFS* vfs, XtblManager* xtblManager, const string& outputPath)
{
    //Ensure that all xtbls in the edit list so edits from previous sessions are handled
    for (auto& edit : Edits)
    {
        if (edit.EditType != "Xtbl")
            continue;

        string xtblName = Path::GetFileName(edit.EditedFilePath);
        string vppName(String::SplitString(edit.EditedFilePath, "\\")[0]);
        xtblManager->ParseXtbl(vppName, xtblName);
    }

    //Propagate xtbl edits up from subnodes to get list of edited xtbls
    std::vector<Handle<XtblFile>> editedXtbls = {};
    for (auto& group : xtblManager->XtblGroups)
        for (auto& xtbl : group->Files)
            if (xtbl->PropagateEdits()) //Returns true if any subnode has been edited
                editedXtbls.push_back(xtbl);

    //Ensure edited xtbls are in the edit list and resave the nanoproj file
    for (auto& xtbl : editedXtbls)
    {
        bool found = false;
        for (auto& edit : Edits)
            if (edit.EditType == "Xtbl" && edit.EditedFilePath == xtbl->Name)
                found = true;

        if (!found)
            Edits.push_back({ "Xtbl", fmt::format("{}\\{}", xtbl->VppName, xtbl->Name) });
    }
    Save(); //Save to ensure that newly edited xtbls are added to the .nanoproj file

    //Todo: Make a general purpose saving/change tracking system for all documents that would handle this and warn the user when data is unsaved
    //Save edited xtbls to project cache
    for (auto& xtbl : editedXtbls)
    {
        //Path to output folder in the project cache
        string outputFolderPath = GetCachePath() + xtbl->VppName + "\\";
        string outputFilePath = outputFolderPath + xtbl->Name;

        //Ensure output folder exists
        std::filesystem::create_directories(outputFolderPath);

        //Save xtbl project cache and rescan cache to see edited files in it
        xtbl->WriteXtbl(outputFilePath);
        RescanCache();
    }

    //Write xtbl edits to modinfo.xml. New entries are written by the next for loop
    for (auto& xtbl : editedXtbls)
    {
        u32 numEditedEntries = 0;
        for (auto& node : xtbl->Entries)
            if (node->Edited && !node->NewEntry)
                numEditedEntries++;

        //Don't write this edit block if there's no valid data for it
        if (numEditedEntries == 0)
            continue;

        //Add <Edit> block for each xtbl being edited
        bool hasCategory = xtbl->GetNodeCategoryPath(xtbl->Entries[0]) != "";
        auto* edit = changes->InsertNewChildElement("Edit");
        edit->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(xtbl->VppName), xtbl->Name).c_str());

        //Determine how the mod manager should identify each entry. Category is only used if the node has one set.
        if(hasCategory)
            edit->SetAttribute("LIST_ACTION", "COMBINE_BY_FIELD:Name,_Editor\\Category");
        else
            edit->SetAttribute("LIST_ACTION", "COMBINE_BY_FIELD:Name");

        //Find and write edits
        for (auto& node : xtbl->Entries)
        {
            if (!node->Edited || node->NewEntry)
                continue;

            auto* entry = edit->InsertNewChildElement(node->Name.c_str());
            auto nameNode = node->GetSubnode("Name");
            string name = nameNode ? std::get<string>(nameNode->Value) : "";
            string category = xtbl->GetNodeCategoryPath(node);

            //Write name and category which are used to identify nodes
            if (nameNode)
            {
                auto* nameXml = entry->InsertNewChildElement("Name");
                nameXml->SetText(name.c_str());
            }
            if (hasCategory)
            {
                auto* editorXml = entry->InsertNewChildElement("_Editor");
                auto* categoryXml = editorXml->InsertNewChildElement("Category");
                categoryXml->SetText(category.c_str());
            }

            //Writes edits based on implementation of IXtblNode::WriteModinfoEdits()
            if (!node->WriteModinfoEdits(entry))
            {
                Log->error("Xtbl modinfo packing failed for subnode: \"{}\\{}\"", node->GetPath(), name);
                return false;
            }
        }
    }

    //Todo: Come up with a better way of handling this. This is pretty much a duplicate of the for loop above with some small changes. May require mod manager changes.
    //Write new entries to modinfo.xml
    for (auto& xtbl : editedXtbls)
    {
        if (xtbl->VppName == "table.vpp_pc")
            continue;

        u32 numNewEntries = 0;
        for (auto& node : xtbl->Entries)
            if (node->NewEntry)
                numNewEntries++;

        //Don't write this edit block if there's no valid data for it
        if (numNewEntries == 0)
            continue;

        //Add <Edit> block for each xtbl being edited
        bool hasCategory = xtbl->GetNodeCategoryPath(xtbl->Entries[0]) != "";
        auto* edit = changes->InsertNewChildElement("Edit");
        edit->SetAttribute("File", fmt::format("data\\{}.vpp\\{}", Path::GetFileNameNoExtension(xtbl->VppName), xtbl->Name).c_str());

        //Find and write new entries. These are written in their entirety since they dont exist in the base xtbl
        for (auto& node : xtbl->Entries)
        {
            if (!node->NewEntry)
                continue;

            auto* entry = edit->InsertNewChildElement(node->Name.c_str());
            auto nameNode = node->GetSubnode("Name");
            string name = nameNode ? std::get<string>(nameNode->Value) : "";
            string category = xtbl->GetNodeCategoryPath(node);

            //Write category
            if (hasCategory)
            {
                auto* editorXml = entry->InsertNewChildElement("_Editor");
                auto* categoryXml = editorXml->InsertNewChildElement("Category");
                categoryXml->SetText(category.c_str());
            }

            //Writes edits based on implementation of IXtblNode::WriteModinfoEdits()
            if (!node->WriteXml(entry, false))
            {
                Log->error("Xtbl modinfo packing failed for subnode: \"{}\\{}\"", node->GetPath(), name);
                return false;
            }
        }
    }

    //Temporary workaround to support MP mods. Just repacks table.vpp_pc fully so players can install the mod manually.
    //Done this way since table.vpp_pc is required to use MP.
    //Changes to other files are still generated the normal way as a modinfo so you could apply the other changes via the MM and then paste table.vpp_pc
    //(July 25th, 2021) In the near future I want to finish a new mod manager that has MP support built in. That should remove the need for this hack.
    Packfile3* table = vfs->GetPackfile("table.vpp_pc");
    const u32 numTableEdits = std::ranges::count_if(editedXtbls, [](Handle<XtblFile> xtbl) { return xtbl->VppName == "table.vpp_pc"; });
    Log->info("{} files changed in table.vpp_pc", numTableEdits);
    if (UseTableWorkaround && table && numTableEdits > 0)
    {
        string tableFileCacheFolder = ".\\Cache\\table.vpp_pc\\";
        string tableFileTempFolder = outputPath + "tableRepackTemp\\";
        string tableFileOutPath = outputPath + "table.vpp_pc";
        Path::CreatePath(tableFileCacheFolder);
        Path::CreatePath(tableFileTempFolder);

        //Copy xtbls from global cache to temp folder. Assuming table.vpp_pc is already full extracted. Should be if anything was edited because it's a C&C packfile
        for (auto& entry : std::filesystem::directory_iterator(tableFileCacheFolder))
            if (!entry.is_directory())
                std::filesystem::copy_file(entry.path().string(), tableFileTempFolder + Path::GetFileName(entry.path().string()));

        //Save edited xtbl data to temp folder
        for (auto& xtbl : editedXtbls)
        {
            if (xtbl->VppName != "table.vpp_pc")
                continue;

            string xtblOutPath = tableFileTempFolder + xtbl->Name;
            xtbl->WriteXtbl(xtblOutPath, false);
        }

        //Repack temp folder and output new vpp_pc into mod output folder
        Packfile3::Pack(tableFileTempFolder, tableFileOutPath, true, true);

        //Cleanup temp folder
        std::filesystem::remove_all(tableFileTempFolder);
    }

    return true;
}