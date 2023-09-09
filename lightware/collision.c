#include "internal.h"
#include "stdio.h"

bool lw_pointInAabb(LW_Aabb aabb, lw_vec2 point) {
    return point[0] >= aabb.low[0] &&
           point[0] <= aabb.high[0] &&
           point[1] >= aabb.low[1] &&
           point[1] <= aabb.high[1];
}