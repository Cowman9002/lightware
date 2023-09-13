#include "internal.h"

#include <stdio.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "LWBB"

#define LOAD_MIN_SUPPORTED_VERSION 1
#define LOAD_MAX_SUPPORTED_VERSION 1

#define SAVE_VERSION 1

typedef struct LevelHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t checksum; // TODO Implement
    uint32_t num_sectors;
    uint32_t num_subsectors;
    uint32_t num_walls;
} LevelHeader;

typedef struct LevelSector {
    uint32_t num_subsectors;
    uint32_t num_walls;
    uint32_t first_subsector_offset;
    uint32_t first_wall_offset;
} LevelSector;

typedef struct LevelSubsector {
    float floor_height;
    float ceiling_height;
} LevelSubsector;

typedef struct LevelWall {
    float x;
    float y;
    uint32_t next;
    uint32_t portal_sector;
    uint32_t portal_wall;
} LevelWall;

typedef struct LevelFile {
    LevelHeader header;
    LevelSector *sectors;
    LevelSubsector *subsectors;
    LevelWall *walls;
} LevelFile;

bool _deserializeLevelFile(LevelFile level_file, LW_PortalWorld *pod);
bool lw_loadPortalWorld(const char *path, LW_PortalWorld *o_pod) {
    LevelFile level_file;
    memset(&level_file, 0, sizeof(level_file));

    FILE *file = fopen(path, "rb");
    if (file == NULL) return false;

    if (fread(&level_file.header, sizeof(level_file.header), 1, file) != 1) goto _error_exit;

    // check header
    if (level_file.header.magic[0] != MAGIC[0]) goto _error_exit;
    if (level_file.header.magic[1] != MAGIC[1]) goto _error_exit;
    if (level_file.header.magic[2] != MAGIC[2]) goto _error_exit;
    if (level_file.header.magic[3] != MAGIC[3]) goto _error_exit;

    if (level_file.header.version < LOAD_MIN_SUPPORTED_VERSION ||
        level_file.header.version > LOAD_MAX_SUPPORTED_VERSION)
        goto _error_exit;

    level_file.sectors    = calloc(level_file.header.num_sectors, sizeof(*level_file.sectors));
    level_file.subsectors = calloc(level_file.header.num_subsectors, sizeof(*level_file.subsectors));
    level_file.walls      = calloc(level_file.header.num_walls, sizeof(*level_file.walls));

    fread(level_file.sectors, sizeof(*level_file.sectors), level_file.header.num_sectors, file);
    fread(level_file.subsectors, sizeof(*level_file.subsectors), level_file.header.num_subsectors, file);
    fread(level_file.walls, sizeof(*level_file.walls), level_file.header.num_walls, file);

    if (!_deserializeLevelFile(level_file, o_pod)) goto _error_exit;

    fclose(file);
    return true;

_error_exit:
    fclose(file);
    free(level_file.sectors);
    free(level_file.subsectors);
    free(level_file.walls);
    return false;
}

bool _serializeLevelFile(LW_PortalWorld pod, LevelFile *level_file);
bool lw_savePortalWorld(const char *path, LW_PortalWorld pod) {
    LevelFile level_file;
    memset(&level_file, 0, sizeof(level_file));

    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;

    if (!_serializeLevelFile(pod, &level_file)) goto _error_exit;

    fwrite(&level_file.header, sizeof(level_file.header), 1, file);
    fwrite(level_file.sectors, sizeof(*level_file.sectors), level_file.header.num_sectors, file);
    fwrite(level_file.subsectors, sizeof(*level_file.subsectors), level_file.header.num_subsectors, file);
    fwrite(level_file.walls, sizeof(*level_file.walls), level_file.header.num_walls, file);

    fclose(file);
    return true;

_error_exit:
    fclose(file);
    free(level_file.sectors);
    free(level_file.subsectors);
    free(level_file.walls);
    return false;
}

