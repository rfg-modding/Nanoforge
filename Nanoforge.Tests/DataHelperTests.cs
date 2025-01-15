using Nanoforge.Misc;
using Nanoforge.Rfg;

namespace Nanoforge.Tests;

///Tests for <see cref="Nanoforge.Misc.DataHelper"/>
public class DataHelperTests
{
    [Fact]
    public void DeepCopyToggleableBytes()
    {
        Toggleable<byte[]?> obj = new([1, 2, 3]);
        Toggleable<byte[]?> objCopy = DataHelper.DeepCopyToggleableBytes(obj);
        Assert.False(ReferenceEquals(obj, objCopy));
        Assert.False(ReferenceEquals(obj.Value, objCopy.Value));
        Assert.NotNull(objCopy.Value);
        
        Toggleable<byte[]?> objWithNullArray = new(null);
        Toggleable<byte[]?> objWithNullArrayCopy = DataHelper.DeepCopyToggleableBytes(objWithNullArray);
        Assert.False(ReferenceEquals(objWithNullArray, objWithNullArrayCopy));
        Assert.Null(objWithNullArray.Value);
        Assert.Null(objWithNullArrayCopy.Value);
    }
    
    [Fact]
    public void EnumToRfgName()
    {
        //Test a few of the zone object enums that have RfgNameAttribute
        Assert.True(DataHelper.ToRfgName(ObjectBoundingBox.BoundingBoxType.GpsTarget, out string boundBoxTypeRfgName) && boundBoxTypeRfgName == "GPS Target");
        Assert.True(DataHelper.ToRfgName(ObjectDummy.DummyTypeEnum.Rally, out string dummyTypeRfgName) && dummyTypeRfgName == "Rally");
        Assert.True(DataHelper.ToRfgName(TriggerRegion.RegionTypeEnum.KillHuman, out string regionTypeRfgName) && regionTypeRfgName == "kill human");
        Assert.True(DataHelper.ToRfgName(ObjLight.LightTypeEnum.Spot, out string lightTypeRfgName) && lightTypeRfgName == "spot");
        Assert.True(DataHelper.ToRfgName(MultiMarker.MultiMarkerType.KingOfTheHillTarget, out string markerTypeRfgName) && markerTypeRfgName == "King of the Hill target");
        
        //This enum doesn't have RfgNameAttribute so it should return false
        Assert.False(DataHelper.ToRfgName<RfgMover.MoveTypeEnum>(RfgMover.MoveTypeEnum.Normal, out string moveTypeRfgName));
    }

    [Fact]
    public void RfgNameToEnum()
    {
        Assert.True(DataHelper.FromRfgName("GPS Target", out ObjectBoundingBox.BoundingBoxType boundingBoxType) && boundingBoxType == ObjectBoundingBox.BoundingBoxType.GpsTarget);
        Assert.True(DataHelper.FromRfgName("None", out ObjectBoundingBox.BoundingBoxType boundingBoxType2) && boundingBoxType2 == ObjectBoundingBox.BoundingBoxType.None);
        //This one is important to test since the rfg name has a typo in it. We need to preserve that typo during map export for the game to properly read the field.
        // ReSharper disable once StringLiteralTypo
        Assert.True(DataHelper.FromRfgName("Tech Reponse Pos", out ObjectDummy.DummyTypeEnum dummyType) && dummyType == ObjectDummy.DummyTypeEnum.TechResponsePos);
        Assert.True(DataHelper.FromRfgName("kill human", out TriggerRegion.RegionTypeEnum regionType) && regionType == TriggerRegion.RegionTypeEnum.KillHuman);
        Assert.True(DataHelper.FromRfgName("spot", out ObjLight.LightTypeEnum lightType) && lightType == ObjLight.LightTypeEnum.Spot);
        Assert.True(DataHelper.FromRfgName("King of the Hill target", out MultiMarker.MultiMarkerType markerType) && markerType == MultiMarker.MultiMarkerType.KingOfTheHillTarget);
        
        //Fake/undefined rfg names. Should fail
        Assert.False(DataHelper.FromRfgName("Directional", out ObjLight.LightTypeEnum badLightType));
        Assert.False(DataHelper.FromRfgName("Tech Response Pos", out ObjectDummy.DummyTypeEnum badDummyType)); //Another check to make sure the typo isn't fixed
        Assert.False(DataHelper.FromRfgName("Fake dummy type name", out ObjectDummy.DummyTypeEnum badDummyType2)); //Another check to make sure the typo isn't fixed
        Assert.False(DataHelper.FromRfgName("Marker type", out MultiMarker.MultiMarkerType badMarkerType));
    }

