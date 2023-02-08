using Common;
using System;
using static System.Compiler;

namespace Nanoforge.Misc.Generators
{
    public class WorkspaceGenerator : Generator
    {
        public override System.String Name => "New workspace info";

        public override void InitUI()
        {
        	AddEdit("Name", "Class name", "");
        }

        public override void Generate(String outFileName, String outText, ref Flags generateFlags)
        {
            generateFlags |= .AllowRegenerate; //This class should always regenerate so it has up to date workspace paths

        	//Output file
        	var name = mParams["Name"];
        	if (name.EndsWith(".bf", .OrdinalIgnoreCase))
        		name.RemoveFromEnd(3);
        	outFileName.Append(name);

        	//Set file contents
        	outText.AppendF(
        		$"""
using System;

namespace {Namespace}
{{
    public static class {name}
    {{
        public static StringView Directory = @"{this.WorkspaceDir}";
        public static StringView Name = @"{this.WorkspaceName}";
    }}
}}
""");
        }
    }
}
