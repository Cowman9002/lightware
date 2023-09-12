#pragma once
#include "../lightware/lightware.h"

#define UNDEFINED (~0)

#define MIN_ZOOM 0.001
#define MAX_ZOOM 1.0
#define MAX_GRID (256.0f)
#define MIN_GRID (1.0f / 32.0f)

#define TEXT_BUFFER_SIZE 64

#define POINT_RENDER_RADIUS 4
#define LINE_SELECTION_RADIUS 3

#define AUTO_PORTAL_EPSILON 0.003

typedef enum State {
    StateIdle = 0,
    StateCreateSector,
    StateMovePoints,
    StateSelectionBox,
}State;

typedef struct Editor {
    char *text_buffer;
    LW_FontTexture font;
    int width, height;

    LW_Color c_background, c_grid, c_font;
    LW_Color c_walls, c_vertices, c_portal;
    LW_Color c_new_walls, c_new_vertices, c_start_vertex;
    LW_Color c_sel_vertex, c_selection_box, c_highlighted_vertices;

    // 3d mode
    bool view_3d;
    LW_Camera cam3d;

    LW_LineDef **selected_points;
    unsigned selected_points_len, selected_points_capacity;
    unsigned select_point_index;

    bool specter_select;

    // grid
    bool grid_active;
    float grid_size;

    // view
    lw_mat4 to_screen_mat;
    lw_mat4 to_world_mat;
    float zoom_t;
    float zoom;
    lw_vec2 cam_pos;
    unsigned cam_rot; // 0 = 0, 1 = 90, 2 = 180, 3 = 270

    // mouse
    lw_vec4 mouse_world_pos;
    lw_vec4 mouse_snapped_pos;

    LW_PortalWorld world;

    // state machine
    State state;

    // create sector state
    LW_Sector new_sector;
    unsigned new_sector_capacity;

    // selection box
    LW_Aabb selection_box;
    lw_vec2 selection_box_pivot;

}Editor;

bool editorInit(Editor *const editor);
int editorUpdate(Editor *const editor, float dt, LW_Context *const context);
int editorRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

int editor2dUpdate(Editor *const editor, float dt, LW_Context *const context);
int editor2dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

int editor3dUpdate(Editor *const editor, float dt, LW_Context *const context);
int editor3dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);