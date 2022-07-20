#pragma once
#include <string>

#define STRINGIFY(value) #value
#define MAKE_VERSION_STRING(major, minor, patch, suffix) "v" STRINGIFY(major) "." STRINGIFY(minor) "." STRINGIFY(patch) suffix

namespace BuildConfig
{
    //Path to the asset folder. Varies depending on build type. See src/CMakeLists.txt.
    static const std::string AssetFolderPath = ASSET_FOLDER_PATH;
    static const std::string MainFontPath = AssetFolderPath + "fonts/NotoSansDisplay-Medium.ttf";
    //static const std::string JapaneseFontPath = AssetFolderPath + "fonts/NotoSansJP-Medium.otf";
    static const std::string FontAwesomePath = AssetFolderPath + "fonts/fa-solid-900.ttf";
    static const std::string VsCodeIconsPath = AssetFolderPath + "fonts/codicon.ttf";
    static const std::string ShaderPath = AssetFolderPath + "shaders/";

#define NanoforgeVersionMajor 0
#define NanoforgeVersionMinor 20
#define NanoforgeVersionPatch 0
#define NanoforgeVersionSuffix "-pre8"
    static const std::string Version = MAKE_VERSION_STRING(NanoforgeVersionMajor, NanoforgeVersionMinor, NanoforgeVersionPatch, NanoforgeVersionSuffix);
}