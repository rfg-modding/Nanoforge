# RF Viewer
Experiments with RFG packfiles and DX11. The main goal for the moment is visualizing the games file formats. So far it can load zone and terrain data from packfiles without fully extracting them and visualize that data in 3d. Only terrain meshes are loaded at the moment. All other objects are represented by their bounding boxes and filtered by type. See the pictures below for more details. 

## Current features
Below are the current features of Nanoforge. See the screenshots section for examples and more details.
- Map/territory viewing. Loads the terrain and zone data for the selected territory. Supports the main campaign map, the DLC map, and the WC and MP maps. Does not load many aspects of the map like building and object meshes yet.
- Partial mesh viewing support. Can open csmesh_pc, gsmesh_pc, ccmesh_pc, and gcmesh_pc files. Attempts to auto find textures for each mesh and can export meshes to .obj files. Does not support importing meshes back into the game yet.
- Texture viewing, exporting, and re-importing. Exports meshes to .dds files. Can edit the games textures by re-importing .dds files and replacing existing texture entries.
- Limited support for automatic modinfo.xml generation. Modinfo.xml files are used by the mod manager to install mods. The mod manager is the most common and convenient way to install RFG mods. Currently only supported for texture editing. If you edit a texture through Nanoforge it will track this change. With `File > Export mod` Nanoforge will generate a modinfo for the textures you edited and place any edited files in a folder with that modinfo. If an edited texture is in a str2_pc file Nanoforge will repack that str2_pc file and update the asm_pc file that references that str2_pc, then place them in the output folder.

## Planned features 
Below are some planned features. This isn't a roadmap so their order does not denote the order which they'll be added. Any editing feature will likely come with auto modinfo generation for that type of edit.
- Xtbl editing similar to Volitions [table file editor](https://github.com/volition-inc/Kinzies-Toy-Box/tree/master/tools/table_file_editor).
- Mesh editing. Example: Exporting a .obj for the sledgehammer, editing it in blender, then re-importing it with Nanoforge for use in the game.
- Zone(map) editing. At first this likely will only allow adding, removing, and editing objects. Terrain editing will come later since it's data is more complicated and stored separately from zone data.
- Support for viewing and editing other mesh formats like ccar_pc(vehicles), cfmesh_pc(foliage/rocks), and cchk_pc(destructibles like buidlings). These will likely be added piecemeal.
- Scriptx editing. Likely using a node editor somewhat similar to UE4 blueprints. See [here](https://imgur.com/GaRuqAY) for an example of how that might look from a very WIP version of this feature that's on the backburner.

## Screenshots
First here's a screenshot showing a view of the main campaign map. The panels on the left and right let you view the objects that make up each map zone and some of their properties. Editing isn't supported yet but is hopefully coming soon. Building and object meshes aren't shown in this view yet but there is an option to draw cubes where each object is, colored by object type. If you've seen early versions of this tool you might notice that the lighting is improved and that there's no longer seams in the terrain.

![](https://i.imgur.com/2HOkcKJ.png)

This next image shows mesh viewing. Nanoforge can currently view static meshes (csmesh_pc & gsmesh_pc files) and character meshes (ccmesh_pc & gcmesh_pc files). When you open one of the meshes it will try to automatically locate it's textures and apply them to the mesh. It also supports exporting these meshes to .obj files which can then be imported into tools like blender. If textures were found for the mesh they'll be exported with the obj as a .mtl file. Re-importing meshes into t he game isn't supported yet, but will be in the future.

![](https://i.imgur.com/4jVOl5m.png)

This next picture shows texture viewing. RFG textures come in a pair of two files. Either a cpeg_pc paired with a gpeg_pc file or a cvbm_pc file paired with a gvbm_pc file (these are identical formats). Each pair can contain 1 or more images, which are listed near the top left corner of the texture. Textures can be exported to .dds files and imported for use in the game.

![](https://i.imgur.com/j0q4dah.jpg)

This final image shows a town down view of the whole main campaign map as a demonstration of Nanoforge showing the map at full scale.

![](https://i.imgur.com/gcWzvqH.png)
