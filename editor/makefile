TARGET := lightware_editor
LIBS := -L../lib -lm -lgdi32 -lcomdlg32 -lole32 -lcomctl32 -loleaut32 -luuid -lshlwapi -lzdll -lSDL2main -lSDL2.dll -llightware.dll
CC := x86_64-w64-mingw32-gcc
CFLAGS := -I../include -Wall -MD -MP -ggdb -std=c11
DEFINES := -DVERSION_MAJOR=0 -DVERSION_MINOR=1

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = main.c editor.c editor2d.c editor3d.c win.c
OBJECTS = $(patsubst %.c, obj/%.o, $(SOURCES))
HEADERS = $(wildcard *.h)

obj/%.o: %.c $(HEADERS)
#	mkdir -p $(dir obj/$<)
	IF NOT EXIST "$(dir obj/$<)" mkdir "$(dir obj/$<)"
	$(CC) $(DEFINES) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(DEFINES) $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -r obj/*
	-rm -f $(TARGET)

-include $(patsubst %.c, obj/%.d, $(SOURCES))