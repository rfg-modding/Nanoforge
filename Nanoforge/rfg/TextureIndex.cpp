#include "TextureIndex.h"
#include "common/filesystem/Path.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include "render/Render.h"
#include "gui/documents/PegHelpers.h"
#include "util/RfgUtil.h"
#include "gui/util/WinUtil.h"

std::optional<string> TextureIndex::FindPeg(const std::string_view pegName) const
{
    u64 hash = std::hash<string>{}(String::ToLower(pegName));
    for (PegData& peg : pegData_)
        if (peg.NameHash == hash)
            return string(pegPathsView_[peg.PathIndex]);

    return {};
}

std::optional<string> TextureIndex::FindTexture(const std::string_view textureName, bool tryHighResVariant) const
{
    //Find a high res variant of the texture
    if (tryHighResVariant)
    {
        string highResTextureName = String::Replace(string(textureName), "_low_", "_");
        std::optional<string> search = FindTexture(highResTextureName);
        if (search)
            return search.value();
    }

    //Find peg that contains the texture
    u64 hash = std::hash<string>{}(String::ToLower(textureName));
    for (TgaData& tga : tgaData_)
        if (tga.NameHash == hash)
            return string(pegPathsView_[pegData_[tga.PegIndex].PathIndex]) + "/" + string(textureName);

    return {};
}

std::optional<PegFile10> TextureIndex::GetPeg(const std::string_view pegName) const
{
    //Get peg path
    std::optional<string> search = FindPeg(pegName);
    if (!search)
        return {};

    //Extract peg files
    string cpuFilePath = search.value();
    string gpuFilePath = cpuFilePath.substr(0, cpuFilePath.find_last_of('/') + 1) + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
    std::optional<std::span<u8>> cpuFileBytes = packfileVFS_->GetFileBytes(cpuFilePath);
    std::optional<std::span<u8>> gpuFileBytes = packfileVFS_->GetFileBytes(gpuFilePath);
    defer(delete[] cpuFileBytes.value().data());
    defer(delete[] gpuFileBytes.value().data());
    if (!cpuFileBytes || !gpuFileBytes)
        return {};

    //Parse peg
    BinaryReader cpuFile(cpuFileBytes.value());
    BinaryReader gpuFile(gpuFileBytes.value());
    PegFile10 peg;
    peg.Read(cpuFile);
    peg.ReadAllTextureData(gpuFile); //Read textures into memory so the gpu file doesn't need to be passed around

    return peg;
}

std::optional<TextureSearchResult> TextureIndex::GetTexturePeg(const std::string_view textureName, bool tryHighResVariant) const
{
    //Get texture path
    std::optional<string> search = FindTexture(textureName, tryHighResVariant);
    if (!search)
        return {};

    //Extract tga name and parent peg from path
    std::vector<std::string_view> split = String::SplitString(search.value(), "/");
    if (split.size() < 3)
        return {}; //Size must be at least 3 for "packfile/peg/tga". May also be "packfile/container/peg/tga"

    std::string_view tgaName = split.back();
    std::string_view pegName = split[split.size() - 2];

    //Get parent peg
    std::optional<PegFile10> pegSearch = GetPeg(pegName);
    if (!pegSearch)
        return {};

    //Locate tga entry in peg
    PegFile10& peg = pegSearch.value();
    std::optional<u32> entrySearch = peg.GetEntryIndex(string(tgaName));
    if (!entrySearch)
    {
        peg.Cleanup();
        return {};
    }

    //Found the tga
    return TextureSearchResult { .Peg = peg, .TextureIndex = entrySearch.value() };
}

std::optional<std::span<u8>> TextureIndex::GetTexturePixels(const std::string_view textureName, bool tryHighResVariant) const
{
    //Get the peg that holds the texture
    std::optional<TextureSearchResult> search = GetTexturePeg(textureName, tryHighResVariant);
    if (!search)
        return {};

    //Make a copy of the pixel data
    PegFile10& peg = search.value().Peg;
    std::optional<std::span<u8>> maybePixels = peg.GetTextureData(search.value().TextureIndex, true);

    //Cleanup peg and return pixel data
    peg.Cleanup();
    return maybePixels.value();
}

