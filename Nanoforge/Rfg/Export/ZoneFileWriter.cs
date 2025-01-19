using System;
using System.Collections.Generic;
using System.IO;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Misc;
using RFGM.Formats.Hashes;
using RFGM.Formats.Math;
using RFGM.Formats.Streams;
using RFGM.Formats.Zones;

namespace Nanoforge.Rfg.Export;

//TODO: Consider if this should be moved into RFGM.Formats
public class ZoneFileWriter : IDisposable
{
    public const uint RfgZoneFileVersion = 36;

    private bool _fileInProgress;

    private bool _objectInProgress;

    //Number of properties written so far on the current object
    private int _numPropertiesWritten;
    private int _numObjectsWritten;
    private long _objectBlockStart;
    private long _objectStartPos;
    private List<long> _objectOffsets = new(); //Used when generating relation data block
    private List<uint> _objectHandles = new();

    private Stream? _stream;
    private string _districtName = string.Empty;
    private DistrictFlags _districtFlags;
    private uint _districtHash;

    public long DataLength { get; private set; }

    ~ZoneFileWriter()
    {
        Dispose(false);
    }

    public void BeginFile(Stream stream, string districtName, DistrictFlags districtFlags, uint districtHash)
    {
        if (_fileInProgress)
        {
            throw new InvalidOperationException("File is already in progress");
        }

        _stream = stream;
        _stream.Seek(0, SeekOrigin.Begin);
        _districtName = districtName;
        _districtFlags = districtFlags;
        _districtHash = districtHash;

        //Skip the header for now. Wait until EndFile() is called to write it so we have all the stats collected.
        stream.Skip(24);

        bool hasRelationData = !_districtFlags.HasFlag(DistrictFlags.ZoneWithoutRelationData) && !_districtFlags.HasFlag(DistrictFlags.IsLayer);
        if (hasRelationData)
        {
            //Write nothing initially. Generated after writing every object since we need to object offsets
            stream.WriteNullBytes(87368);
        }

        _objectBlockStart = 0;
        _objectOffsets.Clear();
        _objectHandles.Clear();

        _fileInProgress = true;
    }

    //Call this once you're done writing the file. Writes information to the header that can't be known until the end, such as the number of objects.
    public void EndFile(bool closeStream = true)
    {
        if (!_fileInProgress || _stream == null)
        {
            throw new InvalidOperationException("File is not in progress. Call BeginFile() first.");
        }

        if (_objectInProgress)
        {
            EndObject();
        }

        const int maxObjects = 7280;
        if (_numObjectsWritten > maxObjects)
        {
            throw new InvalidOperationException($"Hit zone object limit of {_numObjectsWritten}");
        }

        //Write updated header data
        _stream.Seek(0, SeekOrigin.Begin);
        _stream.WriteAsciiString("ZONE");
        _stream.WriteUInt32(RfgZoneFileVersion);
        _stream.WriteUInt32((uint)_numObjectsWritten); //Num objects
        _stream.WriteUInt32(0); //Num handles
        _stream.WriteUInt32(_districtHash); //District hash
        _stream.WriteUInt32((uint)_districtFlags);

        //Note: For the moment we just keep the null bytes written for relation data in BeginFile().
        //      There are many indications that the re-mars-tered game doesn't use this section anymore. Namely that many files have junk data in this section and zeroing it causes no known issues.
        //      The commented out code below is a previous attempt at generating this section from scratch based on some of the zone files where this section wasn't full of junk.
        //Write relation data if needed
        /*bool hasRelationData = (_districtFlags & 5) != 0;
        if (hasRelationData)
        {
            mixin GetSlotIndex(uint key)
            {
                uint result = 0;
                result = ~(key << 15) + key;
                result = ((uint)((int)result >> 10) ^ result) * 9;
                result ^= (uint)((int)result >> 6);
                result += ~(result << 11);
                result = ((uint)((int)result >> 16) ^ result) % 7280;
                result
            }

            ZoneFile36.RelationData* hashTable = new .();
            defer delete hashTable;

            hashTable.Free = (u16)_numObjectsWritten; //Next free key/value index
            for (u16 i = 0; i < 7280; i++)
            {
                hashTable.Slot[i] = 65535;
                hashTable.Next[i] = i + 1;
                hashTable.Keys[i] = 0;
                hashTable.Values[i] = 0;
            }

            for (u16 i = 0; i < _numObjectsWritten; i++)
            {
                uint key = _objectHandles[i];
                uint value = (uint)_objectOffsets[i]; //Offset in bytes relative to the start of the object list
                uint slotIndex = GetSlotIndex!(key);
                hashTable.Slot[slotIndex] = i; //Note: Slot[slotIndex] doesn't always equal i in the vanilla files. Doesn't seem to be an issue with GetSlotIndex() since the games version of that function returns the same values
                hashTable.Keys[i] = key;
                hashTable.Values[i] = value;
            }

            //Writer hash table to file
            _stream.Seek(sizeof(ZoneFile36.Header), .Absolute); //This section is always right after the header
            _stream.Write<ZoneFile36.RelationData>(*hashTable);
        }*/

        if (closeStream)
        {
            _stream.Close();
        }

        _fileInProgress = false;
    }
    
