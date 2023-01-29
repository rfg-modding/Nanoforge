using Common;
using System;
using System.Collections;
using System.Threading;
using Common.Math;

namespace Nanoforge.App
{
    [AttributeUsage(.Field | .Property, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .AllMembers)]
    public struct EditorPropertyAttribute : Attribute
    {

    }

    public class EditorObject
    {
        public bool HasName => ProjectDB.GetObjectName(this) case .Ok;
        public Result<String> GetName() => ProjectDB.GetObjectName(this);
        public void SetName(StringView name) => ProjectDB.SetObjectName(this, name);
    }
}