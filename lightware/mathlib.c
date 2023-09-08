#include "internal.h"

#include <math.h>

float lw_dot2d(lw_vec2 a, lw_vec2 b) {
    return a[0] * b[0] + a[1] * b[1];
}

float lw_cross2d(lw_vec2 a, lw_vec2 b) {
    return a[0] * b[1] - a[1] * b[0];
}

float lw_dist2d(lw_vec2 a, lw_vec2 b) {
    lw_vec2 diff   = { a[0] - b[0], a[1] - b[1] };
    float sqr_dist = lw_dot2d(diff, diff);
    return sqrtf(sqr_dist);
}

float lw_normalized2d(lw_vec2 a, lw_vec2 o) {
    float len = sqrtf(lw_dot2d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 2; ++i)
            o[i] = a[i] * inv_len;
    }
    return len;
}

float lw_normalize2d(lw_vec2 a) {
    float len = sqrtf(lw_dot2d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 2; ++i)
            a[i] *= inv_len;
    }
    return len;
}

void lw_rot2d(lw_vec2 a, float r, lw_vec2 o) {
    float c = cosf(r);
    float s = sinf(r);

    o[0] = c * a[0] + s * a[1];
    o[1] = -s * a[0] + c * a[1];
}

//////////////////////////////////////////////////////////////

float lw_dot3d(lw_vec3 a, lw_vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void lw_cross3d(lw_vec3 a, lw_vec3 b, lw_vec3 o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

float lw_dist3d(lw_vec3 a, lw_vec3 b) {
    lw_vec3 diff   = { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
    float sqr_dist = lw_dot3d(diff, diff);
    return sqrtf(sqr_dist);
}

float lw_normalized3d(lw_vec3 a, lw_vec3 o) {
    float len = sqrtf(lw_dot3d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 3; ++i)
            o[i] = a[i] * inv_len;
    }
    return len;
}

float lw_normalize3d(lw_vec3 a) {
    float len = sqrtf(lw_dot3d(a, a));
    if (len != 0) {
        float inv_len = 1.0f / len;
        for (unsigned i = 0; i < 3; ++i)
            a[i] *= inv_len;
    }
    return len;
}

void lw_mat4Identity(lw_mat4 out) {
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

void lw_mat4Translate(lw_vec3 t, lw_mat4 out) {
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

void lw_mat4Scale(lw_vec3 s, lw_mat4 out) {
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

void lw_mat4RotateX(float r, lw_mat4 out) {
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

void lw_mat4RotateY(float r, lw_mat4 out) {
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

void lw_mat4RotateZ(float r, lw_mat4 out) {
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

void lw_mat4Perspective(float fov, float aspect_ratio, float near, float far, lw_mat4 out) {
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

void lw_mat4Mul(lw_mat4 a, lw_mat4 b, lw_mat4 out) {
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

void lw_mat4MulVec4(lw_mat4 a, lw_vec4 b, lw_vec4 out) {
    out[0] = a[0 + 0 * 4] * b[0] + a[1 + 0 * 4] * b[1] + a[2 + 0 * 4] * b[2] + a[3 + 0 * 4] * b[3];
    out[1] = a[0 + 1 * 4] * b[0] + a[1 + 1 * 4] * b[1] + a[2 + 1 * 4] * b[2] + a[3 + 1 * 4] * b[3];
    out[2] = a[0 + 2 * 4] * b[0] + a[1 + 2 * 4] * b[1] + a[2 + 2 * 4] * b[2] + a[3 + 2 * 4] * b[3];
    out[3] = a[0 + 3 * 4] * b[0] + a[1 + 3 * 4] * b[1] + a[2 + 3 * 4] * b[2] + a[3 + 3 * 4] * b[3];
}
