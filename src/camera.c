#include "camera.h"

#include "draw.h"
#include <math.h>
#include <assert.h>
#include <malloc.h>

Frustum calcFrustumFromCamera(Camera cam) {
    Frustum frustum;

    frustum.num_planes = 6;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));

    const float half_v = FAR_PLANE * tanf(FOV * .5f);
    const float half_h = half_v * ASPECT_RATIO;
    vec3 cam_front, cam_right, cam_up, cam_front_far;
    mat4MulVec3(cam.rot_mat, (vec3){ 1.0f, 0.0f, 0.0f }, cam_right);
    mat4MulVec3(cam.rot_mat, (vec3){ 0.0f, 1.0f, 0.0f }, cam_front);
    mat4MulVec3(cam.rot_mat, (vec3){ 0.0f, 0.0f, 1.0f }, cam_up);
    for (unsigned _x = 0; _x < 3; ++_x)
        cam_front_far[_x] = cam_front[_x] * FAR_PLANE;

    vec3 tmp_vec[2];

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_right[_x] * half_h;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
    cross3d(tmp_vec[1], cam_up, frustum.planes[2]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_right[_x] * half_h;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
    cross3d(cam_up, tmp_vec[1], frustum.planes[3]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_up[_x] * half_v;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
    cross3d(cam_right, tmp_vec[1], frustum.planes[4]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_up[_x] * half_v;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
    cross3d(tmp_vec[1], cam_right, frustum.planes[5]);

    for (unsigned _x = 0; _x < 3; ++_x)
        frustum.planes[FRUSTUM_NEAR_PLANE_INDEX][_x] = cam_front[_x];
    for (unsigned _x = 0; _x < 3; ++_x)
        frustum.planes[FRUSTUM_FAR_PLANE_INDEX][_x] = -cam_front[_x];

    for (unsigned i = 0; i < 6; ++i) {
        normalize3d(frustum.planes[i]);
        frustum.planes[i][3] = dot3d(frustum.planes[i], cam.pos);
    }

    frustum.planes[FRUSTUM_NEAR_PLANE_INDEX][3] += NEAR_PLANE;
    frustum.planes[FRUSTUM_FAR_PLANE_INDEX][3] += -FAR_PLANE;

    return frustum;
}

Frustum calcFrustumFromPoly(vec3 *polygon, unsigned num_verts, Camera cam) {
    assert(num_verts >= 3);
    Frustum frustum;

    frustum.num_planes = num_verts + 2;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));

    for (unsigned i = 0; i < num_verts; ++i) {
        unsigned j = (i + 1) % num_verts;

        calcPlaneFromPoints(cam.pos, polygon[j], polygon[i], frustum.planes[i + 2]);
    }

    // near plane
    calcPlaneFromPoints(polygon[0], polygon[2], polygon[1], frustum.planes[FRUSTUM_NEAR_PLANE_INDEX]);

    // far plane
    vec3 cam_front;
    mat4MulVec3(cam.rot_mat, (vec3){ 0.0f, 1.0f, 0.0f }, cam_front);

    for (unsigned _x = 0; _x < 3; ++_x)
        frustum.planes[FRUSTUM_FAR_PLANE_INDEX][_x] = -cam_front[_x];

    normalize3d(frustum.planes[FRUSTUM_FAR_PLANE_INDEX]);
    frustum.planes[FRUSTUM_FAR_PLANE_INDEX][3] = dot3d(frustum.planes[FRUSTUM_FAR_PLANE_INDEX], cam.pos) - FAR_PLANE;

    return frustum;
}

void calcPlaneFromPoints(vec3 p0, vec3 p1, vec3 p2, vec4 o_plane) {
    vec3 e0, e1;

    for (unsigned _x = 0; _x < 3; ++_x)
        e0[_x] = p1[_x] - p0[_x];
    for (unsigned _x = 0; _x < 3; ++_x)
        e1[_x] = p2[_x] - p0[_x];

    cross3d(e0, e1, o_plane);
    normalize3d(o_plane);

    float d    = dot3d(o_plane, p0);
    o_plane[3] = d;
}