    public void BeginObject(string classname, uint handle, uint num, ushort flags, Vector3 bmin, Vector3 bmax, uint parentHandle, uint firstSiblingHandle, uint firstChildHandle)
    {
        if (_objectInProgress)
        {
            throw new InvalidOperationException("Object is already in progress. You must call EndObject() before calling BeginObject() again.");
        }
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }

        const int maxObjects = 7280;
        if (_numObjectsWritten > maxObjects)
        {
            throw new InvalidOperationException($"Hit zone object limit of {_numObjectsWritten}");
        }

        //Validate that object offset doesn't exceed reasonable limits
        _objectStartPos = _stream.Position;
        long offset = _objectStartPos - _objectBlockStart;
        if (offset > (uint.MaxValue / 2))
        {
            //Just a sanity check. If the zone file got anywhere near this size (~2GB) it'd probably break the game
            throw new Exception("Zone file is too big.");
        }
        _objectOffsets.Add(offset);

        //Write the values we have for object header initially. We won't have the rest until after we write the properties
        _stream.WriteUInt32(Hash.HashVolition(classname));
        _stream.WriteUInt32(handle);
        _stream.WriteStruct<Vector3>(bmin);
        _stream.WriteStruct<Vector3>(bmax);
        _stream.WriteUInt16(flags);
        _stream.WriteUInt16(0); //Block size. Written in EndObject()
        _stream.WriteUInt32(parentHandle);
        _stream.WriteUInt32(firstSiblingHandle);
        _stream.WriteUInt32(firstChildHandle);
        _stream.WriteUInt32(num);
        _stream.WriteUInt16(0); //Number of properties. Written in EndObject()
        _stream.WriteUInt16(0); //Properties block size. Written in EndObject()
        _objectHandles.Add(handle);

