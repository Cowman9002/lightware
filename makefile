TARGET := lightware
LIBS := -lm -lgdi32 -lzdll -lSDL2main -lSDL2.dll
# CC := gcc
CC := x86_64-w64-mingw32-gcc
# CFLAGS := -Iinclude -Llib -Wall -MD -MP -g -DMEMDEBUG
CFLAGS := -static-libgcc -Iinclude -Llib -Wall -MD -MP -ggdb
# CFLAGS := -static-libgcc -Iinclude -Llib -Wall -MD -MP -O2

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = src/main.c src/lodepng.c src/util.c src/draw.c src/color.c src/geo.c src/portals.c
OBJECTS = $(patsubst %.c, obj/%.o, $(SOURCES))
HEADERS = $(wildcard *.h)

obj/%.o: %.c $(HEADERS)
	mkdir -p $(dir obj/$<)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -r obj/*
	-rm -f $(TARGET)

-include $(patsubst %.c, obj/%.d, $(SOURCES))