std::optional<Texture2D> TextureIndex::GetRenderTexture(const std::string_view textureName, ComPtr<ID3D11Device> d3d11Device, bool tryHighResVariant) const
{
    //Locate peg
    std::optional<TextureSearchResult> search = GetTexturePeg(textureName, tryHighResVariant);
    if (!search)
        return {};

    PegFile10& peg = search.value().Peg;
    u32 textureIndex = search.value().TextureIndex;
    PegEntry10& entry = peg.Entries[textureIndex];

    //Extract pixel data
    std::optional<std::span<u8>> maybePixels = peg.GetTextureData(textureIndex);
    if (!maybePixels)
    {
        peg.Cleanup();
        return {};
    }
    std::span<u8> pixels = maybePixels.value();

    //One subresource data per mip level
    DXGI_FORMAT format = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat, peg.Flags);
    std::vector<D3D11_SUBRESOURCE_DATA> textureData = PegHelpers::CalcSubresourceData(entry, peg.Flags, pixels);
    D3D11_SUBRESOURCE_DATA* subresourceData = textureData.size() > 0 ? textureData.data() : nullptr;

    //Create renderer texture
    Texture2D texture;
    texture.Name = string(textureName);
    texture.Create(d3d11Device, entry.Width, entry.Height, format, D3D11_BIND_SHADER_RESOURCE, subresourceData, entry.MipLevels);
    texture.CreateShaderResourceView(); //Need shader resource view to use it in shader
    texture.CreateSampler(); //Need sampler too

    //Cleanup resources
    peg.Cleanup();

    return texture;
}

void TextureIndex::Load()
{
    const string path = "./TextureIndex.bin";
    if (!std::filesystem::exists(path))
    {
        ShowMessageBox("Failed to locate TextureIndex.bin in the Nanoforge folder. Texture searches won't work without it. It should've been included in the zip file you downloaded Nanoforge in. You can download it again at https://www.github.com/Moneyl/Nanoforge/releases", "TextureIndex.bin is missing!", MB_OK);
        Log->error("Failed to load texture search index. Couldn't locate {}", path);
        return;
    }

    //Cleanup buffers if they already exist
    if (tgaData_.data())
        delete[] tgaData_.data();
    if (pegData_.data())
        delete[] pegData_.data();
    if (pegPaths_.data())
        delete[] pegPaths_.data();

    pegPathsView_.clear();

    //Open file
    BinaryReader reader(path);

    //Read header
    u32 tgaBlockSize = reader.ReadUint32();
    u32 pegBlockSize = reader.ReadUint32();
    u32 pathBlockSize = reader.ReadUint32();
    reader.Align(4);

    //Allocate buffers for each data block
    u8* tgaBlock = new u8[tgaBlockSize];
    u8* pegBlock = new u8[pegBlockSize];
    u8* pathBlock = new u8[pathBlockSize];
    tgaData_ = { (TgaData*)tgaBlock, tgaBlockSize / sizeof(TgaData) };
    pegData_ = { (PegData*)pegBlock, pegBlockSize / sizeof(PegData) };
    pegPaths_ = { (char*)pathBlock, pathBlockSize };

    //Read tga data
    reader.ReadToMemory(tgaBlock, tgaBlockSize);
    reader.Align(4);

    //Read peg data
    reader.ReadToMemory(pegBlock, pegBlockSize);
    reader.Align(4);

    //Read peg paths
    reader.ReadToMemory(pathBlock, pathBlockSize);
    reader.Align(4);

    //Create string views for peg paths for easy access
    u32 i = 0;
    char* str = pegPaths_.data();
    while (i < pegPaths_.size())
    {
        char cur = pegPaths_[i++];
        if (cur == '\0')
        {
            u32 size = &pegPaths_[i - 1] - str;
            std::string_view view = { str, size };
            pegPathsView_.push_back(view);
            if (i < pegPaths_.size())
                str = &pegPaths_[i];
        }
    }
}

