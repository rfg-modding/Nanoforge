#include "Registry.h"
#include "common/filesystem/Path.h"
#include <ranges>
#include <string>
#include "Log.h"
#include "pugixml/src/pugixml.hpp"
#include "common/timing/Timer.h"
#include "common/filesystem/File.h"
#include "common/string/String.h"
#include <charconv>

Registry& Registry::Get()
{
    static Registry gRegistry;
    return gRegistry;
}

ObjectHandle Registry::CreateObject(std::string_view objectName, std::string_view typeName)
{
    _objectCreationLock.lock();
    const u64 uid = _nextUID++;
    _objects[uid] = Object(uid, NullUID, objectName);
    ObjectHandle handle = { &_objects[uid] };
    _objectCreationLock.unlock();
    if (typeName != "")
        handle.Property("Type").Set(string(typeName));

    return handle;
}

ObjectHandle Registry::CreateObjectInternal(u64 uid, u64 parentUID, std::string_view objectName, std::string_view typeName)
{
    //Don't allow duplicate UIDs
    _objectCreationLock.lock_shared(); //Allow other threads to read while checking for existing uid
    if (_objects.contains(uid))
    {
        _objectCreationLock.unlock_shared();
        return { nullptr };
    }
    _objectCreationLock.unlock_shared();

    //Create object
    _objectCreationLock.lock();//Block other threads from accessing _objects during write
    _objects[uid] = Object(uid, parentUID, objectName);
    _objectCreationLock.unlock();

    //Setup type if provided
    std::shared_lock<std::shared_mutex> lock(_objectCreationLock);
    ObjectHandle handle = { &_objects[uid] };
    if (typeName != "")
        handle.Property("Type").Set(string(typeName));

    return handle;
}

ObjectHandle Registry::GetObjectHandleByUID(u64 uid)
{
    std::shared_lock<std::shared_mutex> lock(_objectCreationLock); //Shared locks used during reads
    if (!_objects.contains(uid))
        THROW_EXCEPTION("Failed to find object with uid '{}'", uid);

    return { &_objects[uid] };
}

BufferHandle Registry::CreateBuffer(std::span<u8> bytes, std::string_view name)
{
    _bufferCreationLock.lock();
    const u64 uid = _nextBufferUID++;
    _buffers[uid] = RegistryBuffer(uid, name, bytes.size_bytes());
    BufferHandle handle = { &_buffers[uid] };
    _bufferCreationLock.unlock();

    handle.Save(bytes);
    return handle;
}

BufferHandle Registry::CreateBufferInternal(u64 uid, std::string_view name, size_t size)
{
    //Don't allow duplicate UIDs
    _bufferCreationLock.lock_shared(); //Allow other threads to read while checking for existing uid
    if (_buffers.contains(uid))
    {
        _bufferCreationLock.unlock_shared();
        return { nullptr };
    }
    _bufferCreationLock.unlock_shared();

    std::lock_guard<std::shared_mutex> lock(_bufferCreationLock); //Block other threads from reading/writing _buffers during write
    _buffers[uid] = RegistryBuffer(uid, name, size);
    BufferHandle handle = { &_buffers[uid] };
    return handle;
}

u64 Registry::NumObjects() const
{
    return _objects.size();
}

bool Registry::ObjectExists(u64 uid)
{
    std::shared_lock<std::shared_mutex> lock(_objectCreationLock);
    auto find = _objects.find(uid);
    return find != _objects.end();
}

