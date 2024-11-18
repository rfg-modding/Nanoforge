namespace Nanoforge.FileSystem;

//Lets you interact with RFG packfiles (.vpp_pc & .str2_pc version 3) like they're a normal filesystem. Tracks all the vpps in the mounted data folder.
//NOT thread safe. You better make some improvements to this before you try to use it on multiple threads simultaneously.
public static class PackfileVFS
{
    //TODO: PORT THIS CLASS - First need to integrate the SyncFaction packfile code
    
    public static void MountDataFolderAsync(string data, string dataPath)
    {
        //TODO: IMPLEMENT
    }
}