using Nanoforge;
using System;

namespace Nanoforge.Generators
{
	public class NanoforgeClassGenerator : Compiler.Generator
	{
		public override String Name => "Nanoforge Class";

		public override void InitUI()
		{
			AddEdit("Name", "Class name", "");
			AddCheckbox("AddConstructor", "Add constructor", false);
			AddCheckbox("AddDestructor", "Add destructor", false);
		}

		public override void Generate(String outFileName, String outText, ref Flags generateFlags)
		{
			StringView name = mParams["Name"];
			bool addConstructor = mParams["AddConstructor"].Equals("True", true);
			bool addDestructor = mParams["AddDestructor"].Equals("True", true);
			if (name.EndsWith(".bf", .OrdinalIgnoreCase))
				name.RemoveToEnd(3);

			outFileName.Append(name);
			outText.Append(
				scope $"""
				using Nanoforge;
				using System;

				namespace {Namespace}
				{{
					public class {name}
					{{
				"""
			);

			if (addConstructor)
			{
				outText.Append(
					scope $"""

							this()
							{{	

							}}
					"""
				);
			}
			if (addDestructor)
			{
				outText.Append(
					scope $"""


							~this()
							{{	

							}}
					"""
				);
			}

			bool addExtraNewline = !(addConstructor || addDestructor); //Add extra newline if neither constructor or destructor present
			if (addExtraNewline)
			{
				outText.Append("\n");
			}

			outText.Append(
				scope $"""

					}}
				}}
				"""
			);
		}
	}
}