//Convert string_view to integer/floating point type. Avoids using std::to_string() and needing to allocating a string.
template<typename T>
T ConvertStringView(std::string_view str, T valueDefault = (T)0)
{
    //Comptime check that T is supported. Compiler will yell at you already for passing a T to std::from_chars() with no overload. This just provides a more clear error message.
    static_assert(
        std::is_same<T, i64>() ||
        std::is_same<T, i32>() ||
        std::is_same<T, i16>() ||
        std::is_same<T, i8>()  ||
        std::is_same<T, u64>() ||
        std::is_same<T, u32>() ||
        std::is_same<T, u16>() ||
        std::is_same<T, u8>()  ||
        std::is_same<T, f32>() ||
        std::is_same<T, f64>(), "Unsupported type T used on ConvertStringView<T>()");

    T value = valueDefault;
    std::from_chars(str.data(), str.data() + str.size(), value);
    return value;
}

//Split string read from serialized registry file into a list of views for each value. E.g. "{1, 2, 3, 4}" -> std::vector<std::string_view>{"1", "2", "3", "4"}
std::vector<std::string_view> SplitTextArray(std::string_view str)
{
    //Remove { and } and any whitespace from ends to get comma separated list
    std::string_view commaSeparated = String::TrimWhitespace(str);
    commaSeparated.remove_prefix(1);
    commaSeparated.remove_suffix(1);
    std::vector<std::string_view> values = String::SplitString(commaSeparated, ","); //Separate values by comma
    for (std::string_view& value : values)
        value = String::TrimWhitespace(value); //Trim whitespace from each value

    return values;
}

