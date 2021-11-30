#include "ImGuiFontManager.h"
#include "Log.h"

CVar CVar_UIScale("UI Scale", ConfigType::Float,
    "Scale of the user interface. Must restart Nanoforge to for this change to apply.",
    ConfigValue(1.0f), //Default value
    true,  //ShowInSettings
    false, //IsFolderPath
    false, //IsFilePath
    0.5f, 2.0f //Min/max
);

void ImGuiFontManager::Init()
{
    TRACE();
}

void ImGuiFontManager::RegisterFonts()
{
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
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
