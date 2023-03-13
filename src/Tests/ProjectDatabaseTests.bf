using Nanoforge.App.Project;
using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
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
            public Vec3 Vec3F0 = .Zero;

            [EditorProperty]
            public bool Boolean1;
        }

        [Test]
        public static void BasicSnapshotTest()
        {
            var obj = ProjectDB.CreateObject<BasicEditorObject>();
            obj.Int0 = 100;
            obj.Boolean1 = true;

            var obj2 = ProjectDB.CreateObject<BasicEditorObject>();
            obj2.Int0 = 123;
            obj2.Boolean1 = false;

            //Example of how to manually use the API. Using statements automatically handle calling .Commit()
            {
                DiffUtil changes = BeginCommit!("Basic change tracking with manual commit");
                changes.Track(obj);
                obj.Int0 = 3521;
                obj.Boolean0 = true;
                changes.Commit();
            }

            using (BeginCommit!("Basic change tracking test", obj)) //Only tracking one object so it handles calling TrackChanges for us
            {
                obj.Int0 = 200;
            }

            using (var changes = BeginCommit!("Change tracking with DiffUtil<T> accessible", obj))
            {
                obj.Int0 = 400;
            }

            Vec3 obj2Vec3F0_initialState = obj2.Vec3F0;
            using (var changes = BeginCommit!("Change tracking for multiple objects"))
            {
                changes.Track(obj);
                changes.Track(obj2);
                obj.Int0 = -1573;
                obj2.Boolean1 = true;
                obj2.Vec3F0 = .(1023.0f, -534.0f, 562.0f);
            }

            //TODO: Put this in a separate function & add asserts to confirm undo/redo worked correctly
            //TODO: Maybe make helpers to make undo/redo validation for a ton of properties simpler
            Vec3 obj2Vec3F0_finalState = obj2.Vec3F0;
            ProjectDB.Undo();
            Vec3 obj2Vec3F0_postUndoState = obj2.Vec3F0;
            ProjectDB.Redo();
            Vec3 obj2Vec3F0_postRedoState = obj2.Vec3F0;
            var debug3 = 0;
        }

        [Test]
        public static void ComplexObjectTest()
        {

        }
    }
}