ObjectHandle Registry::LoadObject(const std::vector<std::string_view>& lines, size_t entryLineStart)
{
    //Parse buffer header. E.g. Object(1812312921321):
    size_t i = 0;
    std::vector<std::string_view> objectSplit = String::SplitString(lines[i], "Object(");
    if (objectSplit.size() != 1)
    {
        Log->error("Failed to parse registry object. Invalid header '{}'", lines[i]);
        return NullObjectHandle;
    }
    std::string_view uidStrView = objectSplit[0];
    uidStrView.remove_suffix(2); //Remove "):" from end
    u64 uid = ConvertStringView<u64>(uidStrView, NullUID); //uid string to u64

    //Create buffer in memory
    ObjectHandle object = CreateObjectInternal(uid);
    if (!object)
    {
        Log->error("Failed to create object. UID = {}, Registry.registry line {}", uid, entryLineStart + i);
        return NullObjectHandle;
    }
    i++;

    //Parse properties
    while (i < lines.size())
    {
        //Split property E.g. name : type = value
        std::string_view line = String::TrimWhitespace(lines[i]);
        std::vector<std::string_view> split = String::SplitString(line, "=");
        std::vector<std::string_view> nameAndType = split.size() == 2 ? String::SplitString(split[0], ":") : std::vector<std::string_view>{};
        if (split.size() != 2)
        {
            Log->info("Bad syntax on line {}. Expected 'name : type = value'", entryLineStart + i);
            return NullObjectHandle;
        }

        std::string_view name = String::TrimWhitespace(nameAndType[0]);
        std::string_view type = nameAndType.size() == 2 ? String::TrimWhitespace(nameAndType[1]) : "";
        std::string_view value = String::TrimWhitespace(split[1]);

        //Parse value
        if (type == "empty")
        {
            object.Property(name).Set<std::monostate>({});
        }
        else if (type == "i32")
        {
            object.Property(name).Set<i32>(ConvertStringView<i32>(value));
        }
        else if (type == "i8")
        {
            object.Property(name).Set<i8>(ConvertStringView<i8>(value));
        }
        else if (type == "u64")
        {
            object.Property(name).Set<u64>(ConvertStringView<u64>(value));
        }
        else if (type == "u32")
        {
            object.Property(name).Set<u32>(ConvertStringView<u32>(value));
        }
        else if (type == "u16")
        {
            object.Property(name).Set<u16>(ConvertStringView<u16>(value));
        }
        else if (type == "u8")
        {
            object.Property(name).Set<u8>(ConvertStringView<u8>(value));
        }
        else if (type == "f32")
        {
            object.Property(name).Set<f32>(ConvertStringView<f32>(value));
        }
        else if (type == "bool")
        {
            object.Property(name).Set<bool>(value == "true" ? true : false);
        }
        else if (type == "string")
        {
            //Remove quotation marks from start and end of string
            value.remove_prefix(1);
            value.remove_suffix(1);
            object.Property(name).Set<string>(string(value));
        }
        else if (type == "ObjectHandleList")
        {
            object.Property(name).SetObjectList({}); //Ensure property set to correct type
            std::vector<std::string_view> handles = SplitTextArray(value); //Extract handles from array
            for (std::string_view handle : handles)
            {
                //Fill handle list
                u64 handleUid = ConvertStringView<u64>(String::TrimWhitespace(handle));
                object.Property(name).GetObjectList().push_back(ObjectHandle{ (Object*)handleUid });
            }
        }
        else if (type == "StringList")
        {
            object.Property(name).SetStringList({});
            std::vector<std::string_view> strings = SplitTextArray(value); //Extract handles from array
            for (std::string_view str : strings)
            {
                //Remove quotes from front and back
                str.remove_prefix(1);
                str.remove_suffix(1);

                object.Property(name).GetStringList().push_back(string(str));
            }
        }
        else if (type == "Vec3")
        {
            std::vector<std::string_view> xyz = SplitTextArray(value); //Extract x, y, z from array
            if (xyz.size() != 3)
            {
                Log->info("Invalid Vec3 property at line {}. Had {} values, expected 3 (x,y,z). Skipping object.", entryLineStart + i, xyz.size());
                _objects.erase(uid);
                return NullObjectHandle;
            }

            Vec3 value;
            value.x = ConvertStringView<f32>(xyz[0]);
            value.y = ConvertStringView<f32>(xyz[1]);
            value.z = ConvertStringView<f32>(xyz[2]);
            object.Property(name).Set<Vec3>(value);
        }
        else if (type == "Mat33")
        {
            std::vector<std::string_view> values = SplitTextArray(value); //Extract values from array
            if (values.size() != 9)
            {
                Log->info("Invalid Mat3 property at line {}. Had {} values, expected 9 (x,y,z for right, up, and forward vecs). Skipping object.", entryLineStart + i, values.size());
                _objects.erase(uid);
                return NullObjectHandle;
            }

            Mat3 value;
            value.rvec.x = ConvertStringView<f32>(values[0]);
            value.rvec.y = ConvertStringView<f32>(values[1]);
            value.rvec.z = ConvertStringView<f32>(values[2]);
            value.uvec.x = ConvertStringView<f32>(values[3]);
            value.uvec.y = ConvertStringView<f32>(values[4]);
            value.uvec.z = ConvertStringView<f32>(values[5]);
            value.fvec.x = ConvertStringView<f32>(values[6]);
            value.fvec.y = ConvertStringView<f32>(values[7]);
            value.fvec.z = ConvertStringView<f32>(values[8]);
            object.Property(name).Set<Mat3>(value);
        }
        else if (type == "ObjectHandle")
        {
            u64 handleUid = ConvertStringView<u64>(value);
            object.Property(name).Set<ObjectHandle>((Object*)handleUid);
        }
        else if (type == "BufferHandle")
        {
            u64 handleUid = ConvertStringView<u64>(value);
            object.Property(name).Set<BufferHandle>((RegistryBuffer*)handleUid);
        }
        else
        {
            Log->error("Invalid registry object property. Unsupported type '{}'. Skipping property...", type);
            i++;
            continue;
        }

        i++;
    }

    return object;
}

