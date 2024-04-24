# Lightware

Software rendered engine heavily inspired by Ken Silverman's Build Engine with useful and modern features to enhance design capabilities.

_Very Buggy and Prone to Crashes_

## Getting started
- Open a powershell terminal at the root directory and call `.\conf.ps1 make dll` to make the dll for the engine
- Next call `.\conf.ps1 run editor` to launch the editor
- When the editor launches click the `OPEN PROJECT DIRECTORY` button and navigate to the root directory
- Press `Ctrl + O` and navigate to the `res/maps` directory and take a look at some of the maps :D

## Current features:
- Portal/sector based rendering.
- Multi-level sectors for true room over room (Mostly working).
- In engine 2D/3D level editor.

## Planned features:
- ~~Programmable shader pipeline.~~
- Wall, ceiling, and floor texture mapping.
- Directional sprites.
- Triangle models.
- Voxel models.
- Dynamic lighting.
- Baked lighting.
- Sector based global illumination (unsure of implementation details for now).

## Editor Colors:
- `White`: Normal lines
- `Red`: Portal lines
- `Blue`: Points

## Editor Controls:
- `Ctrl + O`: Open file
- `Ctrl + S`: (re)save file
- `Ctrl + Shift + S`: Save file as
---
- `TAB`: Swap between 3d and 2d view
- `W`: Move 2d view up
- `S`: Move 2d view down
- `A`: Move 2d view left
- `D`: Move 2d view right
- `Left`: Rotate 2d view left 90 degrees
- `Right`: Rotate 2d view right 90 degrees
- `Minus`: Decrease zoom
- `Equals`: Increase zoom
- `Scroll`: Change zoom
---
- `G`: Toggle grid
- `[`: Decrease grid size
- `]`: Increase grid size
- `Ctrl + Scroll`: Change grid size
- `P`: Toggle spectre select (allows selecting overlapping values)
---
- `Esc`: Cancel action
---
- `Left Mouse`: Select point
- `Shift + Left Mouse`: Select multiple points
- `Ctrl + Left Mouse`: Select sector's points
- `Shift + Ctrl + Left Mouse`: Select multiple sectors' points
---
- `Space`: Create new sector at point
- `X`: Delete points (Points must be selected)
- `C`: Split line (Hover above line at location where you want split to be)
- `V`: Auto portal (Hover two antiparallel line segments with the same end points to form a portal)
- `J`: Join sectors (press over first sector, then click second sector. Must be connected by portals)
- `B`: Box selection (Click when finished to select all points inside box)
---
### 3D only
- `W`: Move forwards
- `S`: Move back
- `A`: Move left
- `D`: Move right
- `Space`: Move up
- `Shift`: Move down
- `Up`: Look up
- `Down`: Look down
- `Left`: Look left
- `Right`: Look right
---
- `Ctrl + C`: Copy ceiling or floor height
- `Ctrl + V`: Paste ceiling or floor height
- `Q`: Increase floor or ceiling height
- `Z`: Increase floor or ceiling height
- `Scroll`: Change floor or ceiling height
---
- `F`: Add subsector
- `Ctrl + F`: Remove subsector