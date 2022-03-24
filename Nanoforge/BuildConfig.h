#pragma once
#include <string>

namespace BuildConfig
{
    //Path to the asset folder. Varies depending on build type. See src/CMakeLists.txt.
    static const std::string AssetFolderPath = ASSET_FOLDER_PATH;
    static const std::string MainFontPath = AssetFolderPath + "fonts/NotoSansDisplay-Medium.ttf";
    //static const std::string JapaneseFontPath = AssetFolderPath + "fonts/NotoSansJP-Medium.otf";
    static const std::string FontAwesomePath = AssetFolderPath + "fonts/fa-solid-900.ttf";
    static const std::string VsCodeIconsPath = AssetFolderPath + "fonts/codicon.ttf";
    static const std::string ShaderPath = AssetFolderPath + "shaders/";
    static const std::string Version = "v0.20.0-pre0";
}