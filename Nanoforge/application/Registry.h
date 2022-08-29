#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include <RfgTools++/types/Vec3.h>
#include <RfgTools++/types/Mat3.h>
#include <map>
#include "util/Hash.h"
#include <shared_mutex>
#include <stdexcept>
#include <optional>
#include <vector>
#include <variant>
#include <limits>
#include "Log.h"
#include <span>

class Object;
class ObjectHandle;
class RegistryBuffer;
class BufferHandle;
class Property;
class PropertyHandle;

static const u64 NullUID = std::numeric_limits<u64>::max();

//Global data registry. Contains objects which consist of properties.
//The object format focuses on ease of editing instead of performance. Tracks edits for undo/redo.
//Used by tools and UI so all code works with the same format instead of needing unique code per RFG format.
class Registry
{
public:
    //Get global instance of the registry. You should use this to access it. You shouldn't create your own instance.
    static Registry& Get();
    //Create registry object. Returns a handle to the object if it succeeds, or InvalidHandle if it fails.
    ObjectHandle CreateObject(std::string_view objectName = "", std::string_view typeName = "");
    ObjectHandle GetObjectHandleByUID(u64 uid);
    BufferHandle CreateBuffer(std::span<u8> bytes = {}, std::string_view name = "");
    u64 NumObjects() const;
    bool ObjectExists(u64 uid);
    //Load registry from folder
    bool Load(const string& inFolderPath);
    //Save registry to folder
    bool Save(const string& outFolderPath);
    //Discard all registry data in memory. Any unsaved changes are gone.
    void Reset();
    //Filesystem location the registry is serialized to
    string Path() const;
    //Find objects with given name + type. Object must have Name & Type properties to check
    std::vector<ObjectHandle> FindObjects(std::string_view name, std::string_view type = "");
    //Same as ::FindObjects() but only returns the first result
    ObjectHandle FindObject(std::string_view name, std::string_view type = "", const bool locking = false);
    //Find first buffer with provided name. If locking == true it stops new buffers from being created during the search
    BufferHandle FindBuffer(std::string_view name, const bool locking = false);
    std::vector<ObjectHandle> Objects();
    void DeleteObject(ObjectHandle obj);
    bool Ready();

    //Buffers this size or smaller get merged into one buffer file. Done since windows file explorer freaks out, freezes, and has high cpu usage if you write tons of tiny files to one directory.
    //Some maps like terr01 could have 30000+ files smaller than 10KB without this.
    static constexpr u64 TinyBufferMaxSize = 256 * 1024;

private:
    ObjectHandle LoadObject(const std::vector<std::string_view>& lines, size_t entryLineStart); //Load object from lines of serialized registry. Returns null handle if it fails.
    BufferHandle LoadBuffer(const std::vector<std::string_view>& lines, size_t entryLineStart); //Same as ::LoadObject() but for buffers
    ObjectHandle CreateObjectInternal(u64 uid, u64 parentUID = NullUID, std::string_view objectName = "", std::string_view typeName = ""); //For internal use by Registry::Load() only
    BufferHandle CreateBufferInternal(u64 uid, std::string_view name, size_t size);

    std::optional<std::vector<u8>> LoadTinyBuffer(RegistryBuffer& buffer);
    bool SaveTinyBuffer(RegistryBuffer& buffer, std::span<u8> bytes);
    string GetMergedBufferPath() const;

    //Registry objects mapped to their UIDs
    //Note: If the container type is changed ObjectHandle and PropertyHandle likely won't be able to hold pointers anymore.
    //      They rely on the fact that pointers to std::map values are stable.
    std::map<u64, Object> _objects = {};
    std::map<u64, RegistryBuffer> _buffers = {};
    u64 _nextUID = 0;
    u64 _nextBufferUID = 0;

    //UID and offset of buffers where size <= TinyBufferMaxSize, which get merged into one file.
    std::unordered_map<u64, u64> _tinyBufferOffsets = {};
    std::mutex TinyBufferLock;

    //This type of mutex allows multiple readers or one writer. It's fine for multiple threads to read from _objects or _buffers at once, but writing must block access to them to prevent data races
    std::shared_mutex _objectCreationLock;
    std::shared_mutex _bufferCreationLock;
    string _folderPath;
    bool _ready = false;

    Object& GetObjectByUID(u64 uid);

    friend class ObjectHandle;
    friend class PropertyHandle;
    friend class RegistryBuffer;
};

//Handle to registry object
class ObjectHandle
{
public:
    ObjectHandle() { _object = nullptr; }
    ObjectHandle(Object* object);
    u64 UID() const;
    bool Valid() const;
    explicit operator bool() const; //Used to check if handle is valid
    //Get object property. Adds the property to the object if it doesn't already exist. Use ::Has() to check for the property first to avoid adding it.
    PropertyHandle Property(std::string_view name);
    bool Has(std::string_view propertyName);
    std::vector<PropertyHandle> Properties();
    bool operator==(ObjectHandle other);

