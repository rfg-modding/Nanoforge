using System.Reflection;
using AutoFixture;
using AutoFixture.Kernel;
using FluentAssertions;
using Nanoforge.Editor;
using Nanoforge.Render.Resources;
using Nanoforge.Rfg;

namespace Nanoforge.Tests;

public class NanoDBTests
{
    private static List<Type> _editorObjectTypes => AppDomain.CurrentDomain.GetAssemblies()
        .SelectMany(assembly => assembly.GetTypes())
        .Where(type => typeof(EditorObject).IsAssignableFrom(type)).ToList();

    public static IEnumerable<object[]> TestEditorObjects;

    static NanoDBTests()
    {
        var typeList = _editorObjectTypes;
        Fixture fixture = new();
        //Make it so the fixture doesn't complain about circular references on objects. All ZoneObjects can have this issue due to the Parent and Children fields
        fixture.Behaviors.OfType<ThrowingRecursionBehavior>().ToList().ForEach(b => fixture.Behaviors.Remove(b));
        fixture.Behaviors.Add(new OmitOnRecursionBehavior());
        //Don't set RenderObject fields. AutoFixture throws exceptions on some unsafe types. For the moment its fine to exclude that type from the tests.
        fixture.Customizations.Add(new PropertyTypeOmitter(typeof(RenderObject)));
        
        TestEditorObjects = _editorObjectTypes.Select(type => new object[] { new SpecimenContext(fixture).Resolve(type) });
        var list = TestEditorObjects.ToList();
    }

    //Runs once per class that is derived from EditorObject. Tests serialization and deserialization of a single object.
    [Theory, MemberData(nameof(TestEditorObjects))]
    public void AllEditorObjectSaveLoad(EditorObject originalObject)
    {
        string testObjectName = "TestObject";
        originalObject.Name = testObjectName;

        //TODO: Change to write to temporary directory in project dir that gets git ignored
        string projectDirectory = "./AllEditorObjectSaveLoadTest/";
        string projectFilePath = "";
        NanoDB.NewProject(projectDirectory, "AllEditorObjectSaveLoadTest", "UnitTestRunner", "");
        projectFilePath = NanoDB.CurrentProject.FilePath;

        NanoDB.AddObject(originalObject);
        NanoDB.Save();
        NanoDB.Load(projectFilePath);

        EditorObject? deserializedObject = NanoDB.Find(testObjectName);
        Assert.NotNull(deserializedObject);
        deserializedObject.Should().BeEquivalentTo(originalObject);
    }

    //Tests serialization & deserialization on several ZoneObjects with parent-child relationships set.
    [Fact]
    public void ObjectSaveLoadWithHierarchy()
    {
        string projectDirectory = "./ObjectSaveLoadWithHierarchyTest/";
        string projectFilePath = "";
        NanoDB.NewProject(projectDirectory, "ObjectSaveLoadWithHierarchyTest", "UnitTestRunner", "");
        projectFilePath = NanoDB.CurrentProject.FilePath;

        Fixture fixture = new();
        fixture.Behaviors.OfType<ThrowingRecursionBehavior>().ToList().ForEach(b => fixture.Behaviors.Remove(b));
        fixture.Behaviors.Add(new OmitOnRecursionBehavior());
        fixture.Customizations.Add(new PropertyTypeOmitter(typeof(RenderObject)));

        //Note: The object types were picked at random to have some variety. This is not representative of a typical RFG map.
        var objectA = fixture.Create<RfgMover>();
        objectA.Name = "objectA";
        objectA.Parent = null;
        objectA.Children.Clear();
        var objectB = fixture.Create<PlayerStart>();
        objectB.Name = "objectB";
        objectB.Parent = null;
        objectB.Children.Clear();
        var objectC = fixture.Create<PlayerStart>();
        objectC.Name = "objectC";
        objectC.Parent = null;
        objectC.Children.Clear();
        var objectD = fixture.Create<Weapon>();
        objectD.Name = "objectD";
        objectD.Parent = null;
        objectD.Children.Clear();
        
        objectA.AddChild(objectB);
        objectA.AddChild(objectC);
        objectC.AddChild(objectD);
        
        NanoDB.AddObject(objectA);
        NanoDB.AddObject(objectB);
        NanoDB.AddObject(objectC);
        NanoDB.AddObject(objectD);
        
        NanoDB.Save();
        NanoDB.Load(projectFilePath);

        ZoneObject? objectA2 = (ZoneObject?)NanoDB.Find(objectA.Name);
        ZoneObject? objectB2 = (ZoneObject?)NanoDB.Find(objectB.Name);
        ZoneObject? objectC2 = (ZoneObject?)NanoDB.Find(objectC.Name);
        ZoneObject? objectD2 = (ZoneObject?)NanoDB.Find(objectD.Name);
        
        Assert.NotNull(objectA2);
        Assert.NotNull(objectB2);
        Assert.NotNull(objectC2);
        Assert.NotNull(objectD2);
        
        objectA2.Should().BeEquivalentTo(objectA, options => options.IgnoringCyclicReferences());
        objectB2.Should().BeEquivalentTo(objectB, options => options.IgnoringCyclicReferences());
        objectC2.Should().BeEquivalentTo(objectC, options => options.IgnoringCyclicReferences());
        objectD2.Should().BeEquivalentTo(objectD, options => options.IgnoringCyclicReferences());
        
        //TODO: Figure out a better way of handling this
        //Manually check parent/child fields since I couldn't figure out how to do it with FluentAssertions
        Assert.True(ReferenceEquals(objectA2, objectB2.Parent));
        Assert.True(ReferenceEquals(objectA2, objectC2.Parent));
        Assert.True(objectA2.Children.Count == 2);
        Assert.Contains(objectB2, objectA2.Children);
        Assert.Contains(objectC2, objectA2.Children);
        
        Assert.True(objectB2.Children.Count == 0);
        
        Assert.True(ReferenceEquals(objectC2, objectD2.Parent));
        Assert.True(objectC2.Children.Count == 1);
        Assert.Contains(objectD2, objectC2.Children);
        
        Assert.True(objectD2.Children.Count == 0);
    }

