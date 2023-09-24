#pragma once

#include <stdbool.h>

bool pathExists(const char *path);

bool createRelativePath(const char *start, bool start_is_dir, const char *end, bool end_is_dir, char *o_buff);

void doErrorPopup(const char *title, const char *message);

/// @brief Open system save dialog and fill o_file_name with path
/// @param filter File filter in format "Text file (*.txt)\0*.TXT\0PNG (*.png)\*.PNG\0\0"
/// @param default_extension Extension added to file if not set by user. Don't include the dot and don't expect more than 3 chars being used.
/// @param start_dir Where to open dialog. System dependent on how this works
/// @param o_file_name Buffer to set with file path
/// @param file_name_size Size of o_file_name
/// @return true if file has been selected, false if user canceled or some other error occurs.
bool doSaveDialog(const char *filter, const char *default_extension, const char *start_dir, char *o_file_name, unsigned file_name_size);

/// @brief Open system open dialog and fill o_file_name with path
/// @param filter File filter in format "Text file (*.txt)\0*.TXT\0PNG (*.png)\*.PNG\0\0"
/// @param start_dir Where to open dialog. System dependent on how this works
/// @param o_file_name Buffer to set with file path
/// @param file_name_size Size of o_file_name
/// @return true if file has been selected, false if user canceled or some other error occurs.
bool doOpenDialog(const char *filter, const char *start_dir, char *o_file_name, unsigned file_name_size);

bool doOpenFolderDialog(char *o_file_name, unsigned file_name_size);