    //Lock/unlock the objects internal mutex. This is garbage, but temporary. Registry will be rewritten to handle this internally once the map editor is stable.
    //Want to make sure all the requirements of it are understood before doing that. Likely will need to make other changes to the registry to fit map editor features
    void Lock();
    void Unlock();

    //Shortcuts for property access instead of doing object.Property("propertyName").Get<T>(). Property will be created if it doesn't exist for Get<T>/Set<T>. IsType<T> returns false if the prop doesn't exist.
    template<typename T> T Get(std::string_view propertyName);
    template<typename T> void Set(std::string_view propertyName, T value);
    template<typename T> bool IsType(std::string_view propertyName);
    std::vector<ObjectHandle>& GetObjectList(std::string_view propertyName);
    bool AppendObjectList(std::string_view name, ObjectHandle obj);
    std::vector<string>& GetStringList(std::string_view propertyName);
    bool AppendStringList(std::string_view name, const string& str);
    void SetObjectList(std::string_view propertyName, const std::vector<ObjectHandle>& value);
    void SetStringList(std::string_view propertyName, const std::vector<string>& value);
    bool Remove(std::string_view propertyName); //Remove property. Returns true if it's removed. Returns false if it's not removed or doesn't exist.

private:
    Object* _object;

    friend class Registry;
};

class BufferHandle
{
public:
    std::optional<std::vector<u8>> Load();
    bool Save(std::span<u8> bytes);
    string Name();
    void SetName(const string& name);
    u64 UID() const;
    bool Valid() const;
    size_t Size() const;
    explicit operator bool() const; //Used to check if handle is valid

    BufferHandle(RegistryBuffer* buffer) : _buffer(buffer)
    {

    }

private:
    RegistryBuffer* _buffer;

    friend class Registry;
};

//Value storage for Property
using PropertyValue = std::variant<std::monostate, i32, i16, i8, u64, u32, u16, u8, f32, bool, string, std::vector<ObjectHandle>, std::vector<string>, Vec3, Mat3, ObjectHandle, BufferHandle>;

class RegistryProperty
{
public:
    string Name;
    PropertyValue Value;
};

class Object
{
public:
    u64 UID;
    std::vector<RegistryProperty> Properties;
    RegistryProperty& Property(std::string_view name); //Note: Returned property is for short term use in Registry.cpp only. It isn't stable like the PropertyHandle returned by ObjectHandle::Property()
    bool Has(std::string_view propertyName);
    bool AppendObjectList(std::string_view name, ObjectHandle obj);
    bool AppendStringList(std::string_view name, const string& str);
    //Locked when editing properties. Temporary bandaid. The current design is going to be tested on the map editor first.
    //Afterwards a better solution for thread safety and any other major design flaws found will be fixed.
    std::unique_ptr<std::mutex> Mutex = std::make_unique<std::mutex>();

    Object() : UID(NullUID) { }
    Object(u64 uid, u64 parentUID = NullUID, std::string_view name = "") : UID(uid)
    {
        if (parentUID != NullUID)
            Property("ParentUID").Value = parentUID;

        Property("Name").Value = string(name);
    }
};

template<typename T>
class Prop
{
public:


private:
    Object* _object;
    u32 _index; //Index of property in Object::Properties
};

//Handle to a property of a registry object
class PropertyHandle
{
public:
    PropertyHandle(Object* object, u32 index) : _object(object), _index(index) { }
    bool Valid() const { return _object && _object->UID != NullUID; }
    string Name() { return _object->Properties[_index].Name; }
    //Allows checking handle validity with if statements. E.g. if (handle) { /*valid*/ } else { /*invalid*/ }
    explicit operator bool() const { return Valid(); }
    template<typename T> T Get();
    template<typename T> void Set(T value);
    template<typename T> bool IsType();

    //Special cases of Get<T>() and Set<T>() for lists. For convenience to avoid using Get<std::vector<ObjectHandle>>()
    std::vector<ObjectHandle>& GetObjectList()
    {
        RegistryProperty& prop = _object->Properties[_index];
        if (std::holds_alternative<std::monostate>(prop.Value)) //Set to empty list if no type is set
            prop.Value = std::vector<ObjectHandle>();

        return std::get<std::vector<ObjectHandle>>(prop.Value);
    }

    void SetObjectList(const std::vector<ObjectHandle>& newList = {})
    {
        RegistryProperty& prop = _object->Properties[_index];
        prop.Value = newList; //Replaces existing list
    }

