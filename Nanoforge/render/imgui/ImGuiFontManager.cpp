#include "ImGuiFontManager.h"
#include "Log.h"

void ImGuiFontManager::Init(Config* config)
{
    TRACE();
    config_ = config;
}

void ImGuiFontManager::RegisterFonts()
{
    static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;

    //Load fonts from file with preset sizes
    ImGuiIO& io = ImGui::GetIO();
    FontSmall.Load(io, &icons_config, icons_ranges, config_);
    FontDefault.Load(io, &icons_config, icons_ranges, config_);
    FontMedium.Load(io, &icons_config, icons_ranges, config_);
    FontL.Load(io, &icons_config, icons_ranges, config_);
    FontXL.Load(io, &icons_config, icons_ranges, config_);
    //FontXXL.Load(io, &icons_config, icons_ranges, config_);
    //FontXXXL.Load(io, &icons_config, icons_ranges, config_);
    //FontXXXXL.Load(io, &icons_config, icons_ranges, config_);
    //FontXXXXXL.Load(io, &icons_config, icons_ranges, config_);
    //FontXXXXXXL.Load(io, &icons_config, icons_ranges, config_);

    //Set default font
    io.FontDefault = FontDefault.GetFont();
}
