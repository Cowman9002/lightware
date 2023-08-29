#include "geo.h"
#include <stdlib.h>

bool pointInPoly(Line *lines, unsigned num_lines, vec2 point) {
    vec2 ray[2] = { { point[0], point[1] }, { point[0] - 1.0f, point[1] } };
    vec2 line[2];
    float t;

    unsigned num_intersections = 0;

    // Point in polygon casts ray to left and counts number of intersections, odd inside, even out

    for (unsigned i = 0; i < num_lines; ++i) {
        line[0][0] = lines[i].points[0][0];
        line[0][1] = lines[i].points[0][1];
        line[1][0] = lines[i].points[1][0];
        line[1][1] = lines[i].points[1][1];

        if (intersectSegmentRay(line, ray, &t)) {
            // If hitting a vertex exactly, only count if other vertex is above the ray
            if (t != 0.0f && t != 1.0f) {
                ++num_intersections;
            } else if (t == 1.0f && line[0][1] > ray[0][1]) {
                ++num_intersections;
            } else if (line[1][1] > ray[0][1]) {
                ++num_intersections;
            }
        }
    }

    return num_intersections % 2 == 1;
}


bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], float *o_t) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = line0[0][0];
    y1 = line0[0][1];
    x2 = line0[1][0];
    y2 = line0[1][1];
    x3 = line1[0][0];
    y3 = line1[0][1];
    x4 = line1[1][0];
    y4 = line1[1][1];

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float td = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

    if (td == 0) return false;

    float t = tn / td;

    if (t < 0 || t > 1) return false;

    if (o_t != NULL) {
        // TODO Remove division when o_t is not needed
        *o_t = t;
    }

    return true;
}

bool intersectSegmentRay(vec2 line[2], vec2 ray[2], float *o_t) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = line[0][0];
    y1 = line[0][1];
    x2 = line[1][0];
    y2 = line[1][1];
    x3 = ray[0][0];
    y3 = ray[0][1];
    x4 = ray[1][0];
    y4 = ray[1][1];

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (denom == 0) return false;

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float un = (x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2);

    float t = tn / denom;
    float u = un / denom;

    // t is segment, u is ray.
    if (t < 0.0f || t > 1.0f || u < 0.0f) return false;

    if (o_t != NULL) {
        // TODO Remove division when o_t is not needed
        *o_t = t;
    }

    return true;
}