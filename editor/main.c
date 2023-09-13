#include "../lightware/lightware.h"
#include "editor.h"

#include "fileio.h"

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#define _STRINGIFY(k) #k
#define STRINGIFY(k) _STRINGIFY(k)

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

#define MAX_RECENTS 12
static struct RecentsArray {
    int hover_index;
    unsigned len, start;
    char *data[MAX_RECENTS];
} s_recent_projects;

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

    // load recents array
    {
        // preallocate
        for (unsigned i = 0; i < MAX_RECENTS; ++i) {
            s_recent_projects.data[i] = calloc(PATH_MAX, 1);
        }
        s_recent_projects.hover_index = -1;
        s_recent_projects.len         = 0;
        s_recent_projects.start       = 0;

        FILE *recents_file = fopen("recents.data", "r");
        if (recents_file) {
            char line[PATH_MAX];
            if (fscanf(recents_file, "%u %u", &s_recent_projects.len, &s_recent_projects.start) != 2) goto _end_load_recents;
            if (s_recent_projects.len > MAX_RECENTS) s_recent_projects.len = MAX_RECENTS;

            for (unsigned i = 0; i < s_recent_projects.len; ++i) {
                if (fscanf(recents_file, "%" STRINGIFY(PATH_MAX) "s", line) != 1 || line[0] == 0) {
                    // early end
                    s_recent_projects.len = i;
                    break;
                }
                strcpy(s_recent_projects.data[i], line);
            }
            if (s_recent_projects.start > s_recent_projects.len) s_recent_projects.start = s_recent_projects.len;

        } else {
            // file not created yet
            recents_file = fopen("recents.data", "w");
        }

    _end_load_recents:
        fclose(recents_file);
    }

    int result = lw_start(context);

    lw_deinit(context);
    free(editor.project_directory);
    return result;
}

int editorUpdate(Editor *const editor, float dt, LW_Context *const context) {
    if (editor->project_directory[0] == 0) {
        if (s_recent_projects.hover_index >= 0 && lw_isMouseButtonDown(context, 0)) {
            if (s_recent_projects.hover_index == 0) {
                if (!doOpenFolderDialog(editor->project_directory, FILE_NAME_BUFFER_SIZE)) {
                    return LW_EXIT_OK;
                }
            } else {
                if (!pathExists(s_recent_projects.data[s_recent_projects.hover_index - 1])) {
                    // remove recent
                    unsigned sm1 = (s_recent_projects.start - 1 + s_recent_projects.len) % s_recent_projects.len;

                    if (s_recent_projects.hover_index == s_recent_projects.start) {
                        // remove head
                        for (unsigned i = s_recent_projects.start; i < s_recent_projects.len; ++i) {
                            strcpy(s_recent_projects.data[i - 1], s_recent_projects.data[i]);
                        }
                        s_recent_projects.start = sm1;
                    } else {
                        unsigned i = (s_recent_projects.hover_index - 1 + s_recent_projects.len) % s_recent_projects.len;

                        // shift everything after up according to circle buffer
                        for (; i != sm1;) {
                            unsigned j = (i - 1 + s_recent_projects.len) % s_recent_projects.len;
                            strcpy(s_recent_projects.data[i], s_recent_projects.data[j]);
                            i = j;
                        }

                        // shift everything back according to linear array
                        for (i = s_recent_projects.start + 1; i < s_recent_projects.len; ++i) {
                            strcpy(s_recent_projects.data[i - 1], s_recent_projects.data[i]);
                        }
                    }

                    s_recent_projects.len -= 1;

                    doErrorPopup("Not Found!", "Project directory could not be found.\n It might have been deleted or moved.");
                    return LW_EXIT_OK;
                }
                strcpy(editor->project_directory, s_recent_projects.data[s_recent_projects.hover_index - 1]);
            }
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "LightWare Editor: %s", editor->project_directory);
            lw_setWindowTitle(context, editor->text_buffer);

            // add to recent projects

            // check if project is already defined
            bool exists        = false;
            unsigned old_index = 0;
            for (unsigned i = 0; i < s_recent_projects.len; ++i) {
                if (strcmp(s_recent_projects.data[i], editor->project_directory) == 0) {
                    exists    = true;
                    old_index = i;
                    break;
                }
            }

            if (exists) {

                // move stuff back
                unsigned sm1 = (s_recent_projects.start - 1 + s_recent_projects.len) % s_recent_projects.len;
                for (unsigned i = old_index; i != sm1;) {
                    unsigned j = (i + 1) % s_recent_projects.len;
                    strcpy(s_recent_projects.data[i], s_recent_projects.data[j]);
                    i = j;
                }

                // move to front
                strcpy(s_recent_projects.data[sm1], editor->project_directory);
            } else {
                // add to start
                unsigned len                                    = strlen(editor->project_directory);
                s_recent_projects.data[s_recent_projects.start] = calloc(len, sizeof(*s_recent_projects.data[0]));
                strcpy(s_recent_projects.data[s_recent_projects.start], editor->project_directory);
                s_recent_projects.len   = min(s_recent_projects.len + 1, MAX_RECENTS);
                s_recent_projects.start = (s_recent_projects.start + 1) % (s_recent_projects.len + 1);
            }

            // write out file
            FILE *recents_file = fopen("recents.data", "w");
            if (recents_file) {
                fprintf(recents_file, "%u %u\n", s_recent_projects.len, s_recent_projects.start);
                for (unsigned i = 0; i < s_recent_projects.len; ++i) {
                    fprintf(recents_file, "%s\n", s_recent_projects.data[i]);
                }
                fclose(recents_file);
            }
        }
    } else {
        if (lw_isKeyDown(context, LW_KeyS) && (lw_isKey(context, LW_KeyLCtrl) || lw_isKey(context, LW_KeyLCtrl))) {

            // save as, or if no file is open
            if (lw_isKey(context, LW_KeyLShift) || lw_isKey(context, LW_KeyRShift) || editor->open_file[0] == 0) {
                char file_name[FILE_NAME_BUFFER_SIZE] = { 0 };

                if (doSaveDialog("Portal World (*.pod)\0*.POD\0\0", "pod", editor->project_directory, file_name, FILE_NAME_BUFFER_SIZE)) {
                    if (lw_savePortalWorld(file_name, editor->world)) {
                        // set open file name
                        strncpy(editor->open_file, file_name, FILE_NAME_BUFFER_SIZE);
                        createRelativePath(editor->project_directory, true, editor->open_file, false, editor->open_file_relative);
                    } else {
                        char message[FILE_NAME_BUFFER_SIZE + 40];
                        snprintf(message, FILE_NAME_BUFFER_SIZE + 40, "Failed to save '%s'", file_name);
                        doErrorPopup("Save Failure!", message);
                    }
                }
            } else {
                if (!lw_savePortalWorld(editor->open_file, editor->world)) {
                    char message[FILE_NAME_BUFFER_SIZE + 40];
                    snprintf(message, FILE_NAME_BUFFER_SIZE + 40, "Failed to save '%s'", editor->open_file);
                    doErrorPopup("Save Failure!", message);
                }
            }
        } else if (lw_isKeyDown(context, LW_KeyO) && (lw_isKey(context, LW_KeyLCtrl) || lw_isKey(context, LW_KeyLCtrl))) {
            char file_name[FILE_NAME_BUFFER_SIZE] = { 0 };

            if (doOpenDialog("Portal World (*.pod)\0*.POD\0\0", editor->project_directory, file_name, FILE_NAME_BUFFER_SIZE)) {
                LW_PortalWorld pod;
                LW_SectorList_init(&pod.sectors);
                if (lw_loadPortalWorld(file_name, &pod)) {
                    lw_freePortalWorld(editor->world);
                    editor->world = pod;
                    // set open file name
                    strncpy(editor->open_file, file_name, FILE_NAME_BUFFER_SIZE);
                    createRelativePath(editor->project_directory, true, editor->open_file, false, editor->open_file_relative);
                } else {
                    char message[FILE_NAME_BUFFER_SIZE + 40];
                    snprintf(message, FILE_NAME_BUFFER_SIZE + 40, "Failed to load '%s'", file_name);
                    doErrorPopup("Load Failure!", message);
                }
            }
        }

        if (editor->view_3d) {
            return editor3dUpdate(editor, dt, context);
        } else {
            return editor2dUpdate(editor, dt, context);
        }
    }

    return LW_EXIT_OK;
}

int editorRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {
    lw_ivec2 mouse_pos;
    lw_getMousePos(context, mouse_pos);
    lw_vec2 mouse_posf = { mouse_pos[0], mouse_pos[1] };

    if (editor->project_directory[0] == 0) {
        lw_fillBuffer(framebuffer, LW_COLOR_BLACK);

        s_recent_projects.hover_index = -1;
        LW_Aabb rect;
        LW_Color color;

        static const LW_Color c_text = RGB(160, 50, 40), c_highlight = RGB(180, 200, 120);

        const char *text = "OPEN PROJECT DIRECTORY";

        rect.high[0] = editor->font.char_width * strlen(text);
        rect.high[1] = editor->font.texture.height;
        rect.low[0]  = editor->width / 2 - rect.high[0] / 2;
        rect.low[1]  = editor->height / 5 - rect.high[1];

        rect.high[0] += rect.low[0];
        rect.high[1] += rect.low[1];

        color = c_text;
        if (s_recent_projects.hover_index == -1 && lw_pointInAabb(rect, mouse_posf)) {
            s_recent_projects.hover_index = 0;
            color                         = c_highlight;
        }

        lw_drawString(framebuffer, (lw_ivec2){ rect.low[0], rect.low[1] }, color, editor->font, text);

        for (unsigned i = 0; i < s_recent_projects.len; ++i) {
            unsigned j = (s_recent_projects.start - i - 1 + s_recent_projects.len) % s_recent_projects.len;
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "%s", s_recent_projects.data[j]);

            rect.high[0] = editor->font.char_width * strlen(editor->text_buffer);
            rect.high[1] = editor->font.texture.height;
            rect.low[0]  = editor->width / 2 - rect.high[0] / 2;
            rect.low[1]  = editor->height / 4 + (rect.high[1] + 5) * i;

            rect.high[0] += rect.low[0];
            rect.high[1] += rect.low[1];

            color = c_text;
            if (s_recent_projects.hover_index == -1 && lw_pointInAabb(rect, mouse_posf)) {
                s_recent_projects.hover_index = j + 1;
                color                         = c_highlight;
            }

            lw_drawString(framebuffer, (lw_ivec2){ rect.low[0], rect.low[1] }, color, editor->font, editor->text_buffer);
        }
    } else {

        int res;
        if (editor->view_3d) {
            res = editor3dRender(editor, framebuffer, context);
        } else {
            res = editor2dRender(editor, framebuffer, context);
        }

        if (res == LW_EXIT_OK) {
            if (editor->open_file_relative[0] == 0) {
                const char *text = "New File";
                lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(text) - 5, editor->height - editor->font.texture.height - 5 },
                              editor->c_font, editor->font, text);
            } else {
                snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "%s", editor->open_file_relative);
                lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, editor->height - editor->font.texture.height - 5 },
                              editor->c_font, editor->font, editor->text_buffer);
            }
        }
        return res;
    }
    return LW_EXIT_OK;
}
