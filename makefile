TARGET := lightware
LIBS := -lm -lgdi32 -lzdll -lSDL2main -lSDL2.dll
CC := gcc
CFLAGS := -Iinclude -Llib -Wall -MD -MP -g

.PHONY: default all clean

default: $(TARGET)
all: default

SOURCES = src/main.c src/lodepng.c src/draw.c src/color.c
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