void TextureIndex::StartTextureIndexGeneration()
{
    TextureIndexGenTask = Task::Create("Generating texture search database");
    TaskScheduler::QueueTask(TextureIndexGenTask, std::bind(&TextureIndex::TextureIndexGenerationTask, this, TextureIndexGenTask));
}

void TextureIndex::TextureIndexGenerationTask(Handle<Task> task)
{
    TextureIndexGenTaskStatus = "Generating texture search index...";
    TextureIndexGenTaskProgressFraction = 0.0f;

    //Temporary texture data used during generation
    std::vector<PackfileTextureData> packfileTextureData; //tuple<string PegPath, u64 PegNameHash, vector<u64> TgaNameHash>
    packfileTextureData.reserve(packfileVFS_->packfiles_.size());

    std::vector<Handle<Task>> subTasks;
    subTasks.reserve(packfileVFS_->packfiles_.size());

    //Start a separate task for each packfile
    for (Packfile3& packfile : packfileVFS_->packfiles_)
    {
        Handle<Task> subtask = Task::Create(fmt::format("Indexing textures in {}...", packfile.Name()));
        subTasks.push_back(subtask);

        PackfileTextureData& textureData = packfileTextureData.emplace_back();
        TaskScheduler::QueueTask(subtask, std::bind(&TextureIndex::TextureIndexGenerationSubtask, this, subtask, packfile, &textureData));
    }

    //Wait for subtasks to finish
    for (Handle<Task> subtask : subTasks)
        subtask->Wait();

    //Calculate texture stats
    TextureIndexGenTaskStatus = "Calculating texture stats...";
    TextureIndexGenTaskProgressFraction = 0.9f;
    Log->info("Subtasks done! Calculating texture stats...");
    u64 numTgas = 0;
    u64 numPegs = 0;
    u64 totalPegPathsSize = 0;
    u64 longestPegPath = 0;
    for (auto& packfileData : packfileTextureData)
    {
        for (auto& peg : packfileData)
        {
            string& pegPath = std::get<0>(peg);
            u64 pegNameHash = std::get<1>(peg);
            std::vector<u64> tgaNameHashes = std::get<2>(peg);

            totalPegPathsSize += pegPath.size();
            numTgas += tgaNameHashes.size();
            numPegs++;

            if (longestPegPath < pegPath.size())
                longestPegPath = pegPath.size();
        }
    }

    //Print stats
    Log->info("Done calculating texture stats!");
    Log->info("Num tgas: {}, Num pegs: {}, Total peg paths size: {}, Longest peg path: {}", numTgas, numPegs, totalPegPathsSize, longestPegPath);

    TextureIndexGenTaskStatus = "Writing database binary file...";
    TextureIndexGenTaskProgressFraction = 0.95f;

    //Collect data for database file output
    std::vector<string> pegPaths = {};
    std::vector<PegData> pegs = {};
    std::vector<TgaData> tgas = {};
    for (auto& packfileData : packfileTextureData)
    {
        for (auto& peg : packfileData)
        {
            string& pegPath = std::get<0>(peg);
            u64 pegNameHash = std::get<1>(peg);
            std::vector<u64> tgaNameHashes = std::get<2>(peg);

            //Get indices
            u32 pathIndex = pegPaths.size();
            u32 pegIndex = pegs.size();

            //Push back data
            pegPaths.push_back(pegPath);
            pegs.push_back({ .NameHash = pegNameHash, .PathIndex = pathIndex });
            for (u64 tgaNameHash : tgaNameHashes)
                tgas.push_back({ .NameHash = tgaNameHash, .PegIndex = pegIndex });
        }
    }

    //Write index to file
    BinaryWriter writer("./TextureIndex.bin");

    //Calculate data block sizes
    u32 tgaBlockSize = tgas.size() * sizeof(TgaData);
    u32 pegBlockSize = pegs.size() * sizeof(PegData);
    u32 pathBlockSize = std::accumulate(pegPaths.begin(), pegPaths.end(), 0, [](u32 size, string& path) { return size + path.size() + 1; });

    //Write header
    writer.WriteUint32(tgaBlockSize);
    writer.WriteUint32(pegBlockSize);
    writer.WriteUint32(pathBlockSize);
    writer.Align(4);

    //Write tga data
    writer.WriteFromMemory(tgas.data(), tgaBlockSize);
    writer.Align(4);

    //Write peg data
    writer.WriteFromMemory(pegs.data(), pegBlockSize);
    writer.Align(4);

    //Write peg paths
    for (const string& path : pegPaths)
        writer.WriteNullTerminatedString(path);

    TextureIndexGenTaskStatus = "Done!";
    TextureIndexGenTaskProgressFraction = 1.0f;
}

