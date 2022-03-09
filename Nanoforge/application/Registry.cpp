#include "Registry.h"
#include <ranges>
#include <string>
#include "Log.h"

Registry& Registry::Get()
{
    static Registry gRegistry;
    return gRegistry;
}

ObjectHandle Registry::CreateObject(std::string_view objectName, std::string_view typeName)
{
    _objectCreationLock.lock();
    const u64 uid = _nextUID++;
    _objects[uid] = Object(string(objectName), uid);
    _objectCreationLock.unlock();
    ObjectHandle handle = { &_objects[uid] };
    if (typeName != "")
        handle.Property("Type").Set(string(typeName));

    return handle;
}

ObjectHandle Registry::GetObjectHandleByUID(u64 uid)
{
    if (!_objects.contains(uid))
        THROW_EXCEPTION("Failed to find object with uid '{}'", uid);

    return { &_objects[uid] };
}

bool Registry::ObjectExists(u64 uid) const
{
    auto find = _objects.find(uid);
    return find != _objects.end();
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

std::vector<ObjectHandle>& ObjectHandle::SubObjects()
{
    if (_object)
        return _object->SubObjects;
    else
        THROW_EXCEPTION("Attempted to get SubObjects from null object handle.")
}

PropertyHandle ObjectHandle::Property(std::string_view name)
{
    if (!_object)
        THROW_EXCEPTION("Attempted to get property from a null object.");

    //Search for property
    for (size_t i = 0; i < _object->Properties.size(); i++)
        if (_object->Properties[i].Name == name)
            return { _object, (u32)i }; //Property found

    //Property not found. Add it to the object and return its handle
    RegistryProperty& prop = _object->Properties.emplace_back();
    prop.Name = string(name);
    return { _object, (u32)(_object->Properties.size() - 1) };
}

bool ObjectHandle::Has(std::string_view propertyName)
{
    if (_object)
        for (size_t i = 0; i < _object->Properties.size(); i++)
            if (_object->Properties[i].Name == propertyName)
                return true;

    return false;
}

std::vector<PropertyHandle> ObjectHandle::Properties()
{
    std::vector<PropertyHandle> properties = {};
    for (u32 i = 0; i < _object->Properties.size(); i++)
        properties.emplace_back(_object, i);

    return properties;
}

bool ObjectHandle::operator==(ObjectHandle other)
{
    return _object == other._object;
}