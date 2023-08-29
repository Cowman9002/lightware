#pragma once

#include "geo.h"
#include "util.h"

#define INVALID_SECTOR_INDEX (~0)

typedef struct Camera {
    unsigned sector;
    vec3 pos;
    float rot;
    float rot_sin, rot_cos;
} Camera;

typedef struct SectorDef {
    unsigned start, length;
    float ceiling_height, floor_height;
} SectorDef;

typedef struct PortalWorld {
    Line *wall_lines;
    unsigned *wall_nexts;

    SectorDef *sectors;

    unsigned num_walls;
    unsigned num_sectors;
} PortalWorld;


unsigned getCurrentSector(PortalWorld pod, vec2 point, unsigned last_sector);
void renderPortalWorld(PortalWorld pod, Camera cam);