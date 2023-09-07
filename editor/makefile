TARGET := lightware_editor
LIBS := -L../lib/ -lm -llightware.dll
CC := x86_64-w64-mingw32-gcc
CFLAGS := -Wall -MD -MP -ggdb
DEFINES := -DVERSION_MAJOR=0 -DVERSION_MINOR=1

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = editor.c
OBJECTS = $(patsubst %.c, obj/%.o, $(SOURCES))
HEADERS = $(wildcard *.h)

obj/%.o: %.c $(HEADERS)
	mkdir -p $(dir obj/$<)
	$(CC) $(DEFINES) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(DEFINES) $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -r obj/*
	-rm -f $(TARGET)

-include $(patsubst %.c, obj/%.d, $(SOURCES))