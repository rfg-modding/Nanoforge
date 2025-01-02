using System;
using System.IO;
using System.Text;
using ICSharpCode.SharpZipLib.Zip.Compression.Streams;
using RFGM.Formats.Streams;
using RFGM.Formats.Vpp.Models;
using Serilog;

namespace Nanoforge.FileSystem;

//Primitive file - anything that isn't a vpp_pc or str2_pc
public class FileEntry(string name, long dataOffset = 0, uint size = 0, uint compressedSize = 0) : EntryBase(name, long.MaxValue, dataOffset, size, compressedSize)
{
    public override bool IsDirectory => false;

    public override bool IsFile => true;

    public byte[]? Data;
    public bool Preloaded => Data != null;

    public override Stream? OpenStream()
    {
        try
        {
            //If PackfileVFS.PreloadDirectory() is called then files can have their contents cached in memory
            if (Data != null)
            {
                return new MemoryStream(Data);
            }
            
            if (Parent == null)
                return null;

            DirectoryEntry parent = (DirectoryEntry)Parent!;
            bool compressed = parent.Compressed;
            bool condensed = parent.Condensed;
            bool compacted = compressed && condensed;

            using Stream? parentStream = parent.OpenStream();
            if (parentStream == null)
            {
                Log.Error($"Failed to open parent stream in FileEntry.OpenStream() for file entry '{Name}'");
                return null;
            }

            parentStream.Seek(0, SeekOrigin.Begin);
            if (compacted)
            {
                byte[] inflateBuffer = new byte[parent.DataBlockSize];

                //For compacted files we must decompress the entire block
                StreamView compressedView = new(parentStream, parent.DataBlockOffset, parent.DataBlockSizeCompressed);
                using InflaterInputStream inflaterStream = new(compressedView);
                int bytesRead = inflaterStream.Read(inflateBuffer);
                if (bytesRead != inflateBuffer.Length)
                {
                    Log.Error($"Failed to inflate data for file entry '{Name}'. Expected {inflateBuffer.Length} bytes but got {bytesRead}.");
                    return null;
                }

                //TODO: See if this could be done without an intermediate buffer by using StreamViews. Have to make sure inflation works properly if the StreamView offset is not 0
                byte[] justThisFilesBytes = new byte[Size];
                Array.Copy(sourceArray: inflateBuffer, sourceIndex: DataOffset,
                    destinationArray: justThisFilesBytes, destinationIndex: 0, length: justThisFilesBytes.Length);

                MemoryStream result = new(justThisFilesBytes);
                return result;
            }
            else if (compressed)
            {
                byte[] inflateBuffer = new byte[Size];
                using InflaterInputStream inflaterStream = new(parentStream);
                inflaterStream.Skip(parent.DataBlockOffset + DataOffset); //InflaterInputStream cannot seek
                
                var bytesRead = inflaterStream.Read(inflateBuffer);
                if (bytesRead != inflateBuffer.Length)
                {
                    Log.Error($"Failed to inflate data for file entry '{Name}'. Expected {inflateBuffer.Length} bytes but got {bytesRead}.");
                    return null;
                }
                
                MemoryStream result = new(inflateBuffer);
                return result;
            }
            else
            {
                byte[] bytes = new byte[Size];
                parentStream.Seek(parent.DataBlockOffset + DataOffset, SeekOrigin.Begin);
                int bytesRead = parentStream.Read(bytes);
                if (bytesRead != bytes.Length)
                {
                    Log.Error($"Failed to read data for file entry '{Name}'. Expected {bytes.Length} bytes but got {bytesRead}.");
                    return null;
                }

                MemoryStream result = new(bytes);
                return result;
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Exception in FileEntry.OpenStream() for {Name}. Ex: {ex.Message}");
            return null;
        }
    }

    public override byte[]? ReadAllBytes()
    {
        using Stream? stream = OpenStream();
        if (stream == null)
            return null;

        byte[] buffer = new byte[stream.Length];
        var bytesRead = stream.Read(buffer, 0, buffer.Length);
        if (bytesRead != buffer.Length)
        {
            Log.Error($"FileEntry.ReadAllBytes() failed to read {Name}. Expected {buffer.Length} bytes but got {bytesRead} bytes.");
            return null;
        }

        return buffer;
    }

    public override string? ReadAllText()
    {
        using Stream? stream = OpenStream();
        if (stream == null)
            return null;

        using StreamReader reader = new(stream);
        string result = reader.ReadToEnd();
        return result;
    }
}