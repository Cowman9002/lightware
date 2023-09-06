#pragma once

#include "mathlib.h"
#include "camera.h"

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

bool pointInSector(Sector sector, vec3 point);
Sector *getSector(PortalWorld pod, vec3 point);

void portalWorldRender(PortalWorld pod, Camera camera);