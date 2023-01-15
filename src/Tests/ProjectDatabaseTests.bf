using Nanoforge.App.Project;
using Nanoforge.Math;
using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Tests
{
    public static class ProjectDatabaseTests
    {
        public class BasicEditorObject : EditorObject
        {
            [EditorProperty]
            public i32 Int0;

            [EditorProperty]
            public bool Boolean0;

            public f64 UntrackedDouble0;

            [EditorProperty]
            public f32 Float0;

            [EditorProperty]
            public Vec3<f32> Vec3F0 = .Zero;

            [EditorProperty]
            public bool Boolean1;
        }

        [Test]
        public static void BasicSnapshotTest()
        {
            var obj = ProjectDB.CreateObject<BasicEditorObject>();
            obj.Int0 = 100;
            obj.Boolean1 = true;

            using (TrackChanges(obj, "Basic change tracking test"))
            {
                obj.Int0 = 200;
                var debug0 = 0;
            }

            using (var diffUtil = TrackChanges(obj, "Change tracking with DiffUtil<T> accessible"))
            {
                obj.Int0 = 200;
                var debug0 = 0;
            }
        }
    }
}