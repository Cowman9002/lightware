#pragma once

#include "mathlib.h"
#include "camera.h"

typedef struct Sector Sector;
typedef struct SectorDef {
    float floor_height, ceiling_height;
} SectorDef;

typedef struct Sector {
    //  polygon all sub-sectors will use
    unsigned num_walls;
    vec2 *points; // list
    vec4 *planes; // list
    Sector **next_sectors; // list

    // definitions of sub sectors for sector over sector
    unsigned num_sub_sectors;
    SectorDef *sub_sectors; // list
} Sector;

#define LIST_TAG SectorList
#define LIST_ITEM_TYPE Sector
#include "list_header.h"

typedef struct PortalWorld {
    SectorList sectors;
} PortalWorld;

void freeSectorDef(SectorDef def);
void freeSector(Sector sector);
void freePortalWorld(PortalWorld pod);

bool pointInSector(Sector sector, vec3 point);
Sector *getSector(PortalWorld pod, vec3 point);
unsigned getSubSector(Sector *sector, vec3 point);

void portalWorldRender(PortalWorld pod, Camera camera);