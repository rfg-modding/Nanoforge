using Nanoforge;
using System;
using System.Collections;
using System.Threading;
using Nanoforge.Math;

namespace Nanoforge.App
{
    [AttributeUsage(.Field | .Property, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .AllMembers)]
    public struct EditorPropertyAttribute : Attribute
    {

    }

    public class EditorObject
    {
        public String Name
        {
            get
            {
                return ProjectDB.GetObjectName(this);
            }
            set
            {
                ProjectDB.SetObjectName(this, value);
            }
        }
    }
}