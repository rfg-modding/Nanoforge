using Common;
using System;
using System.IO;
using Common.Math;
using Nanoforge.Misc;
using RfgTools.Hashing;
using System.Collections;
using RfgTools.Formats.Zones;
using Nanoforge.App;

namespace Nanoforge.Rfg.Export
{
    public class ZoneFileWriter
    {
        private const u32 RFG_ZONE_VERSION = 36;

        private bool _fileInProgress = false;
        private bool _objectInProgress = false;
        //Number of properties written so far on the current object
        private int _numPropertiesWritten = 0;
        private int _numObjectsWritten = 0;
        private int _objectBlockStart = 0;
        private int _objectStartPos = 0;
        private append List<int> _objectOffsets = .(); //Used when generating relation data block
        private append List<u32> _objectHandles = .();

        private Stream _stream = null;
        private append String _districtName;
        private u32 _districtFlags = 0;

        [Reflect]
        public enum Error
        {
            StreamError,
            FileAlreadyInProgress,
            FileNotInProgress,
            ObjectAlreadyInProgress,
            ObjectNotInProgress,
            HitObjectLimit,
            FileTooBig,
            PropertiesTooBig
        }

        public this()
        {
        }

        public ~this()
    	{
            if (_fileInProgress)
            {
                EndFile();
            }
    	}

        public Result<void, ZoneFileWriter.Error> BeginFile(Stream stream, StringView districtName, u32 districtFlags)
        {
            if (_fileInProgress)
            {
                return .Err(.FileAlreadyInProgress);
            }
            stream = stream;
            _districtName.Set(districtName);
            _districtFlags = districtFlags;

            //Skip the header for now. Wait until EndFile() is called to write it so we have all the stats collected.
            stream.Skip(24);

            bool hasRelationData = (districtFlags & 5) != 0;
            if (hasRelationData)
            {
                //Write nothing initially. Generated after writing every object since we need to object offsets
                stream.WriteNullBytes(87368);
            }

            _objectBlockStart = 0;
            _objectOffsets.Clear();
            _objectHandles.Clear();

            return .Ok;
        }

