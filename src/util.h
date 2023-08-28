#pragma once

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))
#define lerp(a, b, t) ((a) * (1.0 - (t)) + (b) * (t))

#define swap(T, x, y) do { T tmp = x; x = y; y = tmp; } while (0)

typedef float vec2[2];
typedef int vec2i[2];
typedef float vec3[3];
typedef float mat3[3 * 2]; // bottom layer is always 0, 0, 1

#define VEC2(x, y) ((vec2){(x), (y)})
#define VEC3(x, y) ((vec2){(x), (y)})

float dot2d(vec2 a, vec2 b);
float cross2d(vec2 a, vec2 b);

void mat3Translate(vec2 t, mat3 out);
void mat3Rotate(float r, mat3 out);
void mat3Scale(vec2 s, mat3 out);

void mat3Inv(mat3 a, mat3 out);
void mat3Mul(mat3 a, mat3 b, mat3 out);
void mat3MulVec2(mat3 a, vec2 b, vec2 out);