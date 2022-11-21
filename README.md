<h1 align="center">Nanoforge</h1>
<p align="center">A modding tool and file viewer for Red Faction Guerrilla.</p>

<h3 align="center">
  <a href="https://github.com/Moneyl/Nanoforge/releases">Releases</a> •
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#build-instructions">Build instructions</a>
</h3>

<p align="center"><img src="https://user-images.githubusercontent.com/8206401/168496531-fc52d441-dacd-468c-b03d-cdcdde5a9ae6.jpg" width="100%"/>
</p>

Nanoforge is a modding tool for the game Red Faction Guerilla. Its goals are to make RFG modding easier and less error prone, and to provide tooling for all of its file formats. See [releases](https://github.com/Moneyl/Nanoforge/releases) to download the most recent version. See the [RF wiki](https://www.redfactionwiki.com/wiki/RF:G_Editing_Main_Page#Tutorials) for Nanoforge and RFG modding tutorials.

## Features
- Map viewing. Loads terrain and object data for a map. Draws object bounding boxes.
- Mesh viewing and export for some RFG mesh formats. Auto locates textures.
- Texture viewing, exporting, and re-importing.
- XTBL editing via a user interface with tooltips and data validation.
- Automatic mod manager mod generation based on edits.
- Map list isn't hard-coded and shows the in game name for maps alongside the vpp name, useful for when making new maps is figured out. 
- Description text boxes for objects.
- Option to hide bounding boxes; useful for when you need precision with meshes that render properly in the Map viewer. 
- Map editor supports cloning and deep cloning between two maps in the outliner right click menu; both maps must be imported in the current project and opened first.
- Map editor supports editing parent/child references: adding dummy children, orphaning children, changing parents etc. 
- Map editor supports adding dummy objects: these are blank objects that can be used to create objects from scratch by referencing objects already on maps. Be careful because it gives you a lot of freedom and if you make a mistake it can cause issues. This is a stopgap measure until a proper object creation UI can be added in the Rewrite.
- Map editor has partial support for singleplayer maps: highlighting zones and objects in the outliner, checkboxes in the filters panel or you can press f to toggle zone highlighting or g to toggle object highlighitng. Things to note: you must start a new game to see all edits apply, importing/exporting the main map (terr01) takes a very long time and it's recommended to either keep your project for the base game backed up after import or edit the DLC SP map instead (dlc01) as it's faster and less demanding. Some SP specific properties can't be added to objects yet without hand editing your project file including constraints, road paths and navpoints & cloning doesn't work yet but you can edit/move existing objects fine.  

## Useful things to know 
- CTRL + S: save the project.
- CTRL + D: clone selected object.
- CTRL + I : copy scriptx reference from selected object.
- CTRL + B: remove world anchors from selected object.
- CTRL + N: remove dynamic link from selected object.
- DEL : delete selected object. 
- F: toggle hovered highlighting for selected zone.
- G: toggle hovered highlighting for selected object. 
- Outliner and inspector are opened by default.
- Double clicking or clicking the arrow next to the name for an object in the outliner opens/closes it.
- To highlight zones and objects in the map when mousing over them you can use the checkbox in the filters panel or press F to toggle zone highlighting and G to toggle object highlighting. 
- There are no warnings in the console about buildings that fail to load; this is expected behaviour since the building importer is incomplete. 
- You can delete object properties by clicking the X button next to each property.
- You can add properties to objects by using the "Add property" at the bottom of the inspector.
- Axis lines for selected objects are drawn at the object position and not the center of the bbox. 
- The texture editor is incomplete, it can crash if you have multiple textures open and it can be unreliable and prone to corruption with some files like always_loaded.cpeg_pc/gpeg_pc. Recommened to edit a single texture at a time, save your project and restart Nanoforge in-between edits to minimize issues. 

## Screenshots
Nanoforge can view and export RFG meshes as obj files.

![](https://i.imgur.com/4jVOl5m.png)

Nanoforge can view and export textures, plus re-import them back into the game.

![](https://i.imgur.com/j0q4dah.jpg)

## Build instructions
Follow these steps to build Nanoforge from source. Prebuilt versions are available in [Releases](https://github.com/Moneyl/Nanoforge/releases).

### Requirements
To build Nanoforge from source you'll need these programs:
- [Visual Studio 2019 with C++ Cmake tools](https://visualstudio.microsoft.com/downloads/) - If you already installed VS2019 and didn't check the CMake option you can install it by clicking `Tools > Get Tools and Features`, then checking "C++ Cmake tools for Windows" in the Individual Components tab of the installer.
- [Git](https://git-scm.com/) - For downloading dependencies.

### Steps
1) Clone the repo with `git clone https://github.com/Moneyl/Nanoforge.git`
2) `cd` into the directory you cloned the repo to then download the dependencies with `git submodule update --init --recursive`
3) Open the project in VS2019 by right clicking the folder you cloned it in and selecting "Open in Visual Studio". Alternatively select the folder with `File > Open > Folder...` in VS2019.
4) Build the project with `Build > Build Nanoforge.exe` (Ctrl + B). Make sure Nanoforge.exe is selected as the build target. The first time opening the project you might need to wait for CMake config to finish before the option becomes available. I recommend using the "x64-RelWithDebInfo" build option since it'll be optimized but still have some debug info so you can use the debugger. Nanoforge is very slow when built with the normal debug option.
5) Once it's done building you can run it with the debugger attached using F5 or without using Ctrl + F5.