BufferHandle Registry::LoadBuffer(const std::vector<std::string_view>& lines, size_t entryLineStart)
{
    //Remove front and back from buffer def
    std::string_view bufferDef = lines[0]; //Buffer definitions are 1 line only currently
    bufferDef.remove_prefix(strlen("Buffer("));
    bufferDef.remove_suffix(strlen(")"));

    //Split out buffer definition arguments
    std::vector<std::string_view> split = String::SplitString(bufferDef, ",");
    if (split.size() != 3)
    {
        Log->error("Registry buffer definition had {} arguments. Expected 3. Skipping. Definition: '{}'", split.size(), bufferDef);
        return NullBufferHandle;
    }

    //Parse arguments
    u64 uid = ConvertStringView<u64>(String::TrimWhitespace(split[0]));
    size_t size = ConvertStringView<u64>(String::TrimWhitespace(split[2]));
    std::string_view nameArg = String::TrimWhitespace(split[1]);
    nameArg.remove_prefix(1); //Remove front qoutation mark
    nameArg.remove_suffix(1); //Remove back qoutation mark
    std::string_view name = nameArg;

    return CreateBufferInternal(uid, name, size);
}

bool Registry::Load(const string& inFolderPath)
{
    _ready = false;
#ifdef DEBUG_BUILD
    Timer timer;
    timer.Start();
#endif
    //Clear existing state
    _objects.clear();
    _buffers.clear();

    _folderPath = inFolderPath;
    std::filesystem::create_directories(inFolderPath + "Buffers\\");
    string registryFilePath = inFolderPath + "\\Registry.registry";
    if (!std::filesystem::exists(registryFilePath))
        return true;

    string fileBuffer = File::ReadToString(registryFilePath);
    std::vector<std::string_view> entries = String::SplitString(fileBuffer, ";"); //Each entry ends with a semicolon

    //Load objects
    u64 highestUID = 0;
    u64 highestBufferUID = 0;
    size_t entryLineStart = 0;
    for (std::string_view entryText : entries)
    {
        bool parseError = false;
        std::vector<std::string_view> lines = String::SplitString(entryText, "\r\n"); //Get view on each line
        for (size_t i = 0; i < lines.size(); i++)
        {
            //Skip empty lines
            std::string_view lineTrimmed = String::TrimWhitespace(lines[i]);
            if (lineTrimmed.empty())
            {
                continue;
            }
            else if (String::StartsWith(lineTrimmed, "Object("))
            {
                lines = { lines.begin() + i, lines.end() }; //Discard empty lines
                ObjectHandle object = LoadObject(lines, entryLineStart + i);
                entryLineStart += lines.size();
                u64 uid = object.UID();
                if (uid != NullUID && uid > highestUID)
                    highestUID = uid;

                break;
            }
            else if (String::StartsWith(lineTrimmed, "Buffer("))
            {
                lines = { lines.begin() + i, lines.end() }; //Discard empty lines
                BufferHandle buffer = LoadBuffer(lines, entryLineStart + i);
                u64 uid = buffer.UID();
                if (uid != NullUID && uid > highestBufferUID)
                    highestBufferUID = uid;

                break;
            }
            else
            {
                Log->error("Unknown/unsupported entry in registry file '{}'", lineTrimmed);
                parseError = true;
                break;
            }

        }
        if (!parseError)
            entryLineStart += lines.size();
    }
    _nextUID = highestUID + 1;
    _nextBufferUID = highestBufferUID + 1;

    //Patch ObjectHandle internal pointers
    auto patchObjectHandle = [](ObjectHandle& handle)
    {
        Registry& registry = Registry::Get();
        u64 uid = (u64)handle._object; //Stage one stores uid in handle._object since all objects aren't loaded yet
        Object* object = &registry._objects[uid];
        handle._object = object; //By stage two all objects are loaded, so we get the actual Object* and patch it
    };
    auto patchBufferHandle = [](BufferHandle& handle)
    {
        Registry& registry = Registry::Get();
        u64 uid = (u64)handle._buffer;
        RegistryBuffer* buffer = &registry._buffers[uid];
        handle._buffer = buffer;
    };
    for (auto& kv : _objects)
    {
        Object& object = kv.second;
        //Patch properties
        for (RegistryProperty& prop : object.Properties)
        {
            if (std::holds_alternative<ObjectHandle>(prop.Value))
            {
                patchObjectHandle(std::get<ObjectHandle>(prop.Value));
            }
            else if (std::holds_alternative<BufferHandle>(prop.Value))
            {
                patchBufferHandle(std::get<BufferHandle>(prop.Value));
            }
            else if (std::holds_alternative<std::vector<ObjectHandle>>(prop.Value))
            {
                std::vector<ObjectHandle>& handleList = std::get<std::vector<ObjectHandle>>(prop.Value);
                for (ObjectHandle& handle : handleList)
                    patchObjectHandle(handle);
            }
        }
    }

#ifdef DEBUG_BUILD
    Log->info("Loading Registry took {}s", timer.ElapsedSecondsPrecise());
#endif
    _ready = true;
    return true;
}

