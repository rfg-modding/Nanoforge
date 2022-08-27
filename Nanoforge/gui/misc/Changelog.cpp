#include "Changelog.h"
#include "gui/GuiState.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h> //For opening markdown links in the web browser
#include <imgui.h>
#include "render/imgui/ImGuiFontManager.h"
#pragma warning(disable:26495)
#include <imgui_markdown.h>
#pragma warning(default:26495)

//Fonts used by markdown headings
ImFont* H1 = NULL;
ImFont* H2 = NULL;
ImFont* H3 = NULL;
ImGui::MarkdownConfig markdownConfig;
bool FirstDraw = true;
//Used to reduce spacing before and after headings
f32 markdownHeadingAdjustment = 25.0f;

void InitMarkdownViewer(GuiState* state);
//Callbacks used to customize markdown viewer
void LinkCallback(ImGui::MarkdownLinkCallbackData data);
void TooltipCallback(ImGui::MarkdownTooltipCallbackData data);
void FormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo, bool start);

void DrawChangelog(GuiState* state)
{
    if (FirstDraw)
    {
        InitMarkdownViewer(state);
        FirstDraw = false;
    }

    static f32 borderWidth = 10.0f;
    static f32 indent = 8.0f;
    static ImVec4 backgroundColor = { 0.07f, 0.07f, 0.078f, 1.0f };

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { borderWidth, ImGui::GetStyle().ItemSpacing.y });
    if (ImGui::BeginChild("Changelog", { 0, 0 }, false, ImGuiWindowFlags_AlwaysVerticalScrollbar))
    {
        const string& changelog = GetChangelogText();
        ImGui::Indent(indent);
        ImGui::Markdown(changelog.c_str(), changelog.length(), markdownConfig);
        ImGui::EndChild();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void InitMarkdownViewer(GuiState* state)
{
    H1 = state->FontManager->FontXL.GetFont();
    H2 = state->FontManager->FontL.GetFont();
    H3 = state->FontManager->FontMedium.GetFont();

    markdownConfig.linkCallback = LinkCallback;
    markdownConfig.tooltipCallback = TooltipCallback;
    markdownConfig.imageCallback = nullptr;
    markdownConfig.linkIcon = ICON_FA_LINK;
    markdownConfig.headingFormats[0] = { H1, true };
    markdownConfig.headingFormats[1] = { H2, true };
    markdownConfig.headingFormats[2] = { H3, false };
    markdownConfig.userData = nullptr;
    markdownConfig.formatCallback = FormatCallback;
}

void LinkCallback(ImGui::MarkdownLinkCallbackData data)
{
    std::string url(data.link, data.linkLength);
    if (!data.isImage)
    {
        ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

void TooltipCallback(ImGui::MarkdownTooltipCallbackData data)
{
    ImGui::BeginTooltip();
        ImGui::TextUnformatted(data.linkIcon);
        ImGui::SameLine();
        ImGui::TextUnformatted("Open in browser");

        ImGui::TextColored(gui::SecondaryTextColor, string(data.linkData.link, data.linkData.linkLength).c_str());

    ImGui::EndTooltip();
}

void FormatCallback(const ImGui::MarkdownFormatInfo& markdownFormatInfo, bool start)
{
    //Call the default first so any settings can be overwritten by our implementation
    ImGui::defaultMarkdownFormatCallback(markdownFormatInfo, start);

    switch (markdownFormatInfo.type)
    {
    case ImGui::MarkdownFormatType::HEADING:
        ImGui::SetCursorPos({ ImGui::GetCursorPosX(), ImGui::GetCursorPosY() - markdownHeadingAdjustment });
        break;
    case ImGui::MarkdownFormatType::LINK:
        if (start)
            ImGui::PushStyleColor(ImGuiCol_Text, gui::SecondaryTextColor);
        else
            ImGui::PopStyleColor();

        break;
    default:
        break;
    }
}

const string changelog =
R"(# Getting started
  * [See the latest releases on Github](https://github.com/Moneyl/Nanoforge/releases)
  * See [Nanoforge basics](https://www.redfactionwiki.com/wiki/Nanoforge_basics) for a guide on how to use Nanoforge and it's features.
  * See [Nanoforge xtbl editing](https://www.redfactionwiki.com/wiki/Nanoforge_xtbl_editing) for a guide on using Nanoforge to edit xtbls.
  * See [Nanoforge texture editing](https://www.redfactionwiki.com/wiki/Nanoforge_Texture_Editing) for a guide on using Nanoforge to edit textures.

# Changelog
## v0.20.0 - Basic map editor (MP/WC only)

### Changes




## v0.19.0 - High resolution terrain + Mesh viewer improvements
This release adds support for high lod terrain meshes and high resolution terrain textures (see it via `Tools > Open Territory`). It also fixes some mesh rendering bugs and has a much quicker texture search that fails less often.

### Changes
  * High resolution terrain meshes and textures in the map viewer. Can be disabled via `File > Settings`. Doesn't exactly match in game appearance yet. Loading the main map with high res terrain may use a lot of RAM until loading is complete. This can be lowered by decreasing the thread limit in `File > Settings`.
  * The file cache used by previous versions was removed since it had minimal benefits. You can delete your 'Cache' folder if you're installing over an old version.
  * Added LOD level slider for meshes that have multiple lod levels.
  * The mesh exporter now exports gltf files instead of obj files. Different lod levels are organized into their own subnodes and labelled within the gltf.
  * Mesh viewer lighting improvements.
  * Nanoforge now limits the number of threads it uses for background tasks.  `File > Settings` has a MaxThreads setting. This lets you limit how many threads can be used by background tasks. By default it saves 2 threads for your system to avoid lagging your entire PC during intensive tasks like level loading.
  * Added 'Tasks' button at the bottom left of the screen that shows the status of background tasks when clicked.
  * SRGB textures are properly displayed in the texture editor. An SRGB label was also added to the sidebar.
  * Less useful texture info in the viewer is now placed in a "Additional info" section that's collapsed by default.
  * Improved settings.xml regeneration / upgrading
  * BUG: Fixed mesh rendering and export bugs like erroneous triangles and inside out meshes. Occurred when meshes consisted of multiple submeshes.
  * BUG: Fixed slow & infinite texture searches in the mesh viewer. Searches can still fail, but they'll fail quickly.
  * BUG: Fixed missing triangles in low lod terrain.
  * BUG: Fixed a crash when minimizing a window while a mesh or territory viewer is open.

## v0.18.1 - Texture mod bugfix
Fixes bug with editing files in str2_pc files that would output an invalid final mod.

## v0.18.0 - Quality of life improvements
Bugfixes and QOL improvements to make Nanoforge nicer to use.

### Changes
  * Replaced the windows folder browser with the better one that has the favorites sidebar, search, and more.
  * BUG: Fixed asm_pc documents being unclosable.
  * BUG: Fixed crash when Settings.xml is missing or using the old settings format.
  * Automatically upgrades Settings.xml if it's using the old format.
  * Improved error logging to help detect bugs and crashes.
  * Made the file explorer case insensitive by default with an option for case sensitive searches.
  * `File > Mod packaging` now shows a popup that lets you change the mod name, author, and description before packaging.
  * BUG: Fixed bug when creating a new project when one is already open that would copy the edits of the first project to the new one.
  * BUG: Fixed xtbl edits persisting when loading a new project when one is already open.
  * Added more search paths for RFG auto detection.
  * You no longer need to create a project to view files. You only need to for file edits and mod packaging.
  * There isn't a welcome window anymore. Instead the main window opens immediately and there's a startup panel that lists recent projects.
  * Added a built in changelog to the startup panel.
  * The recent projects list is more readable.
  * Added basic edit tracking and warnings when you're about to close a file that has unsaved changes.
  * Files can now be saved with Ctrl + S.


## v0.17.0 - Initial support for RFG localization files
Initial support for viewing RFGs localized strings. Xtbls often use placeholder strings which are replaced at runtime with their localization. This release shows localizations next to placeholders in the xtbl editor.

### Important
  * This release only supports locales that are supported by the vanilla game and which use the Latin alphabet. The supported languages are Czech, German, English, Spanish, French, Italian, and partial support for Polish. Polish uses some additional characters which are missing in Nanoforge since it's font only supports Latin characters currently.
  * This release doesn't localize any of Nanoforge's UI. It still uses English.

### Changes
  * Added support for viewing RFGs localized strings.
  * Added a full list of localized strings accessible via `Tools > Localization > View localized strings` on the main menu bar.
  * Localized strings are shown in the xtbl editor next to placeholders.


## v0.16.0 - New welcome screen + bugfixes
This release adds a settings menu and fixes several bugs.

### Important
  * The format of Settings.xml changed in this release so old copies of it aren't compatible. If you're extracting this release onto an older one be sure to replace your old Settings.xml with the one included in this release. You also shouldn't edit Settings.xml by hand any more if you can avoid it. You can edit your data path in the welcome screen and you can edit all your other settings in the settings menu.

### Changes
  * Redesigned the welcome screen.
  * Added data folder auto detection + a warning on the welcome screen if your data folder is invalid. If it doesn't auto detect your copy of RFG let me know on discord and I can add it to the list of locations that Nanoforge checks.
  * Added a settings menu. Accessible from the welcome screen or from the main menu bar via `File > Settings`.
  * BUG: Fixed a major memory leak that affected the mesh viewer.
  * BUG: Fixed crash when opening two meshes with the same name.


## v0.15.0 - Xtbl editing + performance improvements
This release adds xtbl editing with modinfo.xml generation. It also makes some improvements to baseline memory and cpu usage.

## Changes
  * Xtbl editing with auto modinfo.xml generation.
  * Reduced baseline RAM usage by ~30MB.
  * Improved idle CPU usage. Previously it would always use an entire core, even when idling. Now idle CPU usage can be < 1% depending on your CPU.
  * File explorer searches are now threaded so they don't cause lag.
  * Added asm_pc file viewer.
  * Color coded icons in file explorer.
  * BUG: Fixed crash that could occur when clicking items in the zone object list.
  * The FPS counter can now be toggled in Settings.xml. It's disabled by default.

## Known issues
Below are some known issues with xtbl editing that will be remedied in upcoming releases:
  * Some tables are difficult to read. See character.xtbl and scroll down to "Props" for an example of this.
  * You can't add/remove items from tables, only edit existing ones.
  * Values without a description in the \<TableDescription\> section of an xtbl aren't shown.


## v0.14.0 - Zone list improvements + mission and activity zones
This release improves the zone list and adds mission and activity zones to it for territories with those (terr01 and dlc01).

## Changes
  * Adds mission and activity zones to the zone list. This only applies to the terr01 and dlc01 territories. The multiplayer and wrecking crew maps don't have missions or activities.
  * Converted the zone list to a table and added some more columns. This provides a bit more info about zones and is more readable. Columns can be re-ordered but sorting by any column isn't supported yet. Zones are still sorted by object count.


## v0.13.0 - Zone object list improvements + bugfixes
This release improves the zone object list and object visibility in the 3D map view.

## Changes
  * Improves visibility of selected object in 3D map view. Previously the color changing effect didn't work well on objects with bright colors.
  * Make objects for all visible zones visible in zone object list. Previously required that you click on a zones name in the zone list. Wasn't intuitive to use.
  * BUG: Fix crash when entering no description in the "New project" window + added some validation for new project creation.


## v0.12.0 - WIP Scriptx viewer made public
This release makes an incomplete scriptx viewer accessible in public builds. Access through view > scriptx viewer once you've opened a project. The viewer is incomplete and buggy. I don't plan on finishing it at the moment but made it public since it might be useful for people editing scriptx files. They're a bit easier to understand in node graph form.

## Changes
  * Made incomplete scriptx viewer public in case it's useful for scriptx modders


## v0.11.0 - Zone viewing improvements
This release makes a few improvements to the the zone object list and fixes a bug that caused many zone objects to be hidden from the list.

## Changes
  * Object `Handle` and `Num` values are now shown in the properties panel.
  * Button to copy the objects scriptx reference to the system clipboard added to the properties panel. Scriptx files frequently reference map objects.
  * If an object is selected in the objects list it'll have a have visibility marker drawn for it in the 3D viewport.
  * If an object is set to visible in the filters but it's parent is hidden the object and it's parent will still be shown in the object list.
  * BUG: Fixed a bug that caused many objects to be hidden from the zone object list. Their bounding boxes would still be drawn in the 3D viewport but they wouldn't show up in the list.


## v0.10.0 - UI/UX Improvements
This release makes a few improvements to the file explorer search bar and zone object list.

## Changes
  * Option to hide unsupported formats in the file explorer.
  * Regex search option in file explorer. Wildcard (*) is still supported as well when regex is disabled.
  * Nanoforge will attempt to name objects in the zone object list when you're viewing territories.
  * Territory viewing has moved. It's now in `Tools > Open territory` on the main menu bar. It's recommended that you wait for a territory to finish loading before opening another one. Loading multiple territories at once is supported but may bug out if you load them concurrently.


## v0.9.0 - Map view thick bounding boxes
Improves object bounding box visibility by making them thicker.

## Changes
  * Added wide line support for object bounding boxes. This requires geometry shader support. Most systems should support these by now but if Nanoforge refuses to run try setting `UseGeometryShaders` in settings.xml to `False` (this will disable line thickness control). You can change line thickness by editing the thickness value at the top of `Assets/Shaders/Linelist.hlsl` with a text editor. Shaders are automatically reloaded when edited so you can edit the value while Nanoforge is running.


## v0.8.0 - UI improvements and bugfixes
This release re-adds object bounding boxes in the territory view, makes a few UI improvements, and fixes some bugs.

## Changes
  * Now draws object bounding boxes in the territory view. This existed in a previous version but was broken. Note: This implementation is is pretty simple so there's no control over box thickness. I plan to add thicker lines and improve performance in a future update.
  * Added a progress bar for mod packaging. This way you can see it's progress and cancel part way through.
  * Added specific error message in log if imported textures don't fit dimension requirements (width and height must both be multiples of 4).
  * Automatic data path validation. If the folder set as your data path in Settings.xml doesn't contain any of the default vpp_pc files Nanoforge will warn you upon opening a project. Table.vpp_pc is ignored since it's necessary to delete/rename it to use many mods.
  * BUG: Fixed a crash related to territory/terrain loading.
  * BUG: Alleviated ui lock up when closing meshes or territories before they're fully loaded. There may still be a slight pause, but not the 20-30 second ones that were possible before.


## v0.7.0 - Terrain texturing and Windows 7 compatibility
This release comes with compatibility improvements for Windows 7 and low resolution texturing for terrain that matches in game textures.

## Changes
  * Fixed a compatibility issue with Windows 7. It's been tested to work on at least one system. If it still fails to start try installing the update linked in the requirements section.
  * Terrain is colored with a low resolution version of what you see in game. Looks more interesting than all terrain being grey. A higher resolution version will be added in a future update.
  * BUG: Object bounding boxes are no longer drawn in the 3D map view. This was broken due to necessary renderer changes and will be fixed in the next update.
  * BUG: Mod packaging causes the UI to freeze while it's running. This is expected and generally will only last a few seconds. If you've edited many textures it will take longer. This will be fixed and a progress bar will be added in the next update.


## v0.6.0 - Project rename + change in direction
This update includes several new features and marks a change in direction from being a file viewer with no editing features to being a general modding tool which will start adding editing features. To fit this new direction the name has been changed from RF Viewer to Nanoforge.

## Changes
  * Mesh viewer. Can view .csmesh_pc and .ccmesh_pc files and export them as .obj files. It will try to automatically locate textures for meshes when you open them. Open meshes through the file explorer.
  * Texture viewing, exporting, and editing. Open textures through file explorer. See [Nanoforge basics](https://www.redfactionwiki.com/wiki/Nanoforge_basics#Textures) for more details.
  * Automatic modinfo.xml generation from edits made. Currently only supports texture edits. If the editing texture is in a str2_pc file that file will be auto repacked and the asm_pc file file that references will be updated. See [mod packaging](https://www.redfactionwiki.com/wiki/Nanoforge_basics#Mod_packaging). Note: This only supports RFG Re-mars-tered. Steam edition support will come in a future update.
  * Project system to track edits. Note: You must create a project to use Nanoforge even if you don't plan on doing any editing. This only has to be done once.
  * Improved terrain rendering. Fixed the gaps between terrain and lighting bugs. Still some missing triangles.
  * You can now open multiple map views at once. So, for example, you could view the main campaign map, the DLC campaign map, and several MP maps all at once by docking their windows adjacent to each other. Only the map window that you clicked on most recently is rendered each frame so there should be little to no framerate cost to having many maps open at once after they're done loading.
  * New dark UI theme.
  * Basic UI scaling through `UI_Scale` option in Settings.xml. Values greater than one make the UI larger and values less than one make it smaller. You must restart Nanoforge to see the effects of changing Settings.xml.
  * The file explorer now shows the contents of str2_pc files and has a search bar with `*` wildcard support.
  * BUG: Object bounding boxes are no longer drawn in the 3D map view. This was broken due to necessary renderer changes and will be fixed in the next update.
  * BUG: Mod packaging causes the UI to freeze while it's running. This is expected and generally will only last a few seconds. If you've edited many textures it will take longer. This will be fixed and a progress bar will be added in the next update.
)";

const string& GetChangelogText()
{
    return changelog;
}