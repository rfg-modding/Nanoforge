using Common;
using System;
using System.Collections;
using System.Threading;
using Common.Math;
using Nanoforge.Misc;
using Nanoforge.Rfg;
using Bon;

namespace Nanoforge.App
{
    [AttributeUsage(.Field | .Property, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .AllMembers)]
    public struct EditorPropertyAttribute : Attribute
    {

    }

    [ReflectAll]
    public class EditorObject
    {
        public const u64 NullUID = u64.MaxValue;

        //Unique identifier for editor objects. This is private instead of public readonly to avoid needing constructors on all editor object types that set UID.
        //UID is guaranteed to be set if an editor object is created the correct way. Either through DiffUtil or ProjectDB.
        [BonInclude]
        private u64 _uid = NullUID;
        public u64 UID => _uid;

        public String Name = new .() ~delete _;
    }
}