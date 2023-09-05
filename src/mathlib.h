#pragma once

#include <math.h>

#define TO_RADS (M_PI / 180.0)
#define TO_DEGS (180.0 / M_PI)

#define signum(a) (((a) > 0) - ((a) < 0))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))
#define lerp(a, b, t) ((a) * (1.0 - (t)) + (b) * (t))

#define swap(T, x, y) do { T tmp = x; x = y; y = tmp; } while (0)

typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];
typedef float mat4[16];

float dot2d(vec2 a, vec2 b);
float cross2d(vec2 a, vec2 b);
float cross32d(vec2 a, vec2 b, vec2 c);
float dist2d(vec2 a, vec2 b);
float normalized2d(vec2 a, vec2 o);
float normalize2d(vec2 a);
void rot2d(vec2 a, float r, vec2 o);

float dot3d(vec3 a, vec3 b);
void cross3d(vec3 a, vec3 b, vec3 o);
float dist3d(vec3 a, vec3 b);
float normalized3d(vec3 a, vec3 o);
float normalize3d(vec3 a);

void mat4Identity(mat4 out);
void mat4Translate(vec3 t, mat4 out);
void mat4Scale(vec3 s, mat4 out);
void mat4RotateX(float r, mat4 out);
void mat4RotateY(float r, mat4 out);
void mat4RotateZ(float r, mat4 out);

void mat4Perspective(float fov, float aspect_ratio, float near, float far, mat4 out);

void mat4Mul(mat4 a, mat4 b, mat4 out);

// returns w component of vec4
float mat4MulVec3(mat4 a, vec3 b, vec3 out);