#pragma once

#include "geo.h"
#include "util.h"

#define INVALID_SECTOR_INDEX (~0)

typedef struct Camera {
    float fov;
    unsigned sector;
    unsigned tier;
    vec3 pos;
    float rot;
    float rot_sin, rot_cos;
    float pitch;
    vec3 forward;
} Camera;

typedef struct SectorDef {
    unsigned start, length;
    unsigned num_tiers;
    float *floor_heights, *ceiling_heights; // first is world space, next are relative to last ceiling
} SectorDef;

typedef struct PortalWorld {
    Line *wall_lines;
    unsigned *wall_nexts;

    SectorDef *sectors;

    unsigned num_walls;
    unsigned num_sectors;
} PortalWorld;


unsigned getCurrentSector(PortalWorld pod, vec2 point, unsigned last_sector);
unsigned getSectorTier(PortalWorld pod, float z, unsigned sector_id);
void renderPortalWorld(PortalWorld pod, Camera cam);