    std::vector<string>& GetStringList()
    {
        RegistryProperty& prop = _object->Properties[_index];
        if (std::holds_alternative<std::monostate>(prop.Value)) //Set to empty list if no type is set
            prop.Value = std::vector<string>();

        return std::get<std::vector<string>>(prop.Value);
    }

    void SetStringList(const std::vector<string>& newList = {})
    {
        RegistryProperty& prop = _object->Properties[_index];
        prop.Value = newList; //Replaces existing list
    }

    u32 Index() { return _index; }

private:
    Object* _object;
    u32 _index; //Index of property in Object::Properties
};


//Binary blob of data managed by the registry. Useful for bulk binary data such as textures and meshes. Serialized as binary, unlike objects.
class RegistryBuffer
{
public:
    u64 UID; //Buffer UIDs are separate from object UIDs
    string Name;
    u64 Size; //Size in bytes
    //Stupid shortcut for thread safety. Will be fixed after first iteration of registry fully tested out in the map editor. No point in making it highly optimized before requirements are understood.
    std::unique_ptr<std::mutex> Mutex = std::make_unique<std::mutex>();

    RegistryBuffer() : UID(NullUID), Size(0) { }
    RegistryBuffer(u64 uid, std::string_view name = "", u64 size = 0) : UID(uid), Name(name), Size(size)
    {

    }
    std::optional<std::vector<u8>> Load();
    bool Save(std::span<u8> bytes);
};

static const ObjectHandle NullObjectHandle = { nullptr };
static const BufferHandle NullBufferHandle = { nullptr };
static const PropertyHandle NullPropertyHandle = { nullptr, std::numeric_limits<u32>::max() };

template<typename T>
T PropertyHandle::Get()
{
    //Comptime check that T is supported
    static_assert(
        std::is_same<T, i32>() ||
        std::is_same<T, i16>() ||
        std::is_same<T, i8>() ||
        std::is_same<T, u64>() ||
        std::is_same<T, u32>() ||
        std::is_same<T, u16>() ||
        std::is_same<T, u8>() ||
        std::is_same<T, f32>() ||
        std::is_same<T, bool>() ||
        std::is_same<T, string>() ||
        std::is_same<T, Vec3>() ||
        std::is_same<T, Mat3>() ||
        std::is_same<T, std::monostate>() ||
        std::is_same<T, ObjectHandle>() ||
        std::is_same<T, BufferHandle>() ||
        std::is_same<T, std::vector<string>>() ||
        std::is_same<T, std::vector<ObjectHandle>>(), "Unsupported type used by RegistryProperty::Get<T>()");

    if (!_object)
        THROW_EXCEPTION("Called ::Get() on and invalid property handle!");

    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    RegistryProperty& prop = _object->Properties[_index];
    return std::get<T>(prop.Value);
}

template<typename T>
void PropertyHandle::Set(T value)
{
    //Comptime check that T is supported
    static_assert(
        std::is_same<T, i32>() ||
        std::is_same<T, i16>() ||
        std::is_same<T, i8>() ||
        std::is_same<T, u64>() ||
        std::is_same<T, u32>() ||
        std::is_same<T, u16>() ||
        std::is_same<T, u8>() ||
        std::is_same<T, f32>() ||
        std::is_same<T, bool>() ||
        std::is_same<T, string>() ||
        std::is_same<T, Vec3>() ||
        std::is_same<T, Mat3>() ||
        std::is_same<T, ObjectHandle>() ||
        std::is_same<T, BufferHandle>() ||
        std::is_same<T, std::monostate>() ||
        std::is_same<T, std::vector<string>>() ||
        std::is_same<T, std::vector<ObjectHandle>>(), "Unsupported type used by RegistryProperty::Set<T>()");

    if (!_object)
        THROW_EXCEPTION("Called ::Set() on and invalid property handle!");

    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    RegistryProperty& prop = _object->Properties[_index];
    prop.Value = value;
}

template<typename T>
bool PropertyHandle::IsType()
{
    if (!_object)
        THROW_EXCEPTION("Called ::IsType<T>() on and invalid property handle!");

    std::lock_guard<std::mutex> lock(*_object->Mutex.get());
    RegistryProperty& prop = _object->Properties[_index];
    return std::holds_alternative<T>(prop.Value);
}

template<typename T>
T ObjectHandle::Get(std::string_view propertyName)
{
    return Property(propertyName).Get<T>();
}

template<typename T>
void ObjectHandle::Set(std::string_view propertyName, T value)
{
    Property(propertyName).Set<T>(value);
}

template<typename T>
bool ObjectHandle::IsType(std::string_view propertyName)
{
    return Has(propertyName) && Property(propertyName).IsType<T>();
}