        //Call this once you're done writing the file. Writes information to the header that can't be known until the end, such as the number of objects.
        public Result<void, ZoneFileWriter.Error> EndFile(bool closeStream = true)
        {
            if (!_fileInProgress)
            {
                return .Err(.FileNotInProgress);
            }
            if (_objectInProgress)
            {
                EndObject();
            }
            const int maxObjects = 7280;
            if (_numObjectsWritten > maxObjects || _numObjectsWritten > u32.MaxValue)
            {
                return .Err(.HitObjectLimit);
            }

            //Write updated header data
            _stream.Seek(0, .Absolute);
            _stream.Write("ZONE");
            _stream.Write<u32>(RFG_ZONE_VERSION);
            _stream.Write<u32>((u32)_numObjectsWritten); //Num objects
            _stream.Write<u32>(0); //Num handles
            _stream.Write<u32>(Hash.HashVolitionCRC(_districtName, 0)); //District hash
            _stream.Write<u32>(_districtFlags);

            //Note: For the moment we just keep the null bytes written for relation data in BeginFile().
		    //      There are many indications that the re-mars-tered game doesn't use this section anymore. Namely that many files have junk data in this section and zeroing it causes no known issues.
            //      The commented out code below is a previous attempt at generating this section from scratch based on some of the zone files where this section wasn't full of junk.
            //Write relation data if needed
            /*bool hasRelationData = (_districtFlags & 5) != 0;
            if (hasRelationData)
            {
                mixin GetSlotIndex(u32 key)
                {
                    u32 result = 0;
                    result = ~(key << 15) + key;
                    result = ((u32)((i32)result >> 10) ^ result) * 9;
                    result ^= (u32)((i32)result >> 6);
                    result += ~(result << 11);
                    result = ((u32)((i32)result >> 16) ^ result) % 7280;
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
                    u32 key = _objectHandles[i];
                    u32 value = (u32)_objectOffsets[i]; //Offset in bytes relative to the start of the object list
                    u32 slotIndex = GetSlotIndex!(key);
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
            return .Ok;
        }

        public Result<void, ZoneFileWriter.Error> BeginObject(StringView classname, u32 handle, u32 num, u16 flags, Vec3 bmin, Vec3 bmax, u32 parentHandle, u32 firstSiblingHandle, u32 firstChildHandle)
        {
            if (_objectInProgress)
            {
                return .Err(.ObjectAlreadyInProgress);
            }

            const int maxObjects = 7280;
            if (_numObjectsWritten > maxObjects)
            {
                return .Err(.HitObjectLimit);
            }

            //Validate that object offset doesn't exceed reasonable limits
            _objectStartPos = _stream.Position;
            readonly int offset = _objectStartPos - _objectBlockStart;
            if (offset > (u32.MaxValue / 2))
            {
                //Just a sanity check. If the zone file got anywhere near this size (~2GB) it'd probably break the game
                return .Err(.FileTooBig);
            }

            //Write the values we have for object header initially. We won't have the rest until after we write the properties
            _stream.Write<u32>(Hash.HashVolitionCRC(classname, 0));
            _stream.Write<u32>(handle);
            _stream.Write<Vec3>(bmin);
            _stream.Write<Vec3>(bmax);
            _stream.Write<u16>(flags);
            _stream.Write<u16>(0); //Block size. Written in EndObject()
            _stream.Write<u32>(parentHandle);
            _stream.Write<u32>(firstSiblingHandle);
            _stream.Write<u32>(firstChildHandle);
            _stream.Write<u32>(num);
            _stream.Write<u16>(0); //Number of properties. Written in EndObject()
            _stream.Write<u16>(0); //Properties block size. Written in EndObject()
            _objectHandles.Add(handle);

            _objectInProgress = true;
            return .Ok;
        }

        public Result<void, ZoneFileWriter.Error> EndObject()
        {
            if (!_objectInProgress)
            {
                return .Err(.ObjectNotInProgress);
            }

            //Calculate size of object block and prop block + make sure they're within allowed limits
            readonly i64 objectEndPos = _stream.Position;
            const u16 ObjectHeaderSize = 56;
            readonly i64 blockSize = objectEndPos - _objectStartPos;
            readonly i64 propBlockSize = blockSize - ObjectHeaderSize;
            if (blockSize > u16.MaxValue || propBlockSize > u16.MaxValue || _numPropertiesWritten > u16.MaxValue)
            {
                return .Err(.PropertiesTooBig);
            }

            //Write object header
            _stream.Seek(_objectStartPos, .Absolute);
            _stream.Skip(34); //Skip data we already wrote
            _stream.Write<u16>((u16)blockSize);
            _stream.Skip(16); //More data we already wrote
            _stream.Write<u16>((u16)_numPropertiesWritten);
            _stream.Write<u16>((u16)propBlockSize);
            _stream.Seek(objectEndPos, .Absolute); //Jump to end of object data so next object can be written

            _numObjectsWritten++;
            _objectInProgress = false;
            _numPropertiesWritten = 0;
            return .Ok;
        }

        public Result<void> WriteStringProperty(StringView propertyName, String value)
        {
            if (value.Length > u16.MaxValue - 1)
            {
                Logger.Error("Exceeded maximum zone object string property length. Property: '{}'", propertyName);
                return .Err;
            }

            _stream.Write<u16>(4); //Property type
            _stream.Write<u16>((u16)value.Length + 1); //Property size
            _stream.Write<u32>(Hash.HashVolitionCRC(propertyName, 0)); //Property name hash
            _stream.Write(value);
            _stream.WriteNullBytes(1);
            _stream.Align2(4);

            return .Ok;
        }

        public mixin WriteGenericProperty<T>(StringView propertyName, u16 propertyType, T value) where T : ValueType
        {
            Try!(_stream.Write<u16>(propertyType)); //Property type
            Try!(_stream.Write<u16>((u16)sizeof(T))); //Property size
            Try!(_stream.Write<u32>(Hash.HashVolitionCRC(propertyName, 0))); //Property name hash
            Try!(_stream.Write<T>(value));
            _stream.Align2(4);

            return .Ok;
        }

        public Result<void> WriteBoolProperty(StringView propertyName, bool value)
        {
            WriteGenericProperty!<bool>(propertyName, 5, value);
        }

        public Result<void> WriteFloatProperty(StringView propertyName, float value)
        {
            WriteGenericProperty!<f32>(propertyName, 5, value);
        }

        public Result<void> WriteU32Property(StringView propertyName, u32 value)
        {
            WriteGenericProperty!<u32>(propertyName, 5, value);
        }

        public Result<void> WriteU16Property(StringView propertyName, u16 value)
        {
            WriteGenericProperty!<u16>(propertyName, 5, value);
        }

        public Result<void> WriteU8Property(StringView propertyName, u8 value)
        {
            WriteGenericProperty!<u8>(propertyName, 5, value);
        }

        public Result<void> WriteI32Property(StringView propertyName, i32 value)
        {
            WriteGenericProperty!<i32>(propertyName, 5, value);
        }

        public Result<void> WriteI16Property(StringView propertyName, i16 value)
        {
            WriteGenericProperty!<i16>(propertyName, 5, value);
        }

        public Result<void> WriteVec3Property(StringView propertyName, Vec3 value)
        {
            WriteGenericProperty!<Vec3>(propertyName, 5, value);
        }

        public Result<void> WriteMat3Property(StringView propertyName, Mat3 value)
        {
            WriteGenericProperty!<Mat3>(propertyName, 5, value);
        }

        public Result<void> WriteBoundingBoxProperty(StringView propertyName, BoundingBox value)
        {
            WriteGenericProperty!<BoundingBox>(propertyName, 5, value);
        }

        public Result<void> WritePositionOrientationProperty(Vec3 position, Mat3 orientation)
        {
            Try!(_stream.Write<u16>(5)); //Property type
            Try!(_stream.Write<u16>(sizeof(Vec3) + sizeof(Mat3))); //Property size
            Try!(_stream.Write<u32>(Hash.HashVolitionCRC("op", 0))); //Property name hash
            Try!(_stream.Write(position));
            Try!(_stream.Write(orientation));
            _stream.Align2(4);

            return .Ok;
        }	

        public Result<void> WriteBuffer(StringView propertyName, ProjectBuffer buffer)
        {
            switch (buffer.Load())
            {
                case .Ok(List<u8> bytes):
                    defer delete bytes;
                    if (bytes.Count > u16.MaxValue - 1)
                    {
                        Logger.Error("Exceeded maximum zone object buffer property length when loading zone property '{}'", propertyName);
                        return .Err;
                    }

                    Try!(_stream.Write<u16>(6)); //Property type
                    Try!(_stream.Write<u16>((u16)bytes.Count)); //Property size
                    Try!(_stream.Write<u32>(Hash.HashVolitionCRC(propertyName, 0))); //Property name hash
                    Try!(_stream.TryWrite(bytes));
                    _stream.Align2(4);
                    return .Ok;

                case .Err:
                    Logger.Error(scope $"Failed to load project buffer '{buffer.Name}' while writing zone property '{propertyName}'.");
                    return .Err;
            }
        }

        //Helper to write plain enums to strings. For bitflags like TriggerRegionFlags use WriteFlagsEnum()
        public Result<void> WriteEnum<T>(StringView propertyName, T enumValue) where T : Enum
        {
            String str = scope .();
            Try!(Enum.ToRfgName<T>(enumValue, str));
            Try!(WriteStringProperty(propertyName, str));
            return .Ok;
        }

        //Helper to write bitflag enums. Stored as space separated flags in a string in zone files.
        public Result<void> WriteFlagsString<T>(StringView propertyName, T enumValue) where T : Enum
        {
            String str = scope .();
            Try!(Enum.ToRfgFlagsString<T>(enumValue, str));
            Try!(WriteStringProperty(propertyName, str));
            return .Ok;
        }

        //Note: Using ProjectBuffer temporarily instead of a struct until SP editing is added.
        public Result<void> WriteConstraintTemplate(StringView propertyName, ProjectBuffer data)
        {
            switch (data.Load())
            {
                case .Ok(List<u8> bytes):
                    defer delete bytes;
                    if (bytes.Count != 156)
                    {
                        Logger.Error("Unexpected data size when writing constraint template zone property '{}'. Expected size is 156 bytes. Actual size is {} bytes", propertyName, bytes.Count);
                        return .Err;
                    }
                    if (bytes.Count > u16.MaxValue - 1)
                    {
                        Logger.Error("Exceeded maximum zone object buffer property length when loading constraint template zone property '{}'", propertyName);
                        return .Err;
                    }

                    //Only difference from WriteBuffer is the property type
                    Try!(_stream.Write<u16>(5)); //Property type
                    Try!(_stream.Write<u16>((u16)bytes.Count)); //Property size
                    Try!(_stream.Write<u32>(Hash.HashVolitionCRC(propertyName, 0))); //Property name hash
                    Try!(_stream.TryWrite(bytes));
                    _stream.Align2(4);
                    return .Ok;

                case .Err:
                    Logger.Error(scope $"Failed to load project buffer '{data.Name}' while writing constraint template for zone property '{propertyName}'.");
                    return .Err;
            }
        }
    }
}