bool Registry::Save(const string& outFolderPath)
{
#ifdef DEBUG_BUILD
    Timer timer;
    timer.Start();
#endif
    Path::CreatePath(outFolderPath);

    string fileBuffer; //Generate file in memory then write all at once
    fileBuffer.reserve(_objects.size() * 1200); //Rough guess of 1000 characters per buffer in text form. Prevents a bunch of reallocations during generation

    //Write objects
    for (auto& kv : _objects)
    {
        Object& object = kv.second;
        fileBuffer += fmt::format("Object({}):\n", object.UID);

        //Write properties
        for (RegistryProperty& prop : object.Properties)
        {
            string propType = "";
            string propValue = "";
            if (std::holds_alternative<std::monostate>(prop.Value)) //Default state of std::variant used to store properties. Has no value
            {
                propType = "empty";
                propValue = "{{}}";
            }
            else if (std::holds_alternative<i32>(prop.Value))
            {
                propType = "i32";
                propValue = std::to_string(std::get<i32>(prop.Value));
            }
            else if (std::holds_alternative<i8>(prop.Value))
            {
                propType = "i8";
                propValue = std::to_string(std::get<i8>(prop.Value));
            }
            else if (std::holds_alternative<u64>(prop.Value))
            {
                propType = "u64";
                propValue = std::to_string(std::get<u64>(prop.Value));
            }
            else if (std::holds_alternative<u32>(prop.Value))
            {
                propType = "u32";
                propValue = std::to_string(std::get<u32>(prop.Value));
            }
            else if (std::holds_alternative<u16>(prop.Value))
            {
                propType = "u16";
                propValue = std::to_string(std::get<u16>(prop.Value));
            }
            else if (std::holds_alternative<u8>(prop.Value))
            {
                propType = "u8";
                propValue = std::to_string(std::get<u8>(prop.Value));
            }
            else if (std::holds_alternative<f32>(prop.Value))
            {
                propType = "f32";
                propValue = std::to_string(std::get<f32>(prop.Value));
            }
            else if (std::holds_alternative<bool>(prop.Value))
            {
                propType = "bool";
                propValue = std::get<bool>(prop.Value) ? "true" : "false";
            }
            else if (std::holds_alternative<string>(prop.Value))
            {
                propType = "string";
                propValue = "\"" + std::get<string>(prop.Value) + "\"";
            }
            else if (std::holds_alternative<std::vector<ObjectHandle>>(prop.Value))
            {
                propType = "ObjectHandleList";
                std::vector<ObjectHandle>& handles = std::get<std::vector<ObjectHandle>>(prop.Value);
                propValue = "{";
                for (size_t i = 0; i < handles.size(); i++)
                {
                    propValue += std::to_string(handles[i].UID());
                    if (i < handles.size() - 1)
                        propValue += ", ";
                }
                propValue += "}";
            }
            else if (std::holds_alternative<std::vector<string>>(prop.Value))
            {
                propType = "StringList";
                std::vector<string>& strings = std::get<std::vector<string>>(prop.Value);
                propValue = "{";
                for (size_t i = 0; i < strings.size(); i++)
                {
                    const string& str = strings[i];
                    propValue += fmt::format("\"{}\"", str);
                    if (i < strings.size() - 1)
                        propValue += ", ";
                }
                propValue += "}";
            }
            else if (std::holds_alternative<Vec3>(prop.Value))
            {
                propType = "Vec3";
                Vec3& value = std::get<Vec3>(prop.Value);
                propValue = fmt::format("{{{}, {}, {}}}", value.x, value.y, value.z);
                //Note: Syntax above might look odd. The first two curly braces are doubled to escape fmt formatting and will result in a single {. The third will be replaced by fmt with value.x
                //      Final result is a single { at the start of the vector and a single } at the end with arbitrary x, y, z values between. E.g {1.2, 400.2, -120.2}
            }
            else if (std::holds_alternative<Mat3>(prop.Value))
            {
                propType = "Mat33";
                Mat3& value = std::get<Mat3>(prop.Value);
                propValue = fmt::format("{{{}, {}, {}, {}, {}, {}, {}, {}, {}}}", value.rvec.x, value.rvec.y, value.rvec.z, value.uvec.x, value.uvec.y, value.uvec.z, value.fvec.x, value.fvec.y, value.fvec.z);
            }
            else if (std::holds_alternative<ObjectHandle>(prop.Value))
            {
                propType = "ObjectHandle";
                propValue = std::to_string(std::get<ObjectHandle>(prop.Value).UID());
            }
            else if (std::holds_alternative<BufferHandle>(prop.Value))
            {
                propType = "BufferHandle";
                propValue = std::to_string(std::get<BufferHandle>(prop.Value).UID());
            }
            else
            {
                Log->error("Unsupported PropertyValue type for property '{}', object uid = {}", prop.Name, object.UID);
                return false;
            }

            fileBuffer += fmt::format("\t{} : {} = {}\n", prop.Name, propType, propValue);
        }

        fileBuffer += ";\n\n"; //End of object + a few empty lines for spacing
    }

    //Write buffers
    for (auto& kv : _buffers)
    {
        RegistryBuffer& buffer = kv.second;
        fileBuffer += fmt::format("Buffer({}, \"{}\", {});\n", buffer.UID, buffer.Name, buffer.Size);
    }

    string outPath = outFolderPath + "\\Registry.registry";
    File::WriteTextToFile(outPath, fileBuffer);

#ifdef DEBUG_BUILD
    Log->info("Saving Registry took {}s", timer.ElapsedSecondsPrecise());
#endif
    return true;
}

