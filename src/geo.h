#pragma once

#include "mathlib.h"
#include <stdbool.h>

typedef struct Line {
    vec2 points[2];
}Line;

bool pointInPoly(Line *lines, unsigned num_lines, vec2 point);
bool pointInConvexPoly(vec2 *vertices, unsigned num_vertices, vec2 point);

bool intersectSegmentPlane(vec3 line[2], vec4 plane, float *o_t);

bool intersectSegmentSegment(vec2 line0[2], vec2 line1[2], float *o_t);
bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], float *o_t);
bool intersectSegmentRay(vec2 line[2], vec2 ray[2], float *o_t);