    //Tests save/load for global objects. These are just like normal editor objects, except they aren't tied to a project. Used for persistent data like editor settings.
    [Fact]
    public void GlobalObjectSaveLoad()
    {
        Fixture fixture = new();
        fixture.Customizations.Add(new PropertyTypeOmitter(typeof(RenderObject)));
        GeneralSettings objA = fixture.Create<GeneralSettings>();
        objA.Name = "objA-Global";
        
        NanoDB.AddGlobalObject(objA);
        
        NanoDB.SaveGlobals();
        NanoDB.LoadGlobals();
        
        GeneralSettings? objA2 = (GeneralSettings?)NanoDB.Find(objA.Name);
        
        Assert.NotNull(objA2);
        objA.Should().BeEquivalentTo(objA2, options => options.IgnoringCyclicReferences());

        //Cleanup globals so there's a clean state for any tests that follow
        NanoDB.ClearGlobals();
        NanoDB.SaveGlobals();
    }
    
    //Test basic buffer save/load
    [Fact]
    public void BufferSaveLoad()
    {
        string projectDirectory = "./BufferSaveLoadTest/";
        NanoDB.NewProject(projectDirectory, "BufferSaveLoadTest", "UnitTestRunner", "");
        
        Fixture fixture = new();
        byte[] bufferBytes = fixture.CreateMany<byte>(1024).ToArray();
        Assert.NotNull(bufferBytes);

        ProjectBuffer? buffer = NanoDB.CreateBuffer(bufferBytes, "TestProjectBuffer");
        Assert.NotNull(buffer);
        
        byte[] bufferBytesLoaded = buffer.Load();
        Assert.NotNull(bufferBytesLoaded);
        Assert.Equal(bufferBytes, bufferBytesLoaded);
    }

    //Test that buffer metadata is preserved across project save/load
    [Fact]
    public void BufferProjectSaveLoad()
    {
        string projectDirectory = "./BufferProjectSaveLoadTest/";
        string projectFilePath = "";
        NanoDB.NewProject(projectDirectory, "BufferProjectSaveLoadTest", "UnitTestRunner", "");
        projectFilePath = NanoDB.CurrentProject.FilePath;
        
        Fixture fixture = new();
        byte[] bufferBytes = fixture.CreateMany<byte>(1024).ToArray();
        Assert.NotNull(bufferBytes);
        
        ProjectBuffer? buffer = NanoDB.CreateBuffer(bufferBytes, "TestProjectBuffer");
        Assert.NotNull(buffer);

        NanoDB.Save();
        NanoDB.Load(projectFilePath);

        ProjectBuffer? bufferLoaded = NanoDB.Find<ProjectBuffer>(buffer.Name);
        Assert.NotNull(bufferLoaded);
        
        //Load the buffer from the drive again and make sure it still works
        byte[] bufferBytesLoaded = bufferLoaded.Load();
        Assert.NotNull(bufferBytesLoaded);
        Assert.Equal(bufferBytes, bufferBytesLoaded);
    }
    
