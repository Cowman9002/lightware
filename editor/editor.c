#include "editor.h"

#include <math.h>
#include <malloc.h>

static InputAction s_actions[InputName_size];

float logerp(float a, float b, float t) {
    return b * powf(a / b, t);
}

// solve for t based on desired r
float inv_logerp(float a, float b, float r) {
    return logf(r / b) / log(a / b);
}

bool editorInit(Editor *const editor) {
    if (!lw_loadTexture("res/fonts/small.png", &editor->font.texture)) return false;
    editor->font.char_width = editor->font.texture.width / 95;
    editor->text_buffer     = calloc(1, TEXT_BUFFER_SIZE);

    editor->open_file          = calloc(1, FILE_NAME_BUFFER_SIZE);
    editor->open_file_relative = calloc(1, FILE_NAME_BUFFER_SIZE);
    editor->project_directory  = calloc(1, FILE_NAME_BUFFER_SIZE);

    editor->c_background = RGB(10, 10, 40);
    editor->c_grid       = RGB(60, 80, 120);
    editor->c_font       = RGB(240, 240, 210);

    editor->c_walls    = RGB(200, 200, 200);
    editor->c_vertices = RGB(30, 140, 240);
    editor->c_portal   = RGB(200, 40, 80);

    editor->c_new_walls    = RGB(128, 128, 128);
    editor->c_new_vertices = RGB(80, 30, 240);
    editor->c_start_vertex = RGB(250, 190, 100);

    editor->c_sel_vertex           = RGB(250, 230, 80);
    editor->c_selection_box        = RGB(245, 245, 125);
    editor->c_highlighted_vertices = RGB(240, 212, 30);

    editor->data2d.grid_active          = true;
    editor->data2d.grid_size            = 8.0f;
    editor->data2d.mouse_snapped_pos[3] = 1.0f;

    editor->data2d.specter_select = true;

    editor->data2d.zoom   = 0.08;
    editor->data2d.zoom_t = inv_logerp(MIN_ZOOM, MAX_ZOOM, editor->data2d.zoom);

    editor->data3d.camera.view_frustum.num_planes = 6;
    editor->data3d.camera.view_frustum.planes     = malloc(editor->data3d.camera.view_frustum.num_planes * sizeof(*editor->data3d.camera.view_frustum.planes));
    editor->data3d.floor_snap_val                 = 1.0f;

    editor->data3d.camera.aspect_ratio = (float)editor->width / (float)editor->height;
    editor->data3d.camera.far_plane    = 500.0f;
    editor->data3d.camera.fov          = 80 * TO_RADS;
    editor->data3d.camera.near_plane   = 0.3;
    lw_calcCameraProjection(&editor->data3d.camera);

    LW_SectorList_init(&editor->world.sectors);

    setInputAction(InputName_swapViews, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyTab, .disallowed_modifiers = 0, .required_modifiers = 0 });

    setInputAction(InputName_moveForwards, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyW, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_moveBackwards, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyS, .disallowed_modifiers = MODIFIER_CTRL, .required_modifiers = 0 });
    setInputAction(InputName_moveLeft, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyA, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_moveRight, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyD, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_rotateLeft, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyLeft, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_rotateRight, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyRight, .disallowed_modifiers = 0, .required_modifiers = 0 });

    setInputAction(InputName_toggleGrid, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyG, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_increaseGrid, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyRightBracket, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_decreaseGrid, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyLeftBracket, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_specterSelect, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyP, .disallowed_modifiers = 0, .required_modifiers = 0 });

    setInputAction(InputName_cancel, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyEscape, .disallowed_modifiers = MODIFIER_SHIFT, .required_modifiers = 0 });

    setInputAction(InputName_selectPoint, (InputAction){ .type = InputTypeButton, .major.button = 0, .disallowed_modifiers = MODIFIER_SHIFT | MODIFIER_CTRL, .required_modifiers = 0 });
    setInputAction(InputName_multiSelect, (InputAction){ .type = InputTypeButton, .major.button = 0, .disallowed_modifiers = MODIFIER_CTRL, .required_modifiers = MODIFIER_SHIFT });
    setInputAction(InputName_selectSector, (InputAction){ .type = InputTypeButton, .major.button = 0, .disallowed_modifiers = MODIFIER_SHIFT, .required_modifiers = MODIFIER_CTRL });
    setInputAction(InputName_multiSelectSector, (InputAction){ .type = InputTypeButton, .major.button = 0, .disallowed_modifiers = 0, .required_modifiers = MODIFIER_CTRL | MODIFIER_SHIFT });

    setInputAction(InputName_newSector, (InputAction){ .type = InputTypeKey, .major.key = LW_KeySpace, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_deletePoints, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyX, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_splitLine, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyC, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_autoPortal, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyV, .disallowed_modifiers = 0, .required_modifiers = 0 });
    setInputAction(InputName_selectionBox, (InputAction){ .type = InputTypeKey, .major.key = LW_KeyB, .disallowed_modifiers = 0, .required_modifiers = 0 });

    return true;
}

void setInputAction(InputName action, InputAction val) {
    s_actions[action] = val;
}

bool _modifierTest(LW_Context *const context, InputName action) {
    bool shift = lw_isKey(context, LW_KeyLShift) || lw_isKey(context, LW_KeyRShift);
    bool ctrl  = lw_isKey(context, LW_KeyLCtrl) || lw_isKey(context, LW_KeyRCtrl);
    bool alt   = lw_isKey(context, LW_KeyLAlt) || lw_isKey(context, LW_KeyRAlt);

    if (s_actions[action].disallowed_modifiers & MODIFIER_SHIFT && shift) return false;
    if (s_actions[action].disallowed_modifiers & MODIFIER_CTRL && ctrl) return false;
    if (s_actions[action].disallowed_modifiers & MODIFIER_ALT && alt) return false;

    if (s_actions[action].required_modifiers & MODIFIER_SHIFT && !shift) return false;
    if (s_actions[action].required_modifiers & MODIFIER_CTRL && !ctrl) return false;
    if (s_actions[action].required_modifiers & MODIFIER_ALT && !alt) return false;

    return true;
}

bool isInputAction(LW_Context *const context, InputName action) {
    if (!_modifierTest(context, action)) return false;

    switch (s_actions[action].type) {
        case InputTypeKey: return lw_isKey(context, s_actions[action].major.key);
        case InputTypeButton: return lw_isMouseButton(context, s_actions[action].major.button);
    }
    return false;
}

bool isInputActionDown(LW_Context *const context, InputName action) {
    if (!_modifierTest(context, action)) return false;

    switch (s_actions[action].type) {
        case InputTypeKey: return lw_isKeyDown(context, s_actions[action].major.key);
        case InputTypeButton: return lw_isMouseButtonDown(context, s_actions[action].major.button);
    }
    return false;
}

bool isInputActionUp(LW_Context *const context, InputName action) {
    if (!_modifierTest(context, action)) return false;

    switch (s_actions[action].type) {
        case InputTypeKey: return lw_isKeyUp(context, s_actions[action].major.key);
        case InputTypeButton: return lw_isMouseButtonUp(context, s_actions[action].major.button);
    }
    return false;
}
