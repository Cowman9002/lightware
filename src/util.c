#include "util.h"

#include "math.h"

float dot2d(vec2 a, vec2 b) {
    return a[0] * b[0] + a[1] * b[1];
}

float cross2d(vec2 a, vec2 b) {
    return a[0] * b[1] - a[1] * b[0];
}

float dist2d(vec2 a, vec2 b) {
    vec2 diff      = { a[0] - b[0], a[1] - b[1] };
    float sqr_dist = dot2d(diff, diff);
    return sqrtf(sqr_dist);
}

float dot3d(vec3 a, vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float dist3d(vec3 a, vec3 b) {
    vec3 diff      = { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
    float sqr_dist = dot3d(diff, diff);
    return sqrtf(sqr_dist);
}

float normalize3d(vec3 a) {
    float len = sqrtf(dot3d(a, a));
    if (len != 0) {
        for (unsigned i = 0; i < 3; ++i)
            a[i] /= len;
    }
    return len;
}


void mat3Translate(vec2 t, mat3 out) {
    out[0 + 0 * 3] = 1.0f;
    out[1 + 0 * 3] = 0.0f;
    out[2 + 0 * 3] = t[0];
    out[0 + 1 * 3] = 0.0f;
    out[1 + 1 * 3] = 1.0f;
    out[2 + 1 * 3] = t[1];
}

void mat3Rotate(float r, mat3 out) {
    float c        = cosf(r);
    float s        = sinf(r);
    out[0 + 0 * 3] = c;
    out[1 + 0 * 3] = -s;
    out[2 + 0 * 3] = 0.0f;
    out[0 + 1 * 3] = s;
    out[1 + 1 * 3] = c;
    out[2 + 1 * 3] = 0.0f;
}

void mat3Scale(vec2 s, mat3 out) {
    out[0 + 0 * 3] = s[0];
    out[1 + 0 * 3] = 0.0f;
    out[2 + 0 * 3] = 0.0f;
    out[0 + 1 * 3] = 0.0f;
    out[1 + 1 * 3] = s[1];
    out[2 + 1 * 3] = 0.0f;
}

void mat3Inv(mat3 in, mat3 out) {
    float a = in[0 + 0 * 3];
    float b = in[1 + 0 * 3];
    float c = in[2 + 0 * 3];
    float d = in[0 + 1 * 3];
    float e = in[1 + 1 * 3];
    float f = in[2 + 1 * 3];
    float g = 0.0f;
    float h = 0.0f;
    float i = 1.0f;

    float det     = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    float inv_det = 1.0f / det;

    out[0 + 0 * 3] = (e * i - f * h) * inv_det;
    out[1 + 0 * 3] = -(b * i - c * h) * inv_det;
    out[2 + 0 * 3] = (b * f - c * e) * inv_det;
    out[0 + 1 * 3] = -(d * i - f * g) * inv_det;
    out[1 + 1 * 3] = (a * i - c * g) * inv_det;
    out[2 + 1 * 3] = -(a * f - c * d) * inv_det;
}

void mat3Mul(mat3 a, mat3 b, mat3 out) {
    // c[ji] = a[0i] * b[j0] + a[1i] * b[j1] + a[2i] * b[j2]
    out[0 + 0 * 3] = a[0 + 0 * 3] * b[0 + 0 * 3] + a[1 + 0 * 3] * b[0 + 1 * 3];
    out[1 + 0 * 3] = a[0 + 0 * 3] * b[1 + 0 * 3] + a[1 + 0 * 3] * b[1 + 1 * 3];
    out[2 + 0 * 3] = a[0 + 0 * 3] * b[2 + 0 * 3] + a[1 + 0 * 3] * b[2 + 1 * 3] + a[2 + 0 * 3];

    out[0 + 1 * 3] = a[0 + 1 * 3] * b[0 + 0 * 3] + a[1 + 1 * 3] * b[0 + 1 * 3];
    out[1 + 1 * 3] = a[0 + 1 * 3] * b[1 + 0 * 3] + a[1 + 1 * 3] * b[1 + 1 * 3];
    out[2 + 1 * 3] = a[0 + 1 * 3] * b[2 + 0 * 3] + a[1 + 1 * 3] * b[2 + 1 * 3] + a[2 + 1 * 3];
}

void mat3MulVec2(mat3 a, vec2 b, vec2 out) {
    out[0] = a[0 + 0 * 3] * b[0] + a[1 + 0 * 3] * b[1] + a[2 + 0 * 3];
    out[1] = a[0 + 1 * 3] * b[0] + a[1 + 1 * 3] * b[1] + a[2 + 1 * 3];
}