    //Test writing a smaller byte array to a buffer than it originally had
    [Fact]
    public void BufferShrink()
    {
        string projectDirectory = "./BufferShrinkTest/";
        NanoDB.NewProject(projectDirectory, "BufferShrinkTest", "UnitTestRunner", "");
        
        Fixture fixture = new();
        byte[] bufferBytes = fixture.CreateMany<byte>(1024).ToArray();
        Assert.NotNull(bufferBytes);

        ProjectBuffer? buffer = NanoDB.CreateBuffer(bufferBytes, "TestProjectBuffer");
        Assert.NotNull(buffer);
        
        byte[] bufferBytes2 = fixture.CreateMany<byte>(10).ToArray();
        Assert.NotNull(bufferBytes2);
        Assert.True(buffer.Save(bufferBytes2));
        
        byte[] bufferBytes2Loaded = buffer.Load();
        Assert.NotNull(bufferBytes2Loaded);
        Assert.Equal(bufferBytes2, bufferBytes2Loaded);
        Assert.NotEqual(bufferBytes, bufferBytes2Loaded);
    }
    
    //Test writing a larger byte array to a buffer than it originally had
    [Fact]
    public void BufferGrow()
    {
        string projectDirectory = "./BufferGrowTest/";
        NanoDB.NewProject(projectDirectory, "BufferGrowTest", "UnitTestRunner", "");
        
        Fixture fixture = new();
        byte[] bufferBytes = fixture.CreateMany<byte>(1024).ToArray();
        Assert.NotNull(bufferBytes);

        ProjectBuffer? buffer = NanoDB.CreateBuffer(bufferBytes, "TestProjectBuffer");
        Assert.NotNull(buffer);
        
        byte[] bufferBytes2 = fixture.CreateMany<byte>(2048).ToArray();
        Assert.NotNull(bufferBytes2);
        Assert.True(buffer.Save(bufferBytes2));
        
        byte[] bufferBytes2Loaded = buffer.Load();
        Assert.NotNull(bufferBytes2Loaded);
        Assert.Equal(bufferBytes2, bufferBytes2Loaded);
        Assert.NotEqual(bufferBytes, bufferBytes2Loaded);
    }
    
    [Fact]
    public void ReferencedBufferSaveLoad()
    {
        string projectDirectory = "./ReferencedBufferSaveLoad/";
        string projectFilePath = "";
        NanoDB.NewProject(projectDirectory, "ReferencedBufferSaveLoad", "UnitTestRunner", "");
        projectFilePath = NanoDB.CurrentProject.FilePath;
        
        Fixture fixture = new();
        byte[] bufferBytes = fixture.CreateMany<byte>(1024).ToArray();
        ProjectBuffer? buffer = NanoDB.CreateBuffer(bufferBytes, "ReferencedProjectBuffer");
        Assert.NotNull(buffer);

        ProjectTexture referencingObject = new(buffer);
        referencingObject.Name = "ObjectThatReferencesBuffer";
        NanoDB.AddObject(referencingObject);

        NanoDB.Save();
        NanoDB.Load(projectFilePath);
        
        ProjectTexture? referencingObjectLoaded = NanoDB.Find<ProjectTexture>(referencingObject.Name);
        Assert.NotNull(referencingObjectLoaded);

        ProjectBuffer? bufferLoadedFromReference = referencingObjectLoaded.Data;
        Assert.NotNull(bufferLoadedFromReference);

        ProjectBuffer? bufferLoadedFromSearch = NanoDB.Find<ProjectBuffer>(buffer.Name);
        Assert.NotNull(bufferLoadedFromSearch);
        //The object should be referencing the same ProjectBuffer instance that NanoDB tracks
        Assert.True(ReferenceEquals(bufferLoadedFromReference, bufferLoadedFromSearch));
        //The original buffer and the instance created while loading the project should have the same name and size
        Assert.Equivalent(buffer, bufferLoadedFromSearch);
    }
}