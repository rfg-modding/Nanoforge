#include "ImGuiFontManager.h"

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
    FontL.Load(io, &icons_config, icons_ranges);
    FontXL.Load(io, &icons_config, icons_ranges);
    //FontXXL.Load(io, &icons_config, icons_ranges);
    //FontXXXL.Load(io, &icons_config, icons_ranges);
    //FontXXXXL.Load(io, &icons_config, icons_ranges);
    //FontXXXXXL.Load(io, &icons_config, icons_ranges);
    //FontXXXXXXL.Load(io, &icons_config, icons_ranges);

    //Set default font
    io.FontDefault = FontDefault.GetFont();
}
