#pragma once

#include <stdint.h>
#include <stdbool.h>

#if defined(LIGHTWARE_EXPORT)
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllexport)
#elif defined(__ELF__)
#define LIGHTWARE_API __attribute__((visibility("default")))
#else
#define LIGHTWARE_API
#endif
#else
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllimport)
#else
#define LIGHTWARE_API
#endif
#endif

//
// CONTEXT
//

typedef struct LW_Context LW_Context;
typedef struct LW_Framebuffer LW_Framebuffer;

typedef int (*LW_UpdateFn)(LW_Context *const context, float delta);
typedef int (*LW_RenderFn)(LW_Context *const context, LW_Framebuffer *const main_frame_buffer);

typedef struct LW_ContextInit {
    const char *title;
    unsigned logical_width, logical_height;
    unsigned scale;

    void *user_data;
    LW_UpdateFn update_fn;
    LW_RenderFn render_fn;
} LW_ContextInit;

LIGHTWARE_API LW_Context *lw_init(LW_ContextInit init);
LIGHTWARE_API void lw_deinit(LW_Context *const context);

LIGHTWARE_API int lw_start(LW_Context *const context);

LIGHTWARE_API void *lw_getUserData(LW_Context *const context);

//
//  MATH
//

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TO_RADS (M_PI / 180.0)
#define TO_DEGS (180.0 / M_PI)

#define signum(a) (((a) > 0) - ((a) < 0))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))
#define lerp(a, b, t) ((a) * (1.0 - (t)) + (b) * (t))

#define swap(T, x, y) \
    do {              \
        T tmp = x;    \
        x     = y;    \
        y     = tmp;  \
    } while (0)

typedef float lw_vec2[2];
typedef float lw_vec3[3];
typedef float lw_vec4[4];
typedef float lw_mat4[16];

typedef int lw_ivec2[2];
typedef unsigned lw_uvec2[2];

LIGHTWARE_API float lw_dot2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_cross2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_dist2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_normalized2d(lw_vec2 a, lw_vec2 o);
LIGHTWARE_API float lw_normalize2d(lw_vec2 a);
LIGHTWARE_API void lw_rot2d(lw_vec2 a, float r, lw_vec2 o);

LIGHTWARE_API float lw_dot3d(lw_vec3 a, lw_vec3 b);
LIGHTWARE_API void lw_cross3d(lw_vec3 a, lw_vec3 b, lw_vec3 o);
LIGHTWARE_API float lw_dist3d(lw_vec3 a, lw_vec3 b);
LIGHTWARE_API float lw_normalized3d(lw_vec3 a, lw_vec3 o);
LIGHTWARE_API float lw_normalize3d(lw_vec3 a);

