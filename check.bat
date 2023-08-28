@REM drmemory -suppress ./drmem_suppress.txt -- ./lightware.exe
drmemory -check_uninit_blocklist SDL2.dll -lib_blocklist SDL2.dll!* -lib_blocklist_frames 1 -- ./lightware.exe
@REM drmemory -- ./lightware.exe