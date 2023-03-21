using Common;
using System;

namespace Nanoforge
{
	public static class Workspace
	{
        [OnCompile(.TypeInit), Comptime]
        private static void EmitWorkspaceFields()
        {
            Compiler.EmitTypeBody(typeof(Workspace), scope $"public static StringView Directory = @\"{System.IO.Directory.GetCurrentDirectory(.. scope .())}\\\";\n");
            Compiler.EmitTypeBody(typeof(Workspace), scope $"public static StringView Name = \"{Compiler.CallerProject}\";");
        }
	}
}