string Registry::Path() const
{
    return _folderPath;
}

std::vector<ObjectHandle> Registry::FindObjects(std::string_view name, std::string_view type)
{
    if (name == "" && type == "")
        return {};

    std::vector<ObjectHandle> results = {};
    for (auto& kv : _objects)
    {
        Object& object = kv.second;
        bool match = false;
        if (name != "")
        {
            if (object.Has("Name") && std::get<string>(object.Property("Name").Value) == name)
                match == true;
            else
                continue;
        }
        if (type != "")
        {
            if (object.Has("Type") && std::get<string>(object.Property("Type").Value) == type)
                match = true;
            else
                continue;
        }
        if (match)
            results.push_back({ &object });
    }
    return results;
}

ObjectHandle Registry::FindObject(std::string_view name, std::string_view type, const bool locking)
{
    if (locking) _objectCreationLock.lock();
    for (auto& kv : _objects)
    {
        Object& object = kv.second;
        bool match = false;
        if (name != "")
        {
            if (object.Has("Name") && std::get<string>(object.Property("Name").Value) == name)
                match == true;
            else
                continue;
        }
        if (type != "")
        {
            if (object.Has("Type") && std::get<string>(object.Property("Type").Value) == type)
                match = true;
            else
                continue;
        }

        if (match)
        {
            if (locking) _objectCreationLock.unlock();
            return ObjectHandle{ &object };
        }
    }

    if (locking) _objectCreationLock.unlock();
    return NullObjectHandle;
}

