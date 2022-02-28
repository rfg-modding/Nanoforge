#include "ImGuiFontManager.h"
#include "Log.h"

CVar CVar_UIScale("UI Scale", ConfigType::Float,
    "Scale of the user interface.",
    ConfigValue(1.0f), //Default value
    true,  //ShowInSettings
    false, //IsFolderPath
    false, //IsFilePath
    0.5f, 2.0f //Min/max
);

void ImGuiFontManager::RegisterFonts()
{
    //Icon ranges supported by the current font (noto sans)
    //See a list of unicode characters supported by noto sans here: http://zuga.net/articles/unicode-all-characters-supported-by-the-font-noto-sans/
    //See a list of unicode character ranges here: https://jrgraphix.net/research/unicode_blocks.php
    static const ImWchar icons_ranges[] =
    {
        0x0020, 0x024F, //Basic Latin + Latin Supplement + Latin extended A & B
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
        ICON_MIN_FA, ICON_MAX_FA, //Font awesome icons
        0
    };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;

    //Load fonts from file with preset sizes
    ImGuiIO& io = ImGui::GetIO();
    FontSmall.Load(io, &icons_config, icons_ranges);
    FontDefault.Load(io, &icons_config, icons_ranges);
    FontMedium.Load(io, &icons_config, icons_ranges);
    FontL.Load(io, &icons_config, icons_ranges);
    FontXL.Load(io, &icons_config, icons_ranges);

    //Set default font
    io.FontDefault = FontDefault.GetFont();
}