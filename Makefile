CC ?= cc
SRC_DIR := src
OUT_DIR := build
PROG := tom

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(SDL_LIBS),)
  SDL_CFLAGS := -I/usr/local/include/SDL2 -D_THREAD_SAFE
  SDL_LIBS := -L/usr/local/lib -lSDL2
endif

CFLAGS := -std=c11 -Wall -Wextra -O2 -I$(SRC_DIR) $(SDL_CFLAGS)

SRCS := $(SRC_DIR)/main.c \
        $(SRC_DIR)/tom_machine.c \
        $(SRC_DIR)/tom_asm.c \
        $(SRC_DIR)/tom_audio.c \
        $(SRC_DIR)/tom_font.c \
        $(SRC_DIR)/tom_ui.c \
        $(SRC_DIR)/tom_test.c

.PHONY: all compile run test snap clean

all: compile

compile: $(OUT_DIR)/$(PROG)

$(OUT_DIR)/$(PROG): $(SRCS) $(SRC_DIR)/tom.h $(SRC_DIR)/font8x8_basic.h
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(SDL_LIBS) -lm

run: compile
	$(OUT_DIR)/$(PROG) examples/countup.tom

test: compile
	$(OUT_DIR)/$(PROG) --test examples

snap: compile
	$(OUT_DIR)/$(PROG) --snap $(OUT_DIR)/tom-start.bmp $(OUT_DIR)/tom-mid.bmp examples/countup.tom
	$(OUT_DIR)/$(PROG) --snap $(OUT_DIR)/ex1-start.bmp $(OUT_DIR)/ex1-end.bmp examples/wexampl1.tom

clean:
	rm -rf $(OUT_DIR)