BufferHandle Registry::FindBuffer(std::string_view name, const bool locking)
{
    if (locking) _bufferCreationLock.lock();
    for (auto& kv : _buffers)
    {
        RegistryBuffer& buffer = kv.second;
        if (buffer.Name == name)
        {
            if (locking) _bufferCreationLock.unlock();
            return { &buffer };
        }
    }
    if (locking) _bufferCreationLock.unlock();
    return NullBufferHandle;
}

std::vector<ObjectHandle> Registry::Objects()
{
    std::vector<ObjectHandle> handles = {};
    for (auto& kv : _objects)
        handles.push_back(ObjectHandle(&kv.second));

    return handles;
}

void Registry::DeleteObject(ObjectHandle obj)
{
    //TODO: Somehow do this better. Maybe have ObjectHandle reference a handle table in Registry. That way that data can be edited once to invalidate all handles
    //Invalidate any references to this object
    _objectCreationLock.lock();
    for (auto& kv : _objects)
    {
        ObjectHandle obj2 = { &kv.second };
        if (obj2 == obj)
            continue;

        for (PropertyHandle prop : obj2.Properties())
        {
            if (prop.IsType<ObjectHandle>())
            {
                if (prop.Get<ObjectHandle>() == obj)
                    prop.Set<ObjectHandle>(NullObjectHandle);
            }
            else if (prop.IsType<std::vector<ObjectHandle>>())
            {
                std::vector<ObjectHandle> handles = prop.Get<std::vector<ObjectHandle>>();
                for (size_t i = 0; i < handles.size(); i++)
                    if (handles[i] == obj)
                        handles[i] = NullObjectHandle;
            }
        }
    }

    _objects.erase(obj.UID());
    _objectCreationLock.unlock();
}

bool Registry::Ready()
{
    return _ready;
}

Object& Registry::GetObjectByUID(u64 uid)
{
    if (!_objects.contains(uid))
        THROW_EXCEPTION("Failed to find object with uid '{}'", uid);

    return _objects[uid];
}

ObjectHandle::ObjectHandle(Object* object) : _object(object)
{

}

u64 ObjectHandle::UID() const
{
    return _object ? _object->UID : NullUID;
}

bool ObjectHandle::Valid() const
{
    return _object && _object->UID != NullUID;
}

//Allows checking handle validity with if statements. E.g. if (handle) { /*valid*/ } else { /*invalid*/ }
ObjectHandle::operator bool() const
{
    return Valid();
}

PropertyHandle ObjectHandle::Property(std::string_view name)
{
    if (!_object)
        THROW_EXCEPTION("Attempted to get property from a null object.");

    //Search for property
    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    for (size_t i = 0; i < _object->Properties.size(); i++)
        if (_object->Properties[i].Name == name)
            return { _object, (u32)i }; //Property found

    //Property not found. Add it to the buffer and return its handle
    RegistryProperty& prop = _object->Properties.emplace_back();
    prop.Name = string(name);
    return { _object, (u32)(_object->Properties.size() - 1) };
}

bool ObjectHandle::Has(std::string_view propertyName)
{
    if (!_object)
        return false;

    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    for (size_t i = 0; i < _object->Properties.size(); i++)
        if (_object->Properties[i].Name == propertyName)
            return true;

    return false;
}

std::vector<PropertyHandle> ObjectHandle::Properties()
{
    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    std::vector<PropertyHandle> properties = {};
    for (u32 i = 0; i < _object->Properties.size(); i++)
        properties.emplace_back(_object, i);

    return properties;
}

bool ObjectHandle::operator==(ObjectHandle other)
{
    return _object == other._object;
}

