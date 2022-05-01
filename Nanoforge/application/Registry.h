#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include <RfgTools++/types/Vec3.h>
#include <RfgTools++/types/Mat3.h>
#include <unordered_map>
#include "util/Hash.h"
#include <stdexcept>
#include <optional>
#include <vector>
#include <variant>
#include <limits>
#include "Log.h"
#include <span>

class Property;
class Object;
class PropertyHandle;
class ObjectHandle;

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
    u64 NumObjects() const;
    bool ObjectExists(u64 uid) const;
    //Load registry from folder
    bool Load(const string& inFolderPath);
    //Save registry to folder
    bool Save(const string& outFolderPath);
private:
    ObjectHandle LoadEntry(const std::vector<std::string_view>& lines, size_t entryLineStart); //Load object from lines of serialized registry. Returns null handle if it fails.
    ObjectHandle CreateObjectInternal(u64 uid, u64 parentUID = NullUID, std::string_view objectName = "", std::string_view typeName = ""); //For internal use by Registry::Load() only
    //Registry objects mapped to their UIDs
    //Note: If the container type is changed ObjectHandle and PropertyHandle likely won't be able to hold pointers anymore.
    //      They rely on the fact that pointers to std::unordered_map values are stable.
    std::unordered_map<u64, Object> _objects = {};
    u64 _nextUID = 0;
    std::mutex _objectCreationLock;

    Object& GetObjectByUID(u64 uid);

    friend class ObjectHandle;
    friend class PropertyHandle;
};

//Handle to registry object
class ObjectHandle
{
public:
    ObjectHandle(Object* object);
    u64 UID() const;
    bool Valid() const;
    explicit operator bool() const; //Used to check if handle is valid
    std::vector<ObjectHandle>& SubObjects();
    //Get object property. Adds the property to the object if it doesn't already exist. Use ::Has() to check for the property first to avoid adding it.
    PropertyHandle Property(std::string_view name);
    bool Has(std::string_view propertyName);
    std::vector<PropertyHandle> Properties();
    bool operator==(ObjectHandle other);

private:
    Object* _object;

    friend class Registry;
};

//Value storage for Property
using PropertyValue = std::variant<i32, u64, u32, u16, u8, f32, bool, string, std::vector<ObjectHandle>, Vec3, Mat3, ObjectHandle>;

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
    std::vector<ObjectHandle> SubObjects;
    RegistryProperty& Property(std::string_view name); //Note: Returned property is for short term use in Registry.cpp only. It isn't stable like the PropertyHandle returned by ObjectHandle::Property()
    bool Has(std::string_view propertyName);
    //Locked when editing properties. Temporary bandaid. The current design is going to be tested on the map editor first.
    //Afterwards a better solution for thread safety and any other major design flaws found will be fixed.
    std::unique_ptr<std::mutex> Mutex = std::make_unique<std::mutex>();

    Object() : UID(NullUID) { }
    Object(u64 uid, u64 parentUID = NullUID, std::string_view name = "") : UID(uid)
    {
        if (parentUID != NullUID)
            Property("ParentUID").Value = parentUID;
        if (name != "")
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

    //Special cases of Get<T>() and Set<T>() for object lists. For convenience to avoid using Get<std::vector<ObjectHandle>>()
    std::vector<ObjectHandle>& GetObjectList()
    {
        RegistryProperty& prop = _object->Properties[_index];
        return std::get<std::vector<ObjectHandle>>(prop.Value);
    }

    void SetObjectList(const std::vector<ObjectHandle>& newList = {})
    {
        RegistryProperty& prop = _object->Properties[_index];
        prop.Value = newList; //Replaces existing list
    }

private:
    Object* _object;
    u32 _index; //Index of property in Object::Properties
};

static const ObjectHandle NullObjectHandle = { nullptr };
static const PropertyHandle NullPropertyHandle = { nullptr, std::numeric_limits<u32>::max() };

template<typename T>
T PropertyHandle::Get()
{
    //Comptime check that T is supported
    static_assert(
        std::is_same<T, i32>() ||
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
