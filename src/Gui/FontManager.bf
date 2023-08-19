using System.Collections;
using Nanoforge.App;
using Common;
using System;
using ImGui;

namespace Nanoforge.Gui
{
	//TODO: Convert this to use the new architecture. This was grabbed from old code and I didn't bother converting it yet since its job is simple
	public static class Fonts
	{
        public static ImGuiFont FontSmall;
        public static ImGuiFont FontDefault;
        public static ImGuiFont FontL;
        public static ImGuiFont FontXL;
        private static append List<u16> _iconRanges = .();
        public static bool EnableNonLatinGlyphs = false; //Currently disabled by default since I (only dev) only use english & enabling these increases boot time. Will enable when necessary

		public static void LoadFonts()
		{
            static bool firstRun = true;
            if (firstRun) //Build glyph list the first time this is called
            {
                //Base latin glyphs & FontAwesome fonts are always enabled
                _iconRanges.AddRange(scope u16[](0x0020, 0x024F));
                _iconRanges.AddRange(scope u16[](Icons.ICON_MIN_FA, Icons.ICON_MAX_FA));

                //Other glyphs can optionally be enabled. Disabled by default because they increase boot time and I only use english while programming.
                //Want to be able to enable them later when I add localization
                if (EnableNonLatinGlyphs)
                {
                    _iconRanges.AddRange(scope u16[]
					(
                        0x0250, 0x02AF, //IPA Extensions
                        0x02B0, 0x02FF, //Spacing Modifier Letters
                        0x0300, 0x036F, //Combining Diacritical Markers
                        0x0370, 0x03FF, //Greek and Coptic
                        0x0400, 0x044F, //Cyrillic
                        0x0500, 0x052F, //Cyrillic Supplementary
                        0x1AB0, 0x1ABE, //Combining Diacritical Marks Extended
                        0x1C80, 0x1C88, //Cyrillic Extended C
                        0x1D00, 0x1DBF, //Phonetic Extensions + Supplement
                        0x1DC0, 0x1DFF, //Combining Diacritical Marks Supplement
                        0x1E00, 0x1EFF, //Latin Extended Additional
                        0x1F00, 0x1FFE, //Greek Extended
                        0x2000, 0x206F, //General Punctuation
                        0x2070, 0x209C, //Superscripts and Subscripts
                        0x20A0, 0x20BF, //Currency Symbols
                        0x2100, 0x214F, //Letterlike Symbols
                        0x2150, 0x2189, //Number Forms
                        0x2C60, 0x2C7F, //Latin Extended C
                        0x2DE0, 0x2DFF, //Cyrillic Extended A
                        0x2E00, 0x2E44, //Supplemental Punctuation
                        0xA640, 0xA69F, //Cyrillic Extended B
                        0xA700, 0xA71F, //Modifier Tone Letters
                        0xA720, 0xA7FF, //Latin Extended D
                        0xAB30, 0xAB65, //Latin Extended E
                        0xFB00, 0xFB06, //Alphabetic Presentation Forms
                        0xFE20, 0xFE2F, //Combining Half Marks
                        0xFFFC, 0xFFFD, //Specials
                        0x3040, 0x309F, //Hiragana
                        0x30A0, 0x30FF, //Katakana
                        0x4E00, 0x9FFF, //CJK Unified Ideographs
					));
                }

                //Null terminate list so ImGui knows where it ends
                _iconRanges.Add(0);

                firstRun = false;
            }

			ImGui.FontConfig iconConfig = .();
			iconConfig.MergeMode = true;
			iconConfig.PixelSnapH = true;

			FontSmall.Load(16.0f, &iconConfig, _iconRanges.Ptr);
			FontDefault.Load(18.0f, &iconConfig, _iconRanges.Ptr);
			FontL.Load(26.0f, &iconConfig, _iconRanges.Ptr);
			FontXL.Load(33.5f, &iconConfig, _iconRanges.Ptr);

			ImGui.GetIO().FontDefault = FontDefault.Font;
		}

		//Font class used by FontManager
	  	public struct ImGuiFont
		{
			private float _size = 12.0f;
			private ImGui.Font* _font;

			public float Size { get { return _size; } }
			public ImGui.Font* Font { get { return _font; } }

			public void Push() { ImGui.PushFont(_font); }
			public void Pop() { ImGui.PopFont(); }
			public void Load(float size, ImGui.FontConfig* fontConfig, uint16* glyphRanges) mut
			{
				_size = size;
				var io = ImGui.GetIO();
				//Load font
				io.Fonts.AddFontFromFileTTF(scope $"{BuildConfig.AssetsBasePath}fonts/NotoSansDisplay-Medium.ttf", _size, null, glyphRanges);
				//Load FontAwesome image font
				_font = io.Fonts.AddFontFromFileTTF(scope $"{BuildConfig.AssetsBasePath}fonts/fa-solid-900.ttf", _size, fontConfig, glyphRanges);
			}
		}
	}
}