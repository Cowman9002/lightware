#include "fileio.h"

#include <windows.h>
#include <shobjidl.h>
#include <shlwapi.h>

bool pathExists(const char *path) {
    return PathFileExistsA(path);
}

bool createRelativePath(const char *start, bool start_is_dir, const char *end, bool end_is_dir, char *o_buff) {
    return PathRelativePathToA(
        o_buff,
        start,
        start_is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
        end,
        end_is_dir ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL);
}

void doErrorPopup(const char *title, const char *message) {
    MessageBoxA(NULL, message, title, MB_OK | MB_ICONERROR);
}

bool doSaveDialog(const char *filter, const char *default_extension, const char *start_dir, char *o_file_name, unsigned file_name_size) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = filter;
    ofn.lpstrFile       = o_file_name;
    ofn.nMaxFile        = file_name_size;
    ofn.lpstrInitialDir = start_dir;
    ofn.Flags           = OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt     = default_extension;

    return GetSaveFileNameA(&ofn);
}

bool doOpenDialog(const char *filter, const char *start_dir, char *o_file_name, unsigned file_name_size) {
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize     = sizeof(ofn);
    ofn.lpstrFilter     = filter;
    ofn.lpstrFile       = o_file_name;
    ofn.nMaxFile        = file_name_size;
    ofn.lpstrInitialDir = start_dir;
    ofn.Flags           = OFN_FILEMUSTEXIST;

    return GetOpenFileNameA(&ofn);
}

bool doOpenFolderDialog(char *o_file_name, unsigned file_name_size) {
    bool res = false;
    IFileDialog *pfd;
    IShellItem *psiResult;
    PWSTR pszFilePath = NULL;
    // create dialog
    if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&pfd))) {
        pfd->lpVtbl->SetOptions(pfd, FOS_PICKFOLDERS);
        pfd->lpVtbl->Show(pfd, NULL);

        if (SUCCEEDED(pfd->lpVtbl->GetResult(pfd, &psiResult))) {
            // selected ok
            if (SUCCEEDED(psiResult->lpVtbl->GetDisplayName(psiResult, SIGDN_FILESYSPATH, &pszFilePath))) {
                WCHAR *p = pszFilePath;
                for(unsigned i = 0; i < file_name_size && p; ++i, ++p) {
                    o_file_name[i] = (char)*p;
                }
                CoTaskMemFree(pszFilePath);
                res = true;
            }
            psiResult->lpVtbl->Release(psiResult);
        }
        pfd->lpVtbl->Release(pfd);
    }

    return res;
}