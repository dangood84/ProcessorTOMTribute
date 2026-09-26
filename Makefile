CC ?= cc
SRC_DIR := src
OUT_DIR := build

# MinGW (Boot Camp / MSYS2) writes tom.exe. macOS and the Pi write tom.
EXE :=
CONSOLE :=
UNAME_S := $(shell uname -s 2>/dev/null)
ifneq ($(findstring MINGW,$(UNAME_S)),)
  EXE := .exe
  CONSOLE := -mconsole
endif
ifneq ($(findstring MSYS,$(UNAME_S)),)
  EXE := .exe
  CONSOLE := -mconsole
endif
PROG := tom$(EXE)

SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(SDL_LIBS),)
  SDL_CFLAGS := -I/usr/local/include/SDL2 -D_THREAD_SAFE
  SDL_LIBS := -L/usr/local/lib -lSDL2
endif
# sdl2-config on Windows adds -mwindows, which drops the console.
# --test has to be able to print. -mconsole is applied after the SDL libs.
SDL_LIBS := $(filter-out -mwindows,$(SDL_LIBS))

CFLAGS := -std=c11 -Wall -Wextra -O2 -I$(SRC_DIR) $(SDL_CFLAGS)

SRCS := $(SRC_DIR)/main.c \
        $(SRC_DIR)/tom_machine.c \
        $(SRC_DIR)/tom_asm.c \
        $(SRC_DIR)/tom_audio.c \
        $(SRC_DIR)/tom_font.c \
        $(SRC_DIR)/tom_ui.c \
        $(SRC_DIR)/tom_test.c

.PHONY: all compile run test snap clean windows

ifeq ($(OS),Windows_NT)

# Command Prompt on Boot Camp. `make`, `make windows`, and `run.bat`
# all go through build-win.bat, which finds SDL.h beside gcc.
all: windows

windows:
	./build-win.bat

run: windows
	./build/tom.exe examples/countup.tom

test: windows
	./build/tom.exe --test examples

clean:
	rm -rf $(OUT_DIR)

else

all: compile

compile: $(OUT_DIR)/$(PROG)

$(OUT_DIR)/$(PROG): $(SRCS) $(SRC_DIR)/tom.h $(SRC_DIR)/font8x8_basic.h $(SRC_DIR)/sdl_inc.h
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(SDL_LIBS) $(CONSOLE) -lm

run: compile
	$(OUT_DIR)/$(PROG) examples/countup.tom

test: compile
	$(OUT_DIR)/$(PROG) --test examples

snap: compile
	$(OUT_DIR)/$(PROG) --snap $(OUT_DIR)/tom-start.bmp $(OUT_DIR)/tom-mid.bmp examples/countup.tom
	$(OUT_DIR)/$(PROG) --snap $(OUT_DIR)/ex1-start.bmp $(OUT_DIR)/ex1-end.bmp examples/wexampl1.tom

clean:
	rm -rf $(OUT_DIR)

endif
