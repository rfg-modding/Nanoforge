<h1 align="center">Nanoforge</h1>
<p align="center">A map editor and modding tool for Red Faction Guerrilla.</p>

<h3 align="center">
  <a href="https://github.com/Moneyl/Nanoforge/releases">Releases</a> •
  <a href="#features">Features</a> •
  <a href="#screenshots">Screenshots</a> •
  <a href="#build-instructions">Build instructions</a>
</h3>

<p align="center"><img src="https://github.com/rfg-modding/Nanoforge/assets/8206401/c74d2fbc-1794-419f-9bd6-b24f605d0b80" width="100%"/>
</p>

Nanoforge is a modding tool for the game Red Faction Guerilla. Its goals are to make RFG modding easier and less error prone, and to provide tooling for all of its file formats. See [releases](https://github.com/Moneyl/Nanoforge/releases) to download the most recent version. See the [RF wiki](https://www.redfactionwiki.com/wiki/RF:G_Editing_Main_Page#Tutorials) for Nanoforge and RFG modding tutorials.

## Important note
The master branch is a WIP rewrite of Nanoforge in the [Beef](https://www.beeflang.org/) programming language. It currently doesn't have all the features described in the readme. The old C++ version can be found in `CppBranch`. It has more features but is no longer under development. It's being rewritten because I got tired of dealing with C++. Beef is a less mature language but the development experience is significantly better. I can focus on the development instead of fighting the language and looking out for footguns.

This is also a good chance to rewrite some old code. Much of the old code hasn't scaled well as the goals of the project have grown. It started off being a simple map data visualizer and now the goal is to be a modding tool for any of RFGs file formats. The rewrite will only be a Multiplayer and Wrecking Crew map editor to start. Those maps are much simpler than the single player maps so they're a good place to start and build a solid foundation.

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
- [Beeflang IDE (windows only)](https://www.beeflang.org/) - To compile the source code.
- [Git](https://git-scm.com/) - For downloading dependencies.

### Steps
1) After installing git open a command line window with access to the `git` command.
2) Use `cd` to move to the directory you want to clone NF into. Clone the repo with `https://github.com/rfg-modding/Nanoforge.git`.
3) `cd` into the directory you cloned the repo to then download the dependencies with `git submodule update --init --recursive`.
4) Open the project in the Beef IDE via `File > Open > Open Workspace...`.
5) Build and run the project with F5. You may have to build it twice the first time you run it. Try building again if you see errors on the first attempt. If issues persist create an issue or contact a dev on discord.
