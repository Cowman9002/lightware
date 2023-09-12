#include "editor.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

static void _input(Editor *const editor, float dt, LW_Context *const context);

int editor3dUpdate(Editor *const editor, float dt, LW_Context *const context) {
    if(lw_isKeyDown(context, LW_KeyTab)) {
        editor->view_3d = false;
        return LW_EXIT_OK;
    }

    _input(editor, dt, context);

    // lw_ivec2 mouse_screen_pos;
    // lw_getMousePos(context, mouse_screen_pos);
    // lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    return LW_EXIT_OK;
}

int editor3dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {

    lw_fillBuffer(framebuffer, editor->c_background);

    lw_renderPortalWorld(framebuffer, editor->world, editor->cam3d);

    switch (editor->state) {
        case StateIdle:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Map view");
            break;
        case StateCreateSector:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Add sector");
            break;
        case StateMovePoints:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Move points");
            break;
        case StateSelectionBox:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Box select");
            break;
        default:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "UNDEFINED STATE");
    }
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 }, editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "POS: %.2f %.2f %.2f", editor->cam3d.pos[0], editor->cam3d.pos[1], editor->cam3d.pos[2]);
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ROT: %.2f %.2f", editor->cam3d.yaw, editor->cam3d.pitch);
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 1 },
                  editor->c_font, editor->font, editor->text_buffer);

    return LW_EXIT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

static void _input(Editor *const editor, float dt, LW_Context *const context) {

    lw_vec2 movement, movement_rot;
    movement[0] = lw_isKey(context, LW_KeyD) - lw_isKey(context, LW_KeyA);
    movement[1] = lw_isKey(context, LW_KeyW) - lw_isKey(context, LW_KeyS);
    lw_normalize2d(movement);

    float z = lw_isKey(context, LW_KeySpace) - lw_isKey(context, LW_KeyLShift);
    float r = lw_isKey(context, LW_KeyLeft) - lw_isKey(context, LW_KeyRight);
    float s = lw_isKey(context, LW_KeyUp) - lw_isKey(context, LW_KeyDown);
    
    editor->cam3d.yaw += r * dt * 2.0f;
    editor->cam3d.pitch += s * dt * 1.5f;

    while(editor->cam3d.yaw > M_PI) editor->cam3d.yaw -= 2 * M_PI;
    while(editor->cam3d.yaw < -M_PI) editor->cam3d.yaw += 2 * M_PI;

    editor->cam3d.pitch = clamp(editor->cam3d.pitch, -M_PI * 0.5f, M_PI * 0.5f);

    lw_rot2d(movement, -editor->cam3d.yaw, movement_rot);

    editor->cam3d.pos[0] += movement_rot[0] * dt * 5.0f;
    editor->cam3d.pos[1] += movement_rot[1] * dt * 5.0f;
    editor->cam3d.pos[2] += z * dt * 5.0f;

    editor->cam3d.sector = lw_getSector(editor->world, editor->cam3d.pos);
    editor->cam3d.sub_sector = lw_getSubSector(editor->cam3d.sector, editor->cam3d.pos);

    lw_mat4 translation, rotation, rot_yaw, rot_pitch;

    lw_mat4RotateZ(-editor->cam3d.yaw, rot_yaw);
    lw_mat4RotateX(-editor->cam3d.pitch, rot_pitch);
    lw_mat4Mul(rot_yaw, rot_pitch, editor->cam3d.rot_mat);

    lw_mat4Translate((lw_vec3){-editor->cam3d.pos[0], -editor->cam3d.pos[1], -editor->cam3d.pos[2]}, translation);
    lw_mat4RotateZ(editor->cam3d.yaw, rot_yaw);
    lw_mat4RotateX(editor->cam3d.pitch, rot_pitch);
    lw_mat4Mul(rot_pitch, rot_yaw, rotation);

    lw_mat4Mul(rotation, translation, editor->cam3d.view_mat);
    lw_mat4Mul(editor->cam3d.proj_mat, editor->cam3d.view_mat, editor->cam3d.vp_mat);

    lw_calcCameraFrustum(&editor->cam3d);
}
