#include "portal_world.h"

#include <stdio.h>
#include <malloc.h>

#define LIST_TAG SectorList
#define LIST_ITEM_TYPE Sector
#define LIST_ITEM_FREE_FUNC freeSector
#include "list_impl.h"

void freeSectorPoly(SectorPoly poly) {
    free(poly.points);
    free(poly.next_sectors);
}

void freeSector(Sector sector) {
    freeSectorPoly(sector.polygon);
}

void freePortalWorld(PortalWorld pod) {
    SectorList_free(pod.sectors);
}