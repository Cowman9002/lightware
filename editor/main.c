#include "../lightware/lightware.h"
#include "editor.h"

#include <stdio.h>
#include <string.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

int update(LW_Context *const context, float delta) {
    Editor *editor = (Editor *)lw_getUserData(context);
    return editorUpdate(editor, delta, context);
}

int render(LW_Context *const context, LW_Framebuffer *const main_frame_buffer) {
    Editor *editor = (Editor *)lw_getUserData(context);
    return editorRender(editor, main_frame_buffer, context);
}

int main() {
    Editor editor;
    memset(&editor, 0, sizeof(editor));

    LW_ContextInit init = {
        .title          = "LightWare Editor",
        .logical_width  = 640,
        .logical_height = 480,
        .scale          = 2,
        .user_data      = &editor,
        .update_fn      = update,
        .render_fn      = render,
    };

    LW_Context *context = lw_init(init);
    if (context == NULL) {
        printf("Failed to create LW_Context\n");
        return -1;
    }

    editor.width  = init.logical_width;
    editor.height = init.logical_height;
    if (!editorInit(&editor)) return -1;

    int result = lw_start(context);

    lw_deinit(context);
    return result;
}

int editorUpdate(Editor *const editor, float dt, LW_Context *const context) {
    if (lw_isKeyDown(context, LW_KeyS) && (lw_isKey(context, LW_KeyLCtrl) || lw_isKey(context, LW_KeyLCtrl))) {
        if(lw_savePortalWorld("world.pod", editor->world)) {
            printf("Saved to 'world.pod'\n");
        } else {
            printf("Failed to saved to 'world.pod'\n");
        }
    } else if (lw_isKeyDown(context, LW_KeyO) && (lw_isKey(context, LW_KeyLCtrl) || lw_isKey(context, LW_KeyLCtrl))) {
        LW_PortalWorld pod;
        LW_SectorList_init(&pod.sectors);
        if (lw_loadPortalWorld("world.pod", &pod)) {
            lw_freePortalWorld(editor->world);
            editor->world = pod;
            printf("Loaded 'world.pod'\n");
        } else {
            printf("Failed to load 'world.pod'\n");
        }
    }

    if (editor->view_3d) {
        return editor3dUpdate(editor, dt, context);
    } else {
        return editor2dUpdate(editor, dt, context);
    }
}

int editorRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {
    if (editor->view_3d) {
        return editor3dRender(editor, framebuffer, context);
    } else {
        return editor2dRender(editor, framebuffer, context);
    }
}