void TextureIndex::TextureIndexGenerationSubtask(Handle<Task> task, Packfile3& packfile, PackfileTextureData* textureData)
{
    //Note: PackfileTextureData = std::vector<std::tuple<string, u64, std::vector<u64>>>

    //Reads the texture list from a peg and adds it to textureData
    auto readPegFile = [&](std::span<u8> pegBytes, const string& pegName, const string& path, bool deletePeg)
    {
        //Parse peg
        BinaryReader cpuFile(pegBytes);
        PegFile10 peg;
        peg.Read(cpuFile);

        //Data needed for PackfileTextureData entry
        string pegPath = path + pegName;
        u64 pegNameHash = std::hash<string>{}(String::ToLower(pegName));
        std::vector<u64> tgaNameHashes = {};

        //Fill tga list
        for (auto& entry : peg.Entries)
            tgaNameHashes.push_back(std::hash<string>{}(String::ToLower(entry.Name)));

        //Store peg data
        textureData->push_back({ pegPath, pegNameHash, tgaNameHashes });

        if (deletePeg)
            delete[] pegBytes.data();
    };

    //Loop through subfiles
    for (u32 i = 0; i < packfile.Entries.size(); i++)
    {
        const char* entryName = packfile.EntryNames[i];
        string ext = Path::GetExtension(entryName);

        //If entry is a cpeg, search it
        if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
        {
            std::optional<std::span<u8>> pegBytes = packfile.ExtractSingleFile(entryName, true);
            if (!pegBytes.has_value())
                THROW_EXCEPTION("Failed to extract {} during texture search database generation", entryName);

            readPegFile(pegBytes.value(), entryName, packfile.Name() + "/", true);
        }
        else if (ext == ".str2_pc") //Search for pegs
        {
            //Read container
            std::optional<std::span<u8>> containerBytes = packfile.ExtractSingleFile(entryName, true);
            if (!containerBytes.has_value())
                THROW_EXCEPTION("Failed to extract {} during texture search database generation", entryName);

            //Parse container
            Packfile3 container(containerBytes.value());
            container.ReadMetadata();
            container.SetName(entryName);

            //Read all subfiles at once since most str2 files are C&C and can't do single file extracts anyway.
            //Significantly faster in some packfiles like terr01_l0 which has many str2 files filled with textures
            std::vector<MemoryFile> subfiles = container.ExtractSubfiles();
            if (subfiles.size() > 0)
            {
                //Iterate container files, look for peg
                for (MemoryFile& subfile : subfiles)
                {
                    string containerEntryExt = Path::GetExtension(subfile.Filename);
                    if (containerEntryExt == ".cpeg_pc" || containerEntryExt == ".cvbm_pc")
                    {
                        //Found peg, search it
                        readPegFile(subfile.Bytes, subfile.Filename, packfile.Name() + "/" + container.Name() + "/", false);
                    }
                }

                //Cleanup subfiles. Only first subfile is deleted since they're in one buffer (I hate this design. Easy to mess up)
                delete[] subfiles[0].Bytes.data();
            }
        }
    }

    Log->info("Done indexing textures in {}", packfile.Name());
    TextureIndexGenTaskProgressFraction += 0.9f / packfileVFS_->packfiles_.size();
}