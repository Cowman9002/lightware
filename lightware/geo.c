#include "internal.h"

#include <math.h>

bool lw_pointInPoly(lw_vec2 *vertices, unsigned num_verts, lw_vec2 point) {
    lw_vec2 ray[2] = { { point[0], point[1] }, { -1.0f, 0.0f } };
    lw_vec2 line[2];
    float t;

    unsigned num_intersections = 0;

    // Point in polygon casts ray to left and counts number of intersections, odd inside, even out

    for (unsigned i = 0; i < num_verts; i += 2) {
        line[0][0] = vertices[i][0];
        line[0][1] = vertices[i][1];
        line[1][0] = vertices[i + 1][0];
        line[1][1] = vertices[i + 1][1];

        if (lw_intersectSegmentRay(line, ray, &t, NULL)) {
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

bool lw_pointInConvexPoly(lw_vec2 *vertices, unsigned num_vertices, lw_vec2 point) {
    lw_vec2 a, b, m, n;

    a[0] = vertices[0][0];
    a[1] = vertices[0][1];

    for (unsigned i = 1; i < num_vertices; ++i) {
        b[0] = vertices[i][0];
        b[1] = vertices[i][1];

        m[0] = b[0] - a[0];
        m[1] = b[1] - a[1];

        n[0] = point[0] - a[0];
        n[1] = point[1] - a[1];

        if (lw_cross2d(m, n) > 0.0f) return false;

        a[0] = b[0];
        a[1] = b[1];
    }

    // last line

    b[0] = vertices[0][0];
    b[1] = vertices[0][1];

    m[0] = b[0] - a[0];
    m[1] = b[1] - a[1];

    n[0] = point[0] - a[0];
    n[1] = point[1] - a[1];

    if (lw_cross2d(m, n) > 0.0f) return false;

    return true;
}

bool lw_intersectSegmentPlane(lw_vec3 line[2], lw_vec4 plane, float *o_t) {
    lw_vec3 p0 = {
        plane[0] * plane[3],
        plane[1] * plane[3],
        plane[2] * plane[3],
    };

    lw_vec3 l = {
        line[1][0] - line[0][0],
        line[1][1] - line[0][1],
        line[1][2] - line[0][2],
    };

    float denom = lw_dot3d(l, plane);
    if (denom == 0) return false;
    float s = signum(denom);

    lw_vec3 num0 = {
        p0[0] - line[0][0],
        p0[1] - line[0][1],
        p0[2] - line[0][2],
    };

    float d = lw_dot3d(num0, plane);
    d *= s;
    denom *= s;

    if (d < 0.0f || d > denom) return false;

    if (o_t != NULL) {
        *o_t = d / denom;
    }
    return true;
}

bool lw_intersectRayPlane(lw_vec3 ray[2], lw_vec4 plane, float *o_t) {
    lw_vec3 p0 = {
        plane[0] * plane[3],
        plane[1] * plane[3],
        plane[2] * plane[3],
    };

    // lw_vec3 l = {
    //     ray[1][0] - ray[0][0],
    //     ray[1][1] - ray[0][1],
    //     ray[1][2] - ray[0][2],
    // };

    lw_vec3 l = {
        ray[1][0],
        ray[1][1],
        ray[1][2],
    };

    float denom = lw_dot3d(l, plane);
    if (denom == 0) return false;
    float s = signum(denom);

    lw_vec3 num0 = {
        p0[0] - ray[0][0],
        p0[1] - ray[0][1],
        p0[2] - ray[0][2],
    };

    float d = lw_dot3d(num0, plane);
    d *= s;
    denom *= s;

    // no top bounds because it is ray
    if (d < 0.0f) return false;

    if (o_t != NULL) {
        *o_t = d / denom;
    }
    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

bool lw_intersectSegmentSegment(lw_vec2 seg0[2], lw_vec2 seg1[2], float *o_t, float *o_u) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = seg0[0][0];
    y1 = seg0[0][1];
    x2 = seg0[1][0];
    y2 = seg0[1][1];
    x3 = seg1[0][0];
    y3 = seg1[0][1];
    x4 = seg1[1][0];
    y4 = seg1[1][1];

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (denom == 0) return false;
    float s = signum(denom);

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float un = (x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2);
    tn *= s;
    un *= s;
    denom *= s;

    // t is seg0, u is seg1.
    if (tn < 0.0f || tn > denom || un < 0.0f || un > denom) return false;

    if (o_t != NULL) {
        *o_t = tn / denom;
    }

    if (o_t != NULL) {
        *o_u = un / denom;
    }

    return true;
}

bool lw_intersectSegmentLine(lw_vec2 seg[2], lw_vec2 line[2], float *o_t) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = seg[0][0];
    y1 = seg[0][1];
    x2 = seg[1][0];
    y2 = seg[1][1];
    x3 = line[0][0];
    y3 = line[0][1];
    x4 = line[1][0];
    y4 = line[1][1];

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (denom == 0) return false;
    float s = signum(denom);

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    tn *= s;
    denom *= s;

    if (tn < 0 || tn > denom) return false;

    if (o_t != NULL) {
        *o_t = tn / denom;
    }

    return true;
}

bool lw_intersectSegmentRay(lw_vec2 line[2], lw_vec2 ray[2], float *o_t, float *o_u) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = line[0][0];
    y1 = line[0][1];
    x2 = line[1][0];
    y2 = line[1][1];

    x3 = ray[0][0];
    y3 = ray[0][1];
    x4 = ray[0][0] + ray[1][0];
    y4 = ray[0][1] + ray[1][1];

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (denom == 0) return false;
    float s = signum(denom);

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float un = (x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2);

    tn *= s;
    un *= s;
    denom *= s;

    // t is segment, u is ray.
    if (tn < 0.0f || tn > denom || un < 0.0f) return false;

    if (o_t != NULL) {
        *o_t = tn / denom;
    }

    if (o_u != NULL) {
        *o_u = un / denom;
    }

    return true;
}

void lw_calcPlaneFromPoints(lw_vec3 p0, lw_vec3 p1, lw_vec3 p2, lw_vec4 o_plane) {
    lw_vec3 e0, e1;

    e0[0] = p1[0] - p0[0];
    e0[1] = p1[1] - p0[1];
    e0[2] = p1[2] - p0[2];

    e1[0] = p2[0] - p0[0];
    e1[1] = p2[1] - p0[1];
    e1[2] = p2[2] - p0[2];

    lw_cross3d(e0, e1, o_plane);
    lw_normalize3d(o_plane);

    o_plane[3] = lw_dot3d(o_plane, p0);
}

void lw_closestPointOnSegment(lw_vec2 seg[2], lw_vec2 point, lw_vec2 o_point) {
    lw_vec2 v = { seg[1][0] - seg[0][0], seg[1][1] - seg[0][1] };
    lw_vec2 u = { point[0] - seg[0][0], point[1] - seg[0][1] };

    float d = lw_dot2d(v, v);

    if (d == 0) {
        // line exists at only a single point
        o_point[0] = seg[0][0];
        o_point[1] = seg[0][1];
        return;
    }

    float t = lw_dot2d(v, u) / d;
    t       = clamp(t, 0.0f, 1.0f);

    o_point[0] = lerp(seg[0][0], seg[1][0], t);
    o_point[1] = lerp(seg[0][1], seg[1][1], t);
}