using Nanoforge.App;
using System;
using System.Collections;
using Common;
using Nanoforge.Misc;
using System.IO;
using RfgTools.Formats;

namespace Nanoforge.Rfg.Export;

public class MapExporter
{
    public Result<void> ExportMap(Territory map, StringView outputFolder)
    {
        mixin ExitTask()
        {
            gTaskDialog.CanClose = true;
            return .Err;
        }

        //Setup background task dialog. Lets user see progress and blocks input/keybinds to prevent accidentally data races
        gTaskDialog.Show(numSteps: 6);

        //Copy cached files to the temp folder. We cache the contents of the vpp_pc on import so nothing breaks if the source vpp_pc is modified after import.
        gTaskDialog.SetStatus("Copying cached files to temp folder...");
        String tempFolder = scope $"{NanoDB.CurrentProject.Directory}Temp\\{map.Name}\\";
        if (Directory.Exists(tempFolder))
        {
            Directory.DelTree(tempFolder); //Delete data from previous exports
        }
        Directory.CreateDirectory(tempFolder);

        String importCacheFolder = scope $"{NanoDB.CurrentProject.Directory}Import\\{map.PackfileName}\\";
        if (Directory.CopyTree(importCacheFolder, tempFolder) case .Err(let err))
        {
            gTaskDialog.Log("Failed to copy cached files for map export!");
            Logger.Error("Failed to copy cached map files to temp directory for {}. Error: {}", map.PackfileName, err);
            ExitTask!();
        }
        gTaskDialog.Step();


        //Generate the new rfgzone_pc files
        gTaskDialog.SetStatus("Exporting zone files...");
        ZoneExporter zoneExporter = new .();
        defer delete zoneExporter;
        for (Zone zone in map.Zones)
        {
            String zoneFilePath = scope $"{tempFolder}{zone.Name}";
            String persistentZoneFilePath = scope $"{tempFolder}p_{zone.Name}";
            if (zoneExporter.Export(zone, zoneFilePath, persistent: false) case .Err)
            {
                gTaskDialog.Log(scope $"Failed to export {zone.Name}.rfgzone_pc");
                Logger.Error("Failed to export non persistent zone file '{0}' in '{1}'", zone.Name, map.Name);
                ExitTask!();
            }
            if (zoneExporter.Export(zone, persistentZoneFilePath, persistent: true) case .Err)
            {
                gTaskDialog.Log(scope $"Failed to export p_{zone.Name}.rfgzone_pc");
                Logger.Error("Failed to export persistent zone file '{0}' in '{1}'", zone.Name, map.Name);
                ExitTask!();
            }

            //Copy zone files to ns_base folder so they'll get repacked in that str2_pc
            if (File.Copy(zoneFilePath, scope $"{tempFolder}ns_base\\{zone.Name}") case .Err(let err))
            {
                gTaskDialog.Log(scope $"Failed to copy {zone.Name}.rfgzone_pc to ns_base");
                Logger.Error(scope $"Failed to copy {zone.Name}.rfgzone_pc to ns_base temp folder");
                ExitTask!();
            }
            if (File.Copy(persistentZoneFilePath, scope $"{tempFolder}ns_base\\p_{zone.Name}") case .Err(let err))
            {
                gTaskDialog.Log(scope $"Failed to copy p_{zone.Name} to ns_base");
                Logger.Error(scope $"Failed to copy p_{zone.Name} to ns_base temp folder");
                ExitTask!();
            }
        }
        gTaskDialog.Step();


        //Repack the str2_pc files
        //For now we only support MP & WC so the str2 names are hardcoded
        gTaskDialog.SetStatus("Repacking containers...");
        if (PackfileV3.Pack(scope $"{tempFolder}ns_base\\", scope $"{tempFolder}ns_base.str2_pc", true, true) case .Err)
        {
            gTaskDialog.Log("Failed to repack ns_base.str2_pc");
            Logger.Error("Failed to repack ns_base.str2_pc");
            ExitTask!();
        }
        if (PackfileV3.Pack(scope $"{tempFolder}Landmark LODs\\", scope $"{tempFolder}Landmark LODs.str2_pc", true, true) case .Err)
        {
            gTaskDialog.Log("Failed to repack Landmark LODs.str2_pc");
            Logger.Error("Failed to repack Landmark LODs.str2_pc");
            ExitTask!();
        }
        if (PackfileV3.Pack(scope $"{tempFolder}{map.Name}_map\\", scope $"{tempFolder}{map.Name}_map.str2_pc", true, true) case .Err)
        {
            gTaskDialog.Log(scope $"Failed to repack {map.Name}_map.str2_pc");
            Logger.Error(scope $"Failed to repack {map.Name}_map.str2_pc");
            ExitTask!();
        }
        gTaskDialog.Step();


        //Update the asm_pc file. It has offsets and sizes that must match the str2_pc files.
        gTaskDialog.SetStatus("Updating asset assembler...");
        if (UpdateAsmPc(scope $"{tempFolder}{map.Name}.asm_pc") case .Err)
        {
            gTaskDialog.Log(scope $"Failed to update {map.Name}.asm_pc");
            Logger.Error(scope $"Failed to update {map.Name}.asm_pc");
            ExitTask!();
        }
        gTaskDialog.Step();


        //Repack the vpp_pc file
        gTaskDialog.SetStatus(scope $"Repacking {map.PackfileName}...");
        String vppExportPath = scope $"{NanoDB.CurrentProject.Directory}Temp\\{map.PackfileName}";
        if (PackfileV3.Pack(tempFolder, vppExportPath, map.Compressed, map.Condensed) case .Err)
        {
            gTaskDialog.Log(scope $"Failed to repack {map.PackfileName}");
            Logger.Error(scope $"Failed to repack {map.PackfileName}");
            ExitTask!();
        }
        gTaskDialog.Step();


        //Copy the final vpp_pc file to the export directory and delete the temp folder
        gTaskDialog.SetStatus("Cleaning up...");
        if (File.Copy(vppExportPath, scope $"{outputFolder}\\{map.PackfileName}") case .Err(let err))
        {
            gTaskDialog.Log(scope $"Failed to copy {map.PackfileName} to export folder!");
            Logger.Error(scope $"Failed to copy {map.PackfileName} to export folder '{outputFolder}'!");
            ExitTask!();
        }
        if (Directory.Exists(tempFolder))
        {
            Directory.DelTree(tempFolder); //Delete data from previous exports
        }
        gTaskDialog.Step();

        gTaskDialog.SetStatus("Export complete!");
        gTaskDialog.CanClose = true;
        return .Ok;
    }

    private struct AsmPrimitiveSizeIndices
    {
        public int HeaderIndex = -1;
        public int DataIndex = -1;
    }

    private static Result<void> UpdateAsmPc(StringView asmPath)
    {
        //First read & parse the asm_pc file
        AsmFileV5 asmFile = scope .();
        FileStream stream = scope .();
        if (stream.Open(asmPath, .ReadWrite, .None) case .Err(let err))
        {
            Logger.Error("Failed to open '{}'. Error: {}", asmPath, err);
            return .Err;
        }
        if (asmFile.Read(stream, Path.GetFileName(asmPath, .. scope .())) case .Err(StringView err))
        {
            Logger.Error("Failed to parse '{}'. Error: {}", asmPath, err);
            return .Err;
        }

        //Update the asm_pc based on str2_pc files in the same folder
        //Note: We loop through the str2_pc files first since multiple asm containers can have primitives referencing the contents of a single str2_pc file.
        String parentDirectory = Path.GetDirectoryPath(asmPath, .. scope .())..Append("\\");
        for (var file in Directory.EnumerateFiles(parentDirectory))
        {
            String fileExt = file.GetExtension(.. scope .());
            if (file.IsDirectory || fileExt != ".str2_pc")
                continue;

            String str2Path = file.GetFilePath(.. scope .());
            PackfileV3 str2 = scope .(str2Path);
            if (str2.ReadMetadata() case .Err(StringView err))
            {
                Logger.Error("Failed to load '{}' for asm_pc update.", str2Path);
                return .Err;
            }

            for (AsmFileV5.Container container in asmFile.Containers)
            {
                int curPrimSizeIndex = 0;
                List<AsmPrimitiveSizeIndices> sizeIndices = scope .();
                for (AsmFileV5.Primitive prim in container.Primitives)
                {
                    AsmPrimitiveSizeIndices indices = .();
                    indices.HeaderIndex = curPrimSizeIndex++;
                    if (prim.DataSize == 0)
                    {
                        indices.DataIndex = -1; //This primitive has no gpu file and so it has no data index
                    }
                    else
                    {
                        indices.DataIndex = curPrimSizeIndex++;
                    }
                    sizeIndices.Add(indices);
                }
                readonly bool virtualFlag = ((u16)container.Flags & 512) != 0;
                readonly bool passiveFlag = ((u16)container.Flags & 256) != 0;
                readonly bool realFlag = ((u16)container.Flags & 128) != 0;
                readonly bool virtualContainer = (virtualFlag || passiveFlag) && (!realFlag);

                //Update CompressedSize and DataOffset on non-virtual containers. Don't update them on virtual ones since they aren't real files.
                if (!virtualContainer && StringView.Equals(Path.GetFileNameWithoutExtension(str2Path, .. scope .()), container.Name, true))
                {
                    container.CompressedSize = str2.Header.CompressedDataSize;

                    u64 dataStart = 0;
                    dataStart += 2048; //Header size
                    dataStart += (u64)str2.Entries.Count * 28; //Each entry is 28 bytes
                    dataStart += Stream.CalcAlignment(dataStart, 2048); //Align(2048) after end of entries
                    dataStart += str2.Header.NameBlockSize; //Filenames list
                    dataStart += Stream.CalcAlignment(dataStart, 2048); //Align(2048) after end of file names

                    container.DataOffset = (u32)dataStart;
                }

                for (int entryIndex = 0; entryIndex < str2.Entries.Count; entryIndex++)
                {
                    PackfileV3.Entry entry = str2.Entries[entryIndex];
                    for (int primitiveIndex = 0; primitiveIndex < container.Primitives.Count; primitiveIndex++)
                    {
                        ref AsmFileV5.Primitive primitive = ref container.Primitives[primitiveIndex];
                        AsmPrimitiveSizeIndices indices = sizeIndices[primitiveIndex];
                        if (StringView.Equals(primitive.Name, str2.EntryNames[entryIndex]))
                        {
                            if (entry.DataSize > (u32)i32.MaxValue)
                            {
                                Logger.Error("Failed to update {}. DataSize for {} in {} exceeded the maximum of {}", Path.GetFileName(asmPath, .. scope .()), primitive.Name, container.Name, i32.MaxValue);
                                return .Err;
                            }

                            //If DataSize = 0 assume this primitive has no gpu file
                            if (primitive.DataSize == 0)
                            {
                                primitive.HeaderSize = (i32)entry.DataSize; //Uncompressed size
                                if (!virtualContainer)
                                {
                                    container.PrimitiveSizes[indices.HeaderIndex] = primitive.HeaderSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                                }
                            }
                            else //Otherwise assume primitive has cpu and gpu file
                            {
                                u32 nextEntryDataSize = str2.Entries[entryIndex + 1].DataSize;
                                if (nextEntryDataSize > (u32)i32.MaxValue)
                                {
                                    Logger.Error("Failed to update {}. next DataSize for {} in {} exceeded the maximum of {}", Path.GetFileName(asmPath, .. scope .()), primitive.Name, container.Name, i32.MaxValue);
                                    return .Err;
                                }

                                primitive.HeaderSize = (i32)entry.DataSize; //Cpu file uncompressed size
                                primitive.DataSize = (i32)nextEntryDataSize; //Gpu file uncompressed size
                                if (!virtualContainer)
                                {
                                    container.PrimitiveSizes[indices.HeaderIndex] = primitive.HeaderSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                                    container.PrimitiveSizes[indices.DataIndex] = primitive.DataSize; //TODO: Just remove PrimitiveSizes from AsmFile and generate it on write
                                }
                            }
                        }
                    }
                }
            }
        }

        //Finally write the changed asm_pc file to the hard drive
        if (asmFile.Write(stream) case .Err(StringView err))
        {
            Logger.Error("Failed to write changes to '{}'. Error: {}", asmPath, err);
            return .Err;
        }

        return .Ok;
    }
}