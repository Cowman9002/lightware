#pragma once
#include "../lightware/lightware.h"

#define UNDEFINED (~0)

#define CAMERA_3D_HEIGHT 1.6f

#define MIN_ZOOM 0.001
#define MAX_ZOOM 1.0
#define MAX_GRID (256.0f)
#define MIN_GRID (1.0f / 32.0f)

#define TEXT_BUFFER_SIZE 64
#define FILE_NAME_BUFFER_SIZE 512

#define POINT_RENDER_RADIUS 3
#define LINE_SELECTION_RADIUS 5

#define AUTO_PORTAL_EPSILON 0.003

typedef enum InputName {
    InputName_swapViews,

    InputName_copy,
    InputName_paste,

    InputName_moveForwards,
    InputName_moveBackwards,
    InputName_moveLeft,
    InputName_moveRight,
    InputName_rotateLeft,
    InputName_rotateRight,
    InputName_incrZoom,
    InputName_decrZoom,
    InputName_changeZoom,

    InputName_toggleGrid,
    InputName_increaseGrid,
    InputName_decreaseGrid,
    InputName_changeGrid,
    InputName_specterSelect,

    InputName_cancel,

    InputName_selectPoint,
    InputName_multiSelect,
    InputName_selectSector,
    InputName_multiSelectSector,

    InputName_newSector,
    InputName_deletePoints,
    InputName_splitLine,
    InputName_autoPortal,
    InputName_joinSectors,
    InputName_selectionBox,

    // 3d only
    InputName_rotateUp,
    InputName_rotateDown,
    InputName_moveUp,
    InputName_moveDown,

    InputName_incrHeight,
    InputName_decrHeight,
    InputName_changeHeight,

    InputName_addSubsector,
    InputName_removeSubsector,

    InputName_size,
} InputName;

#define MODIFIER_SHIFT (1 << 0)
#define MODIFIER_CTRL (1 << 1)
#define MODIFIER_ALT (1 << 2)

typedef struct InputAction {
    enum type {
        InputTypeKey,
        InputTypeButton,
        InputTypeScroll,
    } type;

    union major {
        LW_Key key;
        unsigned button;
    } major;
    uint8_t required_modifiers;   // these must be active
    uint8_t disallowed_modifiers; // these cannot be active
} InputAction;

typedef enum State {
    StateIdle = 0,
    StateCreateSector,
    StateMovePoints,
    StateSelectionBox,
    StateJoinSectors,
} State;

struct EditorData2d {
    // Selection

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

    // state machine
    State state;

    // move points
    lw_vec2 move_origin;

    // create sector state
    LW_Sector new_sector;
    unsigned new_sector_capacity;

    // selection box
    LW_Aabb selection_box;
    lw_vec2 selection_box_pivot;

    // join sectors
    LW_Sector *join_src;
};

typedef enum RayHitType {
    RayHitType_None = 0,
    RayHitType_Floor,
    RayHitType_Ceiling,
    RayHitType_Wall0,
} RayHitType;

struct EditorData3d {
    LW_Camera camera;
    
    RayHitType ray_hit_type;
    lw_vec3 intersect_point;
    float intersect_dist;

    float floor_snap_val;
    float copied_height_val;
};

typedef struct Editor {
    char *open_file;
    char *open_file_relative;
    char *project_directory;

    LW_PortalWorld world;
    
    char *text_buffer;
    LW_FontTexture font;
    int width, height;

    LW_Color c_background, c_grid, c_font;
    LW_Color c_walls, c_vertices, c_portal;
    LW_Color c_new_walls, c_new_vertices, c_start_vertex;
    LW_Color c_sel_vertex, c_selection_box, c_highlighted_vertices;
    
    bool view_3d;

    struct EditorData3d data3d;
    struct EditorData2d data2d;

    // fps counter
    unsigned fps_frames;
    float fps_counter;
    unsigned cached_fps;

} Editor;

bool editorInit(Editor *const editor);
int editorUpdate(Editor *const editor, float dt, LW_Context *const context);
int editorRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

int editor2dUpdate(Editor *const editor, float dt, LW_Context *const context);
int editor2dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

int editor3dUpdate(Editor *const editor, float dt, LW_Context *const context);
int editor3dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

void setInputAction(InputName action, InputAction val);
bool isInputAction(LW_Context *const context, InputName action);
bool isInputActionDown(LW_Context *const context, InputName action);
bool isInputActionUp(LW_Context *const context, InputName action);
float inputActionValue(LW_Context *const context, InputName action);