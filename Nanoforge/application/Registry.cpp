#include "Registry.h"
#include <ranges>
#include <string>

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
        handle.GetOrCreateProperty("Type").Set(string(typeName));

    return handle;
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
