#include "internal.h"
#include "stdio.h"

bool lw_pointInAabb(LW_Aabb aabb, lw_vec2 point) {
    return point[0] >= aabb.low[0] &&
           point[0] <= aabb.high[0] &&
           point[1] >= aabb.low[1] &&
           point[1] <= aabb.high[1];
}

bool lw_pointInCircle(LW_Circle circle, lw_vec2 point) {
    lw_vec2 d = {circle.pos[0] - point[0], circle.pos[1] - point[1]};
    float sd = lw_dot2d(d, d);
    float sr = circle.radius * circle.radius;

    return sd <= sr;
}