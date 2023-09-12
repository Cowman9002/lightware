#include "internal.h"

#include <stdio.h>
#include <malloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAGIC "LWBB"

#define LOAD_MIN_SUPPORTED_VERSION 1
#define LOAD_MAX_SUPPORTED_VERSION 1

#define SAVE_VERSION 1

typedef struct LevelHeader {
    uint8_t magic[4];
    uint32_t version;
    uint32_t num_sectors;
    uint32_t sector_table_start;
    uint32_t subsector_table_start;
    uint32_t wall_table_start;
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

    unsigned num_sectors, num_subsectors, num_walls;
} LevelFile;

bool lw_loadPortalWorld(const char *path, LW_PortalWorld *o_pod) {
    return false;
}

bool _genLevelFile(LW_PortalWorld pod, LevelFile *level_file);
bool lw_savePortalWorld(const char *path, LW_PortalWorld pod) {
    LevelFile level_file;
    memset(&level_file, 0, sizeof(level_file));

    FILE *file = fopen(path, "wb");
    if (file == NULL) return false;

    if (!_genLevelFile(pod, &level_file)) goto _error_exit;

    fwrite(&level_file.header, sizeof(level_file.header), 1, file);
    fwrite(level_file.sectors, sizeof(*level_file.sectors), level_file.num_sectors, file);
    fwrite(level_file.subsectors, sizeof(*level_file.subsectors), level_file.num_subsectors, file);
    fwrite(level_file.walls, sizeof(*level_file.walls), level_file.num_walls, file);

    fclose(file);
    return true;

_error_exit:
    fclose(file);
    free(level_file.sectors);
    free(level_file.subsectors);
    free(level_file.walls);
    return false;
}

bool _genLevelFile(LW_PortalWorld pod, LevelFile *level_file) {

    level_file->num_sectors = pod.sectors.num_nodes;
    level_file->sectors     = calloc(level_file->num_sectors, sizeof(*level_file->sectors));

    // header

    level_file->header.magic[0] = MAGIC[0];
    level_file->header.magic[1] = MAGIC[1];
    level_file->header.magic[2] = MAGIC[2];
    level_file->header.magic[3] = MAGIC[3];

    level_file->header.version            = SAVE_VERSION;
    level_file->header.num_sectors        = level_file->num_sectors;
    level_file->header.sector_table_start = sizeof(level_file->header);

    ////

    LW_SectorListNode *node = pod.sectors.head;
    unsigned start_subsector;
    unsigned start_wall;
    for (unsigned i = 0; node != NULL; node = node->next, ++i) {
        // cache
        start_subsector = level_file->num_subsectors;
        start_wall      = level_file->num_walls;

        // make room
        level_file->num_subsectors += node->item.num_subsectors;
        level_file->num_walls += node->item.num_walls;

        level_file->subsectors = realloc(level_file->subsectors, level_file->num_subsectors * sizeof(*level_file->subsectors));
        level_file->walls      = realloc(level_file->walls, level_file->num_walls * sizeof(*level_file->walls));

        // fill in sector
        level_file->sectors[i].num_subsectors         = node->item.num_subsectors;
        level_file->sectors[i].num_walls              = node->item.num_walls;
        level_file->sectors[i].first_subsector_offset = start_subsector;
        level_file->sectors[i].first_wall_offset      = start_wall;

        // fill in subsectors
        for(unsigned j = 0; j < node->item.num_subsectors; ++j) {
            level_file->subsectors[start_subsector + j].floor_height = node->item.subsectors[j].floor_height;
            level_file->subsectors[start_subsector + j].ceiling_height = node->item.subsectors[j].ceiling_height;
        }

        // fill in walls
        for(unsigned j = 0; j < node->item.num_walls; ++j) {
            LW_LineDef const * line = &node->item.walls[j];
            // easy stuff
            level_file->walls[start_wall + j].x = line->start[0];
            level_file->walls[start_wall + j].y = line->start[1];
            level_file->walls[start_wall + j].next = line->next;

            // portal
            if(line->portal_sector == NULL) {
                level_file->walls[start_wall + j].portal_sector = ~0;
                level_file->walls[start_wall + j].portal_wall = ~0;
            } else {
                level_file->walls[start_wall + j].portal_sector = LW_SectorList_find_index(&pod.sectors, line->portal_sector);
                if(level_file->walls[start_wall + j].portal_sector == pod.sectors.num_nodes) {
                    level_file->walls[start_wall + j].portal_wall = ~0;
                } else {
                    level_file->walls[start_wall + j].portal_wall = (line->portal_wall - line->portal_sector->walls) / sizeof(*line->portal_wall);
                }
            }
        }
   
    }

    return true;
}