#include "mathlib.h"

#include <math.h>

float dot2d(vec2 a, vec2 b) {
    return a[0] * b[0] + a[1] * b[1];
}

float cross2d(vec2 a, vec2 b) {
    return a[0] * b[1] - a[1] * b[0];
}

float cross32d(vec2 a, vec2 b, vec2 c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

float dist2d(vec2 a, vec2 b) {
    vec2 diff      = { a[0] - b[0], a[1] - b[1] };
    float sqr_dist = dot2d(diff, diff);
    return sqrtf(sqr_dist);
}

float normalized2d(vec2 a, vec2 o) {
    float len = sqrtf(dot2d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 2; ++i)
            o[i] = a[i] * inv_len;
    }
    return len;
}

float normalize2d(vec2 a) {
    float len = sqrtf(dot2d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 2; ++i)
            a[i] *= inv_len;
    }
    return len;
}


void rot2d(vec2 a, float r, vec2 o) {
    float c = cosf(r);
    float s = sinf(r);

    o[0] = c * a[0] + s * a[1];
    o[1] = -s * a[0] + c * a[1];
}

float dot3d(vec3 a, vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float dist3d(vec3 a, vec3 b) {
    vec3 diff      = { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
    float sqr_dist = dot3d(diff, diff);
    return sqrtf(sqr_dist);
}

float normalized3d(vec3 a, vec3 o) {
    float len = sqrtf(dot3d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 3; ++i)
            o[i] = a[i] * inv_len;
    }
    return len;
}

float normalize3d(vec3 a) {
    float len = sqrtf(dot3d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 3; ++i)
            a[i] *= inv_len;
    }
    return len;
}

void mat4Identity(mat4 out) {
    out[0 + 0 * 4] = 1.0f;
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = 0.0f;

    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = 1.0f;
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = 0.0f;

    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = 1.0f;
    out[3 + 2 * 4] = 0.0f;

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4Translate(vec3 t, mat4 out) {
    out[0 + 0 * 4] = 1.0f;
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = t[0];

    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = 1.0f;
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = t[1];

    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = 1.0f;
    out[3 + 2 * 4] = t[2];

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4Scale(vec3 s, mat4 out) {
    out[0 + 0 * 4] = s[0];
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = 0.0f;

    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = s[1];
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = 0.0f;

    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = s[2];
    out[3 + 2 * 4] = 0.0f;

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4RotateX(float r, mat4 out) {
    float c = cosf(r);
    float s = sinf(r);

    out[0 + 0 * 4] = 1.0f;
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = 0.0f;

    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = c;
    out[2 + 1 * 4] = s;
    out[3 + 1 * 4] = 0.0f;

    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = -s;
    out[2 + 2 * 4] = c;
    out[3 + 2 * 4] = 0.0f;

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4RotateY(float r, mat4 out) {
    float c = cosf(r);
    float s = sinf(r);

    out[0 + 0 * 4] = c;
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = -s;
    out[3 + 0 * 4] = 0.0f;

    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = 1.0f;
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = 0.0f;

    out[0 + 2 * 4] = s;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = c;
    out[3 + 2 * 4] = 0.0f;

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4RotateZ(float r, mat4 out) {
    float c = cosf(r);
    float s = sinf(r);

    out[0 + 0 * 4] = c;
    out[1 + 0 * 4] = s;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = 0.0f;

    out[0 + 1 * 4] = -s;
    out[1 + 1 * 4] = c;
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = 0.0f;

    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = 1.0f;
    out[3 + 2 * 4] = 0.0f;

    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 0.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 1.0f;
}

void mat4Perspective(float fov, float aspect_ratio, float near, float far, mat4 out) {
    float inv_aspect       = 1.0f / aspect_ratio;
    float inv_tan_half_fov = 1.0f / tanf(fov * 0.5f);

    // x
    out[0 + 0 * 4] = inv_aspect * inv_tan_half_fov;
    out[1 + 0 * 4] = 0.0f;
    out[2 + 0 * 4] = 0.0f;
    out[3 + 0 * 4] = 0.0f;

    // y
    out[0 + 1 * 4] = 0.0f;
    out[1 + 1 * 4] = -(far + near) / (far - near);
    out[2 + 1 * 4] = 0.0f;
    out[3 + 1 * 4] = -(2 * far * near) / (far - near);

    // z
    out[0 + 2 * 4] = 0.0f;
    out[1 + 2 * 4] = 0.0f;
    out[2 + 2 * 4] = inv_tan_half_fov;
    out[3 + 2 * 4] = 0.0f;

    // w
    out[0 + 3 * 4] = 0.0f;
    out[1 + 3 * 4] = 1.0f;
    out[2 + 3 * 4] = 0.0f;
    out[3 + 3 * 4] = 0.0f;
}

void mat4Mul(mat4 a, mat4 b, mat4 out) {
    // c[ji] = a[0i] * b[j0] + a[1i] * b[j1] + a[2i] * b[j2]
    for (unsigned c = 0; c < 4; c++) {
        for (unsigned r = 0; r < 4; r++) {
            out[c + r * 4] = a[0 + r * 4] * b[c + 0 * 4] +
                             a[1 + r * 4] * b[c + 1 * 4] +
                             a[2 + r * 4] * b[c + 2 * 4] +
                             a[3 + r * 4] * b[c + 3 * 4];
        }
    }
}

float mat4MulVec3(mat4 a, vec3 b, vec3 out) {
    out[0] = a[0 + 0 * 4] * b[0] + a[1 + 0 * 4] * b[1] + a[2 + 0 * 4] * b[2] + a[3 + 0 * 4];
    out[1] = a[0 + 1 * 4] * b[0] + a[1 + 1 * 4] * b[1] + a[2 + 1 * 4] * b[2] + a[3 + 1 * 4];
    out[2] = a[0 + 2 * 4] * b[0] + a[1 + 2 * 4] * b[1] + a[2 + 2 * 4] * b[2] + a[3 + 2 * 4];
    return a[0 + 3 * 4] * b[0] + a[1 + 3 * 4] * b[1] + a[2 + 3 * 4] * b[2] + a[3 + 3 * 4];
}