std::vector<ObjectHandle>& ObjectHandle::GetObjectList(std::string_view propertyName)
{
    return Property(propertyName).GetObjectList();
}

std::vector<string>& ObjectHandle::GetStringList(std::string_view propertyName)
{
    return Property(propertyName).GetStringList();
}

void ObjectHandle::SetObjectList(std::string_view propertyName, const std::vector<ObjectHandle>& value)
{
    Property(propertyName).SetObjectList(value);
}

void ObjectHandle::SetStringList(std::string_view propertyName, const std::vector<string>& value)
{
    Property(propertyName).SetStringList(value);
}

bool ObjectHandle::Remove(std::string_view propertyName)
{
    if (!_object)
        THROW_EXCEPTION("Attempted to use null object handle");

    //Search for property
    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    for (size_t i = 0; i < _object->Properties.size(); i++)
        if (_object->Properties[i].Name == propertyName)
        {
            _object->Properties.erase(_object->Properties.begin() + i);
            return true;
        }

    return false;
}

RegistryProperty& Object::Property(std::string_view name)
{
    std::lock_guard<std::mutex> lock(*Mutex.get());

    //Search for property
    for (RegistryProperty& prop : Properties)
        if (prop.Name == name)
            return prop;

    //Property not found. Add it to the buffer
    RegistryProperty& prop = Properties.emplace_back();
    prop.Name = string(name);
    return prop;
}

bool Object::Has(std::string_view propertyName)
{
    std::lock_guard<std::mutex> lock(*Mutex.get());
    for (RegistryProperty& prop : Properties)
        if (prop.Name == propertyName)
            return true;

    return false;
}

std::optional<std::vector<u8>> RegistryBuffer::Load()
{
    std::lock_guard<std::mutex> lock(*Mutex.get());
    string filePath = fmt::format("{}\\Buffers\\{}.{}.buffer", Registry::Get().Path(), Name, UID);
    if (!std::filesystem::exists(filePath))
        return {};

    return File::ReadAllBytes(filePath);
}

bool RegistryBuffer::Save(std::span<u8> bytes)
{
    std::lock_guard<std::mutex> lock(*Mutex.get());
    //Ensure buffers folder exists
    string buffersPath = fmt::format("{}\\Buffers\\", Registry::Get().Path());
    Path::CreatePath(buffersPath);

    //Save buffer to file
    string filePath = fmt::format("{}{}.{}.buffer", buffersPath, Name, UID);
    File::WriteToFile(filePath, bytes);
    Size = bytes.size_bytes();
    return true;
}

std::optional<std::vector<u8>> BufferHandle::Load()
{
    if (!_buffer)
        THROW_EXCEPTION("_buffer is nullptr in BufferHandle::Load()");

    return _buffer->Load();
}

bool BufferHandle::Save(std::span<u8> bytes)
{
    if (!_buffer)
        THROW_EXCEPTION("_buffer is nullptr in BufferHandle::Save()");

    return _buffer->Save(bytes);
}

string BufferHandle::Name()
{
    if (!_buffer)
        THROW_EXCEPTION("_buffer is nullptr in BufferHandle::Name()");

    std::lock_guard<std::mutex> lock(*_buffer->Mutex.get());
    return _buffer->Name;
}

void BufferHandle::SetName(const string& name)
{
    if (!_buffer)
        THROW_EXCEPTION("_buffer is nullptr in BufferHandle::SetName()");

    std::lock_guard<std::mutex> lock(*_buffer->Mutex.get());
    _buffer->Name = name;
}

u64 BufferHandle::UID() const
{
    if (!_buffer)
        THROW_EXCEPTION("_buffer is nullptr in BufferHandle::UID()");

    return _buffer->UID;
}

bool BufferHandle::Valid() const
{
    return _buffer && _buffer->UID != NullUID;
}

size_t BufferHandle::Size() const
{
    return _buffer->Size;
}

BufferHandle::operator bool() const
{
    return Valid();
}
