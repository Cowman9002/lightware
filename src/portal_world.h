#pragma once

#include "mathlib.h"

typedef struct Frustum {
    vec4 planes[6];
}Frustum;

typedef struct Sector Sector;
// polygon defined counter-clockwise by standard
typedef struct SectorPoly{
    size_t num_points;
    vec2 *points;
    vec4 *planes;
    Sector **next_sectors;
}SectorPoly;

typedef struct Sector {
    SectorPoly polygon;
    float floor_height, ceiling_height;
}Sector;

#define LIST_TAG SectorList
#define LIST_ITEM_TYPE Sector
#include "list_header.h"

typedef struct PortalWorld {
    SectorList sectors;
}PortalWorld;

void freeSectorPoly(SectorPoly poly);
void freeSector(Sector sector);
void freePortalWorld(PortalWorld pod);

void portalWorldRender(PortalWorld pod, mat4 vp_matrix, vec3 cam_pos, Frustum frustum);