    [Fact]
    public void EnumToRfgFlagsString()
    {
        ObjectMover.BuildingTypeFlagsEnum buildingTypeFlags = ObjectMover.BuildingTypeFlagsEnum.Dynamic | ObjectMover.BuildingTypeFlagsEnum.ForceField | ObjectMover.BuildingTypeFlagsEnum.House;
        ObjectMover.BuildingTypeFlagsEnum buildingTypeFlags2 = ObjectMover.BuildingTypeFlagsEnum.None;
        ObjectMover.ChunkFlagsEnum chunkFlags = ObjectMover.ChunkFlagsEnum.Building | ObjectMover.ChunkFlagsEnum.Regenerate;
        ObjectMover.ChunkFlagsEnum chunkFlags2 = ObjectMover.ChunkFlagsEnum.PlumeOnDeath | ObjectMover.ChunkFlagsEnum.InheritDamagePercentage;
        ObjLight.ObjLightFlags objLightFlags = ObjLight.ObjLightFlags.UseClipping | ObjLight.ObjLightFlags.NightTime | ObjLight.ObjLightFlags.DayTime;
        ObjLight.ObjLightFlags objLightFlags2 = ObjLight.ObjLightFlags.UseClipping;
        ObjLight.ObjLightFlags objLightFlags3 = ObjLight.ObjLightFlags.None;
        
        Assert.True(DataHelper.ToRfgFlagsString(buildingTypeFlags, out string buildingTypeFlagsString) && buildingTypeFlagsString == "Dynamic Force_Field House");
        Assert.True(DataHelper.ToRfgFlagsString(buildingTypeFlags2, out string buildingTypeFlagsString2) && buildingTypeFlagsString2 == "");
        Assert.True(DataHelper.ToRfgFlagsString(chunkFlags, out string chunkFlagsString) && chunkFlagsString == "building regenerate");
        Assert.True(DataHelper.ToRfgFlagsString(chunkFlags2, out string chunkFlagsString2) && chunkFlagsString2 == "plume_on_death inherit_damage_pct");
        Assert.True(DataHelper.ToRfgFlagsString(objLightFlags, out string objLightFlagsString) && objLightFlagsString == "use_clipping daytime nighttime");
        Assert.True(DataHelper.ToRfgFlagsString(objLightFlags2, out string objLightFlagsString2) && objLightFlagsString2 == "use_clipping");
        Assert.True(DataHelper.ToRfgFlagsString(objLightFlags3, out string objLightFlagsString3) && objLightFlagsString3 == "");
    }

    [Fact]
    public void RfgFlagsStringToEnum()
    {
        string buildingTypeFlagsString = "Dynamic Force_Field House";
        string buildingTypeFlagsString2 = "";
        string chunkFlagsString = "building regenerate";
        string chunkFlagsString2 = "plume_on_death inherit_damage_pct";
        string objLightFlagsString = "use_clipping daytime nighttime";
        string objLightFlagsString2 = "use_clipping";
        string objLightFlagsString3 = "";
        
        Assert.True(DataHelper.FromRfgFlagsString(buildingTypeFlagsString, out ObjectMover.BuildingTypeFlagsEnum buildingTypeFlags) && buildingTypeFlags == (ObjectMover.BuildingTypeFlagsEnum.Dynamic | ObjectMover.BuildingTypeFlagsEnum.ForceField | ObjectMover.BuildingTypeFlagsEnum.House));
        Assert.True(DataHelper.FromRfgFlagsString(buildingTypeFlagsString2, out ObjectMover.BuildingTypeFlagsEnum buildingTypeFlags2) && buildingTypeFlags2 == ObjectMover.BuildingTypeFlagsEnum.None);
        Assert.True(DataHelper.FromRfgFlagsString(chunkFlagsString, out ObjectMover.ChunkFlagsEnum chunkFlags) && chunkFlags == (ObjectMover.ChunkFlagsEnum.Building | ObjectMover.ChunkFlagsEnum.Regenerate));
        Assert.True(DataHelper.FromRfgFlagsString(chunkFlagsString2, out ObjectMover.ChunkFlagsEnum chunkFlags2) && chunkFlags2 == (ObjectMover.ChunkFlagsEnum.PlumeOnDeath | ObjectMover.ChunkFlagsEnum.InheritDamagePercentage));
        Assert.True(DataHelper.FromRfgFlagsString(objLightFlagsString, out ObjLight.ObjLightFlags lightFlags) && lightFlags == (ObjLight.ObjLightFlags.UseClipping | ObjLight.ObjLightFlags.NightTime | ObjLight.ObjLightFlags.DayTime));
        Assert.True(DataHelper.FromRfgFlagsString(objLightFlagsString2, out ObjLight.ObjLightFlags lightFlags2) && lightFlags2 == ObjLight.ObjLightFlags.UseClipping);
        Assert.True(DataHelper.FromRfgFlagsString(objLightFlagsString3, out ObjLight.ObjLightFlags lightFlags3) && lightFlags3 == ObjLight.ObjLightFlags.None);
    }
}