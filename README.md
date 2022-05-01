<h1 align="center">Nanoforge</h1>
<p align="center">A modding tool and file viewer for Red Faction Guerrilla.</p>

<h3 align="center">
  <a href="https://github.com/Moneyl/Nanoforge/releases">Releases</a> •
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#build-instructions">Build instructions</a>
</h3>

<p align="center"><img src="https://camo.githubusercontent.com/7a6cd157cb680303b3ccb8edab74734d6c3a24060de4e5ce2b7500c4839cc73f/68747470733a2f2f692e696d6775722e636f6d2f465458665641422e6a7067" width="100%"/>
</p>

Nanoforge is a modding tool for the game Red Faction Guerilla. Its goals are to make RFG modding easier and less error prone, and to provide tooling for all of its file formats. See [releases](https://github.com/Moneyl/Nanoforge/releases) to download the most recent version. See the [RF wiki](https://www.redfactionwiki.com/wiki/RF:G_Editing_Main_Page#Tutorials) for Nanoforge and RFG modding tutorials.

Note: The master branch contains the latest code which may unstable or WIP. See the [v0.19.x branch](https://github.com/Moneyl/Nanoforge/tree/v0.19.x) for code from the last stable release. This way master has the latest code and is free to make significant changes and bugfixes can be made on the last stable release without introducing new features.

## Features
- Map viewing. Loads terrain and object data for a map. Draws object bounding boxes.
- Mesh viewing and export for some RFG mesh formats. Auto locates textures.
- Texture viewing, exporting, and re-importing.
- Xtbl editing via a user interface with tooltips and data validation.
- Automatic mod manager mod generation based on edits.

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
