#include "Importers.h"
#include "rfg/PackfileVFS.h"
#include "rfg/TextureIndex.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include <RfgTools++/formats/textures/PegFile10.h>

//TODO: Import this as a more general texture format. Right now it's very RFG specific
ObjectHandle Importers::ImportPegFromPath(std::string_view pegPath, TextureIndex* textureIndex, bool useExistingTextures)
{
    Registry& registry = Registry::Get();

    //Check if texture was already loaded
    if (useExistingTextures)
    {
        ObjectHandle objSearch = registry.FindObject(Path::GetFileName(pegPath), "Peg", true);
        if (objSearch)
            return objSearch;
    }

    std::optional<PegFile10> search = textureIndex->GetPegFromPath(pegPath);
    if (!search)
        return NullObjectHandle;

    PegFile10& pegFile = search.value();
    ObjectHandle peg = registry.CreateObject(Path::GetFileName(pegPath), "Peg");
    peg.Property("Signature").Set<u32>(pegFile.Signature);
    peg.Property("Version").Set<u16>(pegFile.Version);
    peg.Property("Platform").Set<u16>(pegFile.Platform);
    peg.Property("Flags").Set<u16>(pegFile.Flags);
    peg.Property("AlignValue").Set<u16>(pegFile.AlignValue);

    size_t i = 0;
    for (PegEntry10& entry : pegFile.Entries)
    {
        ObjectHandle entryObj = registry.CreateObject(entry.Name, "PegEntry");
        entryObj.Property("Width").Set<u16>(entry.Width);
        entryObj.Property("Height").Set<u16>(entry.Height);
        entryObj.Property("Format").Set<u16>((u16)entry.BitmapFormat); //PegFormat enum
        entryObj.Property("Flags").Set<u16>((u16)entry.Flags); //TextureFlags enum
        entryObj.Property("SourceWidth").Set<u16>(entry.SourceWidth);
        entryObj.Property("SourceHeight").Set<u16>(entry.SourceHeight);
        entryObj.Property("AnimTilesWidth").Set<u16>(entry.AnimTilesWidth);
        entryObj.Property("AnimTilesHeight").Set<u16>(entry.AnimTilesHeight);
        entryObj.Property("NumFrames").Set<u16>(entry.NumFrames);
        entryObj.Property("Fps").Set<u8>(entry.Fps);
        entryObj.Property("MipLevels").Set<u8>(entry.MipLevels);
        entryObj.Property("FrameSize").Set<u32>(entry.FrameSize);
        entryObj.Set<ObjectHandle>("Peg", peg); //Handle of peg that contains this texture

        BufferHandle pixelBuffer = registry.CreateBuffer(entry.RawData, fmt::format("{}_{}_{}", Path::GetFileName(pegPath), i, entry.Name));
        entryObj.Property("Pixels").Set(pixelBuffer);

        peg.Property("Entries").GetObjectList().push_back(entryObj);
        i++;
    }

    pegFile.Cleanup();
    return peg;
}

ObjectHandle GetPegEntry(ObjectHandle peg, std::string_view entryName)
{
    if (!peg.Has("Entries"))
        return NullObjectHandle;

    for (ObjectHandle entry : peg.GetObjectList("Entries"))
        if (entry.Has("Name") && String::EqualIgnoreCase(entry.Get<string>("Name"), entryName))
            return entry;

    return NullObjectHandle;
}