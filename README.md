# RF Viewer
Experiments with RFG packfiles and DX11. The main goal for the moment is visualizing the games file formats. So far it can load zone and terrain data from packfiles without fully extracting them and visualize that data in 3d. Only terrain meshes are loaded at the moment. All other objects are represented by their bounding boxes and filtered by type. See the pictures below for more details. 

## Screenshots
First, here's a screenshot with the bounding boxes of only one zone visible. Terrain rendering is very basic with no texturing and gaps between terrain meshes. Terrain color is set by elevation. 

![](https://i.imgur.com/dNuTbdv.png)

Next, this shows the full map with bounding boxes for all zones visible. Note that extremely common objects like navpoints are filtered out to keep things somewhat clear.

![](https://i.imgur.com/Bt3nStA.png)

Lastly here's a top down image of the map with no zone bounding boxes visible.

![](https://i.imgur.com/QW5RvV6.png)
