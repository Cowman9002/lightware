TARGET := lightware.dll
LIBS := -L../lib -lm -lgdi32 -lzdll -lSDL2main -lSDL2.dll
CC := x86_64-w64-mingw32-gcc
CFLAGS := -I../include -Wall -MD -MP -ggdb
DEFINES := -DVERSION_MAJOR=0 -DVERSION_MINOR=1 -DLIGHTWARE_EXPORT

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = lodepng.c lightware.c draw.c mathlib.c geo.c portal.c collision.c
OBJECTS = $(patsubst %.c, obj/%.o, $(SOURCES))
HEADERS = $(wildcard *.h)

obj/%.o: %.c $(HEADERS)
	mkdir -p $(dir obj/$<)
	$(CC) $(DEFINES) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) -fpic -shared -Wl,--out-implib,lib$(TARGET).a $(OBJECTS) $(DEFINES) $(CFLAGS) $(LIBS) -o $@

clean:
	-rm -r obj/*
	-rm -f $(TARGET)
	-rm -f lib$(TARGET).a

-include $(patsubst %.c, obj/%.d, $(SOURCES))