bool _deserializeLevelFile(LevelFile level_file, LW_PortalWorld *pod) {

    LevelSector level_sector;
    LevelSubsector level_subsector;
    LevelWall level_wall;
    LW_Sector new_sector;

    for (unsigned i = 0; i < level_file.header.num_sectors; ++i) {
        level_sector = level_file.sectors[i];

        new_sector.num_subsectors = level_sector.num_subsectors;
        new_sector.num_walls      = level_sector.num_walls;
        new_sector.subsectors     = calloc(new_sector.num_subsectors, sizeof(*new_sector.subsectors));
        new_sector.walls          = calloc(new_sector.num_walls, sizeof(*new_sector.walls));

        // load subsectors
        for (unsigned j = 0; j < new_sector.num_subsectors; ++j) {
            level_subsector                         = level_file.subsectors[level_sector.first_subsector_offset + j];
            new_sector.subsectors[j].floor_height   = level_subsector.floor_height;
            new_sector.subsectors[j].ceiling_height = level_subsector.ceiling_height;
        }

        // load walls
        for (unsigned j = 0; j < new_sector.num_walls; ++j) {
            level_wall                        = level_file.walls[level_sector.first_wall_offset + j];
            new_sector.walls[j].start[0]      = level_wall.x;
            new_sector.walls[j].start[1]      = level_wall.y;
            new_sector.walls[j].next          = level_wall.next;
            new_sector.walls[j].portal_sector = (intptr_t)level_wall.portal_sector;
            new_sector.walls[j].portal_wall   = (intptr_t)level_wall.portal_wall;

            new_sector.walls[level_wall.next].prev = j;
        }

        LW_SectorList_push_back(&pod->sectors, new_sector);
    }

    // solve dependencies
    LW_SectorListNode *node = pod->sectors.head;
    for (; node != NULL; node = node->next) {
        for (unsigned i = 0; i < node->item.num_walls; ++i) {
            node->item.walls[i].sector = &node->item;

            if ((intptr_t)node->item.walls[i].portal_sector != 0) {
                size_t index                      = (intptr_t)node->item.walls[i].portal_sector - 1;
                node->item.walls[i].portal_sector = LW_SectorList_get(&pod->sectors, index);
                if (node->item.walls[i].portal_sector == NULL) {
                    return false;
                }

                unsigned wall_index = (intptr_t)node->item.walls[i].portal_wall - 1;
                if (wall_index >= node->item.walls[i].portal_sector->num_walls) {
                    return false;
                }

                node->item.walls[i].portal_wall = &node->item.walls[i].portal_sector->walls[wall_index];
            } else {
                node->item.walls[i].portal_sector = NULL;
                node->item.walls[i].portal_wall   = NULL;
            }

            lw_recalcLinePlane(&node->item.walls[i]);
        }
    }

    return true;
}

bool _serializeLevelFile(LW_PortalWorld pod, LevelFile *level_file) {

    level_file->header.num_sectors = pod.sectors.num_nodes;
    level_file->sectors            = calloc(level_file->header.num_sectors, sizeof(*level_file->sectors));

    // header

    level_file->header.magic[0] = MAGIC[0];
    level_file->header.magic[1] = MAGIC[1];
    level_file->header.magic[2] = MAGIC[2];
    level_file->header.magic[3] = MAGIC[3];

    level_file->header.version = SAVE_VERSION;

    ////

    LW_SectorListNode *node = pod.sectors.head;
    unsigned start_subsector;
    unsigned start_wall;
    for (unsigned i = 0; node != NULL; node = node->next, ++i) {
        // cache
        start_subsector = level_file->header.num_subsectors;
        start_wall      = level_file->header.num_walls;

        // make room
        level_file->header.num_subsectors += node->item.num_subsectors;
        level_file->header.num_walls += node->item.num_walls;

        level_file->subsectors = realloc(level_file->subsectors, level_file->header.num_subsectors * sizeof(*level_file->subsectors));
        level_file->walls      = realloc(level_file->walls, level_file->header.num_walls * sizeof(*level_file->walls));

        // fill in sector
        level_file->sectors[i].num_subsectors         = node->item.num_subsectors;
        level_file->sectors[i].num_walls              = node->item.num_walls;
        level_file->sectors[i].first_subsector_offset = start_subsector;
        level_file->sectors[i].first_wall_offset      = start_wall;

        // fill in subsectors
        for (unsigned j = 0; j < node->item.num_subsectors; ++j) {
            level_file->subsectors[start_subsector + j].floor_height   = node->item.subsectors[j].floor_height;
            level_file->subsectors[start_subsector + j].ceiling_height = node->item.subsectors[j].ceiling_height;
        }

        // fill in walls
        for (unsigned j = 0; j < node->item.num_walls; ++j) {
            LW_LineDef const *line = &node->item.walls[j];
            // easy stuff
            level_file->walls[start_wall + j].x    = line->start[0];
            level_file->walls[start_wall + j].y    = line->start[1];
            level_file->walls[start_wall + j].next = line->next;

            // portal
            if (line->portal_sector == NULL) {
                level_file->walls[start_wall + j].portal_sector = 0;
                level_file->walls[start_wall + j].portal_wall   = 0;
            } else {
                unsigned index = LW_SectorList_find_index(&pod.sectors, line->portal_sector);

                if (index >= pod.sectors.num_nodes) {
                    level_file->walls[start_wall + j].portal_wall = 0;
                    level_file->walls[start_wall + j].portal_sector = 0;
                } else {
                    level_file->walls[start_wall + j].portal_sector = index + 1;
                    level_file->walls[start_wall + j].portal_wall = (line->portal_wall - line->portal_sector->walls) / sizeof(*line->portal_wall) + 1;
                }
            }
        }
    }

    return true;
}