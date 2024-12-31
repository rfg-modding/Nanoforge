using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using RFGM.Formats.Vpp;
using RFGM.Formats.Vpp.Models;
using Serilog;

namespace Nanoforge.FileSystem;

//Lets you interact with RFG packfiles (.vpp_pc & .str2_pc version 3) like they're a normal filesystem. Tracks all the vpps in the mounted data folder.
//NOT thread safe. You better make some improvements to this before you try to use it on multiple threads simultaneously.
public static class PackfileVFS
{
    public static DirectoryEntry? Root = null;
    public static string DirectoryPath { get; private set; } = "";
    public static string Mount { get; private set; } = "";
    public static bool Loading { get; private set; } = false;
    public static bool Ready => Root != null && !Loading;
    
    public static void MountDataFolderInBackground(string mount, string directoryPath)
    {
        ThreadPool.QueueUserWorkItem(_ => MountDataFolder(mount, directoryPath));
    }
    
    private static void MountDataFolder(string mount, string directoryPath)
    {
        try
        {
            Loading = true;
            Mount = mount;
            DirectoryPath = directoryPath;
            Root = new DirectoryEntry(mount);
            

            int errorCount = 0;
            string[] packfilePaths = Directory.GetFiles(directoryPath, "*.vpp_pc", SearchOption.TopDirectoryOnly);
            foreach (string packfilePath in packfilePaths)
            {
                string packfileName = Path.GetFileName(packfilePath);
                if (!MountPackfile(packfilePath))
                {
                    Log.Error("PackfileVFS failed to mount {packfilePath} in the VFS.");
                    errorCount++;
                    continue;
                }
                
            }

        }
        catch (Exception ex)
        {
            Log.Error(ex, "Error while mounting data folder");
            throw;
        }
        finally
        {
            Loading = false;
        }
    }
    
    private static bool MountPackfile(string path, bool loadIntoMemory = false)
    {
        string packfileName = Path.GetFileName(path);
        
        VppReader reader = new VppReader();
        using var filestream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read);
        LogicalArchive packfile = reader.Read(filestream, packfileName, new CancellationToken());

        DirectoryEntry packfileEntry = new(packfileName, packfile)
        {
            Parent = Root,
        };
        Root!.Entries.Add(packfileEntry);

        foreach (LogicalFile file in packfile.LogicalFiles)
        {
            string filename = file.Name;
            string extension = Path.GetExtension(filename);
            if (extension == ".str2_pc") //Container. Create a directory entry with the name of the str2 and add its subfiles as FileEntry's
            {
                VppReader containerReader = new();
                LogicalArchive containerArchive = containerReader.Read(file.Content, filename, new CancellationToken(), onlyReadMetadata: true);
                DirectoryEntry containerEntry = new(filename, containerArchive);
                packfileEntry.Entries.Add(containerEntry);

                //TODO: DO NOT COMMIT - FOR TESTING
                // Stream str = file.Content;
                // str.Seek(1, SeekOrigin.Begin);
                // str.Seek(2, SeekOrigin.Begin);
                // str.Seek(0, SeekOrigin.Begin);
                
                foreach (var containerFile in containerArchive.LogicalFiles)
                {
                    FileEntry containerFileEntry = new(containerFile.Name, containerFile);
                    containerEntry.Entries.Add(containerFileEntry);
                }
            }
            else //Plain file. Make a FileEntry for it
            {
                FileEntry fileEntry = new(filename, file);
                packfileEntry.Entries.Add(fileEntry);
            }   
        }

        return true;
    }

    public static EntryBase? GetEntry(string path)
    {
        if (Root == null)
        {
            Log.Error("PackfileVFS.GetEntry() failed. No root directory. Mount the RFG directory first.");
            return null;
        }
        
        //Strip mount point. E.g. //data/
        if (!path.StartsWith(Mount))
        {
            Log.Error($"Invalid path '{path}' passed to VFS. Path must start with the mount point '{Mount}'");
            return null;
        }
        path = path.Remove(0, Mount.Length);
        if (path.Length == 0)
            return null;

        DirectoryEntry directory = Root;
        while (path.Length > 0)
        {
            int nextDelimiter = path.IndexOf("/", StringComparison.Ordinal);
            if (nextDelimiter == -1)
            {
                //No more slashes. We hit the end of the path on a file
                FileEntry? result = (FileEntry?)directory.Entries.Select(entry => entry)
                                                                 .FirstOrDefault(entry => entry.Name.Equals(path, StringComparison.OrdinalIgnoreCase) && entry.IsFile);
                
                return result;
            }
            else if (nextDelimiter == path.Length - 1)
            {
                //Slash on the end of the path. We hit the end of the path on a directory
                path = path.Substring(0, path.Length - 1); //Remove slash from end so we can find a directory with that name
                DirectoryEntry? result = (DirectoryEntry?)directory.Entries.Select(entry => entry)
                                                                           .FirstOrDefault(entry => entry.Name.Equals(path, StringComparison.OrdinalIgnoreCase) && entry.IsDirectory);
                
                return result;                
            }
            else
            {
                //Slash in the middle of the path. Find the next entry and proceed
                string part = path.Substring(0, nextDelimiter);
                path = path.Remove(0, nextDelimiter + 1);
                DirectoryEntry? result = (DirectoryEntry?)directory.Entries.Select(entry => entry)
                                                                           .FirstOrDefault(entry => entry.Name.Equals(part, StringComparison.OrdinalIgnoreCase) && entry.IsDirectory);

                if (result == null)
                    return null;
                
                directory = result;
            }
        }
        
        return null;
    }

    public static IEnumerable<EntryBase> Enumerate(string path)
    {
        EntryBase? entry = GetEntry(path);
        if (entry == null || !entry.IsDirectory)
            return [];
        
        return (entry as DirectoryEntry)!;
    }

    public static bool Exists(string path)
    {
        return GetEntry(path) != null;
    }

    public static Stream? OpenFile(string path)
    {
        EntryBase? entry = GetEntry(path);
        if (entry == null)
        {
            return null;
        }
        
        return OpenFile(entry);
    }

    public static Stream? OpenFile(EntryBase entry)
    {
        if (!entry.IsFile)
            return null;
        
        return (entry as FileEntry)?.LogicalFile.Content;
    }

    public static byte[]? ReadAllBytes(string path)
    {
        EntryBase? entry = GetEntry(path);
        if (entry == null)
        {
            return null;
        }

        return ReadAllBytes(entry);
    }
    
    public static byte[]? ReadAllBytes(EntryBase entry)
    {
        if (!entry.IsFile)
            return null;
        
        FileEntry fileEntry = (FileEntry)entry;
        using MemoryStream ms = new();        
        fileEntry.LogicalFile.Content.CopyTo(ms);
        return ms.ToArray();
    }

    public static string? ReadAllText(string path)
    {
        EntryBase? entry = GetEntry(path);
        if (entry == null)
        {
            return null;
        }

        return ReadAllText(entry);
    }
    
    public static string? ReadAllText(EntryBase entry)
    {
        if (!entry.IsFile)
            return null;
        
        FileEntry fileEntry = (FileEntry)entry;
        using StreamReader reader = new(fileEntry.LogicalFile.Content);
        return reader.ReadToEnd();
    }
}