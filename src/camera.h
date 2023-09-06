#pragma once

#include "mathlib.h"

typedef struct Sector Sector;
typedef struct Camera {
    Sector *sector;
    vec3 pos;
    float yaw, pitch;

    mat4 view_mat;
    mat4 proj_mat;
    mat4 vp_mat;
    mat4 rot_mat;
}Camera;

#define FRUSTUM_NEAR_PLANE_INDEX 0
#define FRUSTUM_FAR_PLANE_INDEX 1
typedef struct Frustum {
    // First two should always be near, then far
    vec4 *planes;
    unsigned num_planes;
}Frustum;

Frustum calcFrustumFromCamera(Camera cam);
Frustum calcFrustumFromPoly(vec3 *polygon, unsigned num_verts, Camera cam);

void calcPlaneFromPoints(vec3 p0, vec3 p1, vec3 p2, vec4 o_plane);