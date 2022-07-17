using Nanoforge;
using System;
using static System.Compiler;

namespace Nanoforge.Generators
{
	public class SystemGenerator : Generator
	{
		public override System.String Name => "System";

		public override void InitUI()
		{
			AddEdit("Name", "Class name", "");
		}

		public override void Generate(String outFileName, String outText, ref Flags generateFlags)
		{
			//Output file
			var name = mParams["Name"];
			if (name.EndsWith(".bf", .OrdinalIgnoreCase))
				name.RemoveFromEnd(3);
			outFileName.Append(name);

			//Set file contents
			outText.AppendF(
				$"""
				using Nanoforge.App;
				using Nanoforge;
				using System;

				namespace {Namespace}
				{{
					[System]
					class {name} : ISystem
					{{
						static void ISystem.Build(App app)
						{{

						}}

						[SystemInit]
						void Init(App app)
						{{

						}}

						[SystemStage]
						void Update(App app)
						{{

						}}
					}}
				}}
				""");
		}
	}
}