namespace Nanoforge.Misc;

public static class DataHelper
{
    public static Toggleable<byte[]?> DeepCopyToggleableBytes(Toggleable<byte[]?> input)
    {
        if (input.Value == null)
        {
            return new Toggleable<byte[]?>(null);            
        }
        
        byte[] bytesCopy = new byte[input.Value.Length];
        input.Value.CopyTo(bytesCopy, 0);
        return new Toggleable<byte[]?>(bytesCopy);
    }
}