        _objectInProgress = true;
    }

    public void EndObject()
    {
        if (!_objectInProgress)
        {
            throw new InvalidOperationException("Object is not in progress. You must call BeginObject() before calling EndObject().");
        }
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }

        if (_stream.Position > DataLength)
        {
            DataLength = _stream.Position;
        }

        //Calculate size of object block and prop block + make sure they're within allowed limits
        long objectEndPos = _stream.Position;
        const ushort objectHeaderSize = 56;
        long blockSize = objectEndPos - _objectStartPos;
        long propBlockSize = blockSize - objectHeaderSize;
        if (blockSize > ushort.MaxValue || propBlockSize > ushort.MaxValue || _numPropertiesWritten > ushort.MaxValue)
        {
            throw new Exception("Zone object properties block is too big. Either too many properties were written to one object or they are too large.");
        }

        //Write object header
        _stream.Seek(_objectStartPos, SeekOrigin.Begin);
        _stream.Skip(34); //Skip data we already wrote
        _stream.WriteUInt16((ushort)blockSize);
        _stream.Skip(16); //More data we already wrote
        _stream.WriteUInt16((ushort)_numPropertiesWritten);
        _stream.WriteUInt16((ushort)propBlockSize);
        _stream.Seek(objectEndPos, SeekOrigin.Begin); //Jump to end of object data so next object can be written

        _numObjectsWritten++;
        _objectInProgress = false;
        _numPropertiesWritten = 0;
    }
    
    public void WriteStringProperty(string propertyName, String value)
    {
        if (value.Length > ushort.MaxValue - 1)
        {
            throw new Exception($"Exceeded maximum zone object string property length. Property: '{propertyName}'");
        }
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }

        _stream.WriteUInt16(4); //Property type
        _stream.WriteUInt16((ushort)(value.Length + 1)); //Property size
        _stream.WriteUInt32(Hash.HashVolitionCRC(propertyName, 0)); //Property name hash
        _stream.WriteAsciiString(value);
        _stream.WriteNullBytes(1);
        _stream.AlignWrite(4);
        _numPropertiesWritten++;
    }

    public unsafe void WriteGenericProperty<T>(string propertyName, ushort propertyType, T value) where T : unmanaged
    {
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }

        _stream.WriteUInt16(propertyType); //Property type
        _stream.WriteUInt16((ushort)Unsafe.SizeOf<T>()); //Property size
        _stream.WriteUInt32(Hash.HashVolitionCRC(propertyName, 0)); //Property name hash
        _stream.WriteStruct<T>(value);
        _stream.AlignWrite(4);
        _numPropertiesWritten++;
    }
    
    public void WriteBoolProperty(string propertyName, bool value)
    {
        WriteGenericProperty<bool>(propertyName, 5, value);
    }
    
    public void WriteFloatProperty(string propertyName, float value)
    {
        WriteGenericProperty<float>(propertyName, 5, value);
    }

    public void WriteU32Property(string propertyName, uint value)
    {
        WriteGenericProperty<uint>(propertyName, 5, value);
    }

    public void WriteU16Property(string propertyName, ushort value)
    {
        WriteGenericProperty<ushort>(propertyName, 5, value);
    }

    public void WriteU8Property(string propertyName, byte value)
    {
        WriteGenericProperty<byte>(propertyName, 5, value);
    }

    public void WriteI32Property(string propertyName, int value)
    {
        WriteGenericProperty<int>(propertyName, 5, value);
    }

    public void WriteI16Property(string propertyName, short value)
    {
        WriteGenericProperty<short>(propertyName, 5, value);
    }

    public void WriteVec3Property(string propertyName, Vector3 value)
    {
        WriteGenericProperty<Vector3>(propertyName, 5, value);
    }

    public void WriteMat3Property(string propertyName, Matrix3x3 value)
    {
        WriteGenericProperty<Matrix3x3>(propertyName, 5, value);
    }

    public void WriteBoundingBoxProperty(string propertyName, BoundingBox value)
    {
        WriteGenericProperty<BoundingBox>(propertyName, 5, value);
    }
    
    public void WritePositionOrientProperty(Vector3 position, Matrix3x3 orientation)
    {
        PositionOrient op = new PositionOrient()
        {
            Position = position,
            Orient = orientation
        };
        WriteGenericProperty<PositionOrient>("op", 5, op);
    }

    public void WriteBuffer(string propertyName, Span<byte> bytes)
    {
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }
        if (bytes.Length > ushort.MaxValue - 1)
        {
            throw new Exception($"Exceeded maximum zone object buffer property length when loading zone property '{propertyName}'");
        }
        
        _stream.WriteUInt16(6); //Property type
        _stream.WriteUInt16((ushort)bytes.Length); //Property size
        _stream.WriteUInt32(Hash.HashVolitionCRC(propertyName, 0)); //Property name hash
        _stream.Write(bytes);
        _stream.AlignWrite(4);
        _numPropertiesWritten++;
    }
    
    //Helper to write plain enums to strings. For bitflags like TriggerRegionFlags use WriteFlagsEnum()
    public void WriteEnum<T>(string propertyName, T enumValue) where T : Enum
    {
        if (DataHelper.ToRfgName(enumValue, out string rfgName))
        {
            WriteStringProperty(propertyName, rfgName);
        }
        else
        {
            throw new Exception($"Failed to convert enum to string in ZoneFileWriter.WriteEnum<T>() for property '{propertyName}', type: {typeof(T).FullName}");
        }
    }

    //Helper to write bitflag enums. Stored as space separated flags in a string in zone files.
    public void WriteFlagsString<T>(string propertyName, T enumValue) where T : Enum
    {
        if (DataHelper.ToRfgFlagsString(enumValue, out string rfgFlagsString))
        {
            WriteStringProperty(propertyName, rfgFlagsString);
        }
        else
        {
            throw new Exception($"Failed to convert bitflag to rfg flags string in ZoneFileWriter.WriteFlagsString<T>() for property '{propertyName}', type: {typeof(T).FullName}");
        }
    }
    
    //Note: Using u8[] temporarily instead of a struct until SP editing is added.
    public void WriteConstraintTemplate(string propertyName, Span<byte> bytes)
    {
        if (_stream == null)
        {
            throw new InvalidOperationException("Stream not initialized. Call BeginFile() first.");
        }
        if (bytes.Length != 156)
        {
            throw new Exception($"Unexpected data size when writing constraint template zone property '{propertyName}'. Expected size is 156 bytes. Actual size is {bytes.Length} bytes");
        }
        if (bytes.Length > ushort.MaxValue - 1)
        {
            throw new Exception($"Exceeded maximum zone object buffer property length when loading constraint template zone property '{propertyName}'");
        }

        //Only difference from WriteBuffer is the property type
        _stream.WriteUInt16(5); //Property type
        _stream.WriteUInt16((ushort)bytes.Length); //Property size
        _stream.WriteUInt32(Hash.HashVolitionCRC(propertyName, 0)); //Property name hash
        _stream.Write(bytes);
        _stream.AlignWrite(4);
        _numPropertiesWritten++;
    }

    private void Dispose(bool disposing)
    {
        if (disposing)
        {
            if (_fileInProgress)
            {
                EndFile();
            }

            _stream?.Dispose();
        }
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }
}