LIGHTWARE_API void lw_mat4Identity(lw_mat4 out);
LIGHTWARE_API void lw_mat4Translate(lw_vec3 t, lw_mat4 out);
LIGHTWARE_API void lw_mat4Scale(lw_vec3 s, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateX(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateY(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateZ(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4Perspective(float fov, float aspect_ratio, float near, float far, lw_mat4 out);
LIGHTWARE_API void lw_mat4Mul(lw_mat4 a, lw_mat4 b, lw_mat4 out);
LIGHTWARE_API void lw_mat4MulVec4(lw_mat4 a, lw_vec4 b, lw_vec4 out);

//
//  GEOMETRY
//

/// @brief Returns true if point is inside of polygon. Polygon can be concave and have holes, but should be closed for predictable results.
/// @param vertices array of points stored {line_start0, line_end0, line_start1, line_end1, ...}
/// @param num_verts number of vertices in the vertices array
/// @param point point to test
/// @return true if point is inside of the polygon
LIGHTWARE_API bool lw_pointInPoly(lw_vec2 *vertices, unsigned num_verts, lw_vec2 point);

/// @brief Returns true if point is inside of closed, convex polygon. Unlike pointInPoly, the polygon must be convex and cannot contain holes for predictable results.
/// @param vertices array of points stored and connected one after the other, wrapping from the last to the beginning
/// @param num_vertices  number of vertices in vertices array
/// @param point point to test
/// @return true if point is inside of the polygon
LIGHTWARE_API bool lw_pointInConvexPoly(lw_vec2 *vertices, unsigned num_vertices, lw_vec2 point);

LIGHTWARE_API bool lw_intersectSegmentPlane(lw_vec3 line[2], lw_vec4 plane, float *o_t);

LIGHTWARE_API bool lw_intersectSegmentSegment(lw_vec2 seg0[2], lw_vec2 seg1[2], float *o_t);
LIGHTWARE_API bool lw_intersectSegmentLine(lw_vec2 seg[2], lw_vec2 line[2], float *o_t);
LIGHTWARE_API bool lw_intersectSegmentRay(lw_vec2 line[2], lw_vec2 ray[2], float *o_t);

LIGHTWARE_API void lw_calcPlaneFromPoints(lw_vec3 p0, lw_vec3 p1, lw_vec3 p2, lw_vec4 o_plane);

//
//  DRAWING
//

typedef struct LW_Color {
    uint8_t a, b, g, r;
} LW_Color;

typedef struct LW_Rect {
    lw_ivec2 pos, size;
} LW_Rect;

#define RGBA(r, g, b, a) ((Color){ (a), (b), (g), (r) })
#define RGB(r, g, b) RGBA(r, g, b, 255)
#define RGBV(v) RGB(v, v, v)

#define LW_COLOR_WHITE ((LW_Color){ .r = 255, .g = 255, .b = 255, .a = 255 })
#define LW_COLOR_BLACK ((LW_Color){ .r = 0, .g = 0, .b = 0, .a = 255 })
#define LW_COLOR_RED ((LW_Color){ .r = 255, .g = 0, .b = 0, .a = 255 })
#define LW_COLOR_GREEN ((LW_Color){ .r = 0, .g = 255, .b = 0, .a = 255 })
#define LW_COLOR_BLUE ((LW_Color){ .r = 0, .g = 0, .b = 255, .a = 255 })
#define LW_COLOR_YELLOW ((LW_Color){ .r = 255, .g = 255, .b = 0, .a = 255 })
#define LW_COLOR_CYAN ((LW_Color){ .r = 0, .g = 255, .b = 255, .a = 255 })
#define LW_COLOR_PURPLE ((LW_Color){ .r = 255, .g = 0, .b = 255, .a = 255 })

LIGHTWARE_API LW_Color lw_lerpColor(LW_Color c0, LW_Color c1, uint8_t t);
LIGHTWARE_API LW_Color lw_mulColor(LW_Color c0, LW_Color c1);
LIGHTWARE_API LW_Color lw_shadeColor(LW_Color c0, uint8_t m);

LIGHTWARE_API void lw_getFramebufferDimentions(LW_Framebuffer *const frame_buffer, lw_ivec2 o_dims);

LIGHTWARE_API void lw_setPixel(LW_Framebuffer *const framebuffer, lw_uvec2 pos, LW_Color color);
LIGHTWARE_API void lw_fillBuffer(LW_Framebuffer *const framebuffer, LW_Color color);
LIGHTWARE_API void lw_fillRect(LW_Framebuffer *const framebuffer, LW_Rect rect, LW_Color color);
LIGHTWARE_API void lw_drawRect(LW_Framebuffer *const framebuffer, LW_Rect rect, LW_Color color);
LIGHTWARE_API void lw_drawLine(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color);
LIGHTWARE_API void lw_drawPoly(LW_Framebuffer *const framebuffer, lw_ivec2 *vertices, unsigned num_vertices, LW_Color color);

//
//  PORTAL WORLD
//

typedef struct LW_Subsector {
    float floor_height, ceiling_height;
} LW_Subsector;

typedef struct LW_Sector {
    unsigned num_walls;
    lw_vec2 *points;                 // list
    lw_vec4 *planes;                 // list
    struct LW_Sector **next_sectors; // list

    // definitions of sub sectors for sector over sector
    unsigned num_sub_sectors;
    LW_Subsector *sub_sectors; // list
} LW_Sector;

#define LIST_TAG LW_SectorList
#define LIST_ITEM_TYPE LW_Sector
#include "list_header.h"

typedef struct LW_PortalWorld {
    LW_SectorList sectors;
} LW_PortalWorld;

typedef struct LW_Frustum {
    // First should always be near plane
    lw_vec4 *planes;
    unsigned num_planes;
} LW_Frustum;

typedef struct LW_Camera {
    LW_Sector *sector;
    unsigned sub_sector;

    lw_vec3 pos;
    float yaw, pitch;

    float fov, aspect_ratio, far_plane, near_plane;

    lw_mat4 view_mat;
    lw_mat4 proj_mat;
    lw_mat4 vp_mat;
    lw_mat4 rot_mat;
    LW_Frustum view_frustum;
} LW_Camera;

/// @brief Assumes cam->view_frustum already has a preallocated buffer for at least 6 planes
/// @param cam
/// @return
LIGHTWARE_API void lw_calcCameraFrustum(LW_Camera *const cam);
LIGHTWARE_API LW_Frustum lw_calcFrustumFromPoly(lw_vec3 *polygon, unsigned num_verts, lw_vec3 view_point);

LIGHTWARE_API bool lw_loadPortalWorld(const char *path, float scale, LW_PortalWorld *o_pod);

LIGHTWARE_API void lw_freeSubsector(LW_Subsector subsector);
LIGHTWARE_API void lw_freeSector(LW_Sector sector);
LIGHTWARE_API void lw_freePortalWorld(LW_PortalWorld pod);

LIGHTWARE_API bool lw_pointInSector(LW_Sector sector, lw_vec2 point);
LIGHTWARE_API LW_Sector *lw_getSector(LW_PortalWorld pod, lw_vec2 point);
LIGHTWARE_API unsigned lw_getSubSector(LW_Sector *sector, lw_vec3 point);

LIGHTWARE_API void lw_renderPortalWorld(LW_PortalWorld pod, LW_Camera camera);