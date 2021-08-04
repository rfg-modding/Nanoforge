# Nanoforge
A modding tool for the game Red Faction Guerilla. The main goal is to make it easier to mod RFG and to automate some of the more tedious and error-prone parts of modding. See [releases](https://github.com/Moneyl/Nanoforge/releases) to download the most recent version. See the [RF wiki](https://www.redfactionwiki.com/wiki/RF:G_Editing_Main_Page#Tutorials) for Nanoforge tutorials and RFG modding tutorials.

## Contents
- [Current features](https://github.com/Moneyl/Nanoforge#current-features)
- [Planned features](https://github.com/Moneyl/Nanoforge#planned-features)
- [Screenshots](https://github.com/Moneyl/Nanoforge#screenshots)
- [Build instructions](https://github.com/Moneyl/Nanoforge#build-instructions)

## Current features
Below are the current features of Nanoforge. See the screenshots section for examples.
- Map/territory viewing. Loads the terrain and zone data for the selected territory. Draws bounding boxes of map objects.
- Partial mesh viewing support + export to .obj. Supports static and character meshes. Attempts to auto find textures for each mesh. Importing meshes back into the game isn't supported yet
- Texture viewing, editing, and export to .dds. Can edit textures by editing them in an external tool and re-importing them.
- Xtbl editing. Editing is via a user interface so you don't have to directly edit xml data.
- Automatic modinfo.xml generation for file formats with editing support (textures and xtbl files). Modinfo.xml is a file used by the RFG Mod Manager. Editing this file is error prone so Nanoforge automates it when possible.

## Planned features 
Below are some planned features. This isn't a roadmap so their order does not denote the order which they'll be added. Any editing feature will likely come with auto modinfo generation for that type of edit.
- Mesh editing. Example: Exporting a .obj for the sledgehammer, editing it in blender, then re-importing it with Nanoforge for use in the game.
- Zone / map editing. At first this likely will only allow adding, removing, and moving objects. Terrain editing will come later since it's data is more complicated and stored separately from zone data.
- Support for viewing and editing other mesh formats like vehicles, and cchk_pc buildings. These will be added piecemeal.
- Scriptx editing. Likely using a node editor somewhat similar to UE4 blueprints. Nanoforge currently has a [very WIP version of this](https://imgur.com/GaRuqAY) that can view scriptx files but can't edit them.

## Screenshots
This screenshot shows the map view of the main campaign map. The panels on the left and right let you view the objects that make up each map zone and their properties. Color coded bounding boxes are drawn for each map object. Editing isn't supported yet. 

![](https://i.imgur.com/FTXfVAB.jpg)

Below is the mesh viewer. It currently supports viewing static meshes and character meshes. It attempts to auto-locate mesh textures and can export meshes as .obj files. If mesh textures were found it'll also export the textures as .dds files and a .mtl file describing the textures. These can be imported into blender and other 3D modeling software.

![](https://i.imgur.com/4jVOl5m.png)

This next picture shows texture viewing. RFG textures come in a pair of two files. Either a cpeg_pc paired with a gpeg_pc file or a cvbm_pc file paired with a gvbm_pc file (these are identical formats). Each pair can contain 1 or more images, which are listed near the top left corner of the texture. Textures can be exported to .dds files and imported for use in the game.

![](https://i.imgur.com/j0q4dah.jpg)

This final image shows a top down view of the whole main campaign map to demonstrate that Nanoforge can view the map at full scale.

![](https://i.imgur.com/onv4Dln.jpg)

## Build instructions
Below are the steps to manually build Nanoforge from source. Prebuilt versions are available in [Releases](https://github.com/Moneyl/Nanoforge/releases).

### Requirements
To build Nanoforge from source you'll need these programs:
- [Visual Studio 2019 with C++ Cmake tools](https://visualstudio.microsoft.com/downloads/) - If you already installed VS2019 and didn't check the CMake option you can install it by clicking `Tools > Get Tools and Features`, then checking "C++ Cmake tools for Windows" in the Individual Components tab of the installer.
- [Git](https://git-scm.com/) - For downloading dependencies.

### Steps
1) Clone the repo with `git clone https://github.com/Moneyl/Nanoforge.git`
2) `cd` into the directory you cloned to repo to then download the dependencies with `git submodule update --init --recursive`
3) Open the project in VS2019 by right clicking the folder you cloned it in and selecting "Open in Visual Studio". Alternatively select the folder with `File > Open > Folder...` in VS2019.
4) Build the project with `Build > Build Nanoforge.exe` (Ctrl + B). The first time opening the project you might need to wait for CMake config to finish before the option becomes available. I recommend using the "x64-RelWithDebInfo" build option since it'll be optimized but still have some debug info so you can use the debugger. Nanoforge is very slow when built with the normal debug option.
5) Once it's done building you can run it with the debugger attached using F5 or without using Ctrl + F5.
