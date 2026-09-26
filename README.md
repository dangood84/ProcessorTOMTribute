# TOM

A small tribute to the classroom CPU simulator from BTEC National Diploma Computing: a 10×10 memory grid, a short assembly language, and a yellow cell that walks the program from the first line to `HLT`.

Each line makes a click. A clean halt plays a two-tone car horn.

The seminar called the machine **TOM** — Totally Obedient Moron. The operator names along the bottom (`LDA`, `STO`, `AC+`, `JMP`, `JM0`, `INP`, `OUT`, `HLT`, and the rest) follow that sheet. This program is a new simulator written in C. It does not include the original `TOM.exe`.

How the pieces fit together: `WORKINGS.md` for the machine and the files, `EXECUTION_FLOW.md` for a line-by-line trace.

## Sample to run

The window opens with `examples/countup.tom` already loaded. That is the seminar exercise “count up in fives”.

```bash
make run
```

Press **F5**.

The yellow cell starts on `LDA 20`. It walks the top row, jumps back to cell 00 nine times, and the printer fills with:

```
5  10  15  20  25  30  35  40  45  50
```

Then the cell sits on `HLT` and the horn sounds. **F7** shortens the pause between lines. **Esc** stops early. **F2** puts the counter and the printer back to the start and leaves the program in the grid.

The same source is in `examples/countup.tom` if you want to change a number and run it again:

```
./build/tom examples/countup.tom
```

You can also drop a `.tom` file onto the window.

### The first program from the seminar

Press **1** (or the **1 First** button) and then **F5**. This is `examples/wexampl1.tom`:

```
LDA 24
OUT
HLT

ORG 24
DAT 55
```

When it halts, the program counter shows **02**, the accumulator shows **55**, and the printer shows **55**. That is the state the seminar sheet recorded after example 1. The horn plays on the `HLT`.

## Requirements

- A C11 compiler (`cc` or `gcc`)
- **SDL2**

The grid is drawn at 1100×740. On a smaller Windows desktop or a Raspberry Pi panel the window shrinks to fit, and the same grid is scaled into it.

### macOS

Homebrew, including an M1 MacBook Pro on Sonoma and an Intel Mac:

```bash
brew install sdl2
make test
make run
```

On Apple Silicon, Homebrew lives in `/opt/homebrew`. On a 2014 Intel Mac it lives in `/usr/local`. The Makefile uses `sdl2-config`, so either prefix works.

### Raspberry Pi OS

Use the **desktop** image. A Lite/SSH-only session has no window to open. `make test` still runs over SSH; the horn and the grid need the desktop.

**1. Packages**

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev
```

`libsdl2-dev` supplies the matching ARM library. There is no `SDL2.dll` to copy. 32-bit Pi OS (`armhf`) and 64-bit Pi OS (`aarch64`) both work.

**2. Test, then open the grid**

From the project folder:

```bash
make test
make run
```

`make test` prints `all tests passed` and does not open a window. `make run` opens count-up. Press **F5**.

If the window never appears over SSH, run it on the Pi’s desktop, or set `DISPLAY` if you are forwarding X.

### Windows 10 (Boot Camp)

This is the path for a 2014 Intel MacBook Pro on **Boot Camp** (Windows 10, 64-bit). The Mac disk is often read-only from Windows, so clone the project onto `C:` rather than building on the macOS volume.

**1. Compiler and SDL2**

Install [MSYS2](https://www.msys2.org/) if `gcc` is not already there. Open **MSYS2 MinGW x64** (the blue one, not the plain MSYS shell) and run:

```bash
pacman -Syu
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkgconf make git
```

If `pacman -Syu` closes the window, open it again and finish the second update it asks for, then install the packages.

`SDL2.dll` is what Windows loads when the program **starts**. The message `SDL.h: No such file or directory` happens while **compiling**, before that DLL is used. The header `SDL.h` is in the SDL2 development package (`include\SDL2`), which `pacman -S mingw-w64-x86_64-SDL2` installs next to `gcc`. Copying the DLL into `build\` does not put that header on the compiler’s path.

**2. Build from Command Prompt**

```bat
cd C:\Users\DGood\Documents\ProcessorTOMTribute
build-win.bat
```

The bat looks for `SDL.h` beside `gcc` (and under `C:\msys64\mingw64`). It then copies `SDL2.dll` into `build\` itself.

Same build from make, if `make` is on PATH:

```bat
make windows
```

**3. Test, then open the grid**

```bat
build\tom.exe --test examples
run.bat
```

`run` is the same as `run.bat`. `--test` prints `all tests passed` with no window. `run.bat` opens count-up. Press **F5**.

If Windows says it cannot find `SDL2.dll` at startup, the DLL is missing from `build\` or it is the wrong bitness. `gcc -dumpmachine` and the DLL have to match. The bat copies the DLL that belongs to that `gcc`.

## Run

From the project root:

```bash
./run.sh
```

or:

```bash
make run          # opens countup.tom
make test         # assembles and runs the samples with no window
make snap         # writes BMP frames of the grid into build/
make clean
```

Other programs:

```bash
./build/tom examples/wexampl1.tom
./build/tom examples/wexampl2.tom
./build/tom examples/subtract.tom
```

## Keys

| Key | Action |
|-----|--------|
| **F5** | Run. After a halt, this starts again from cell 00. |
| **F8** | Carry out one line. |
| **Esc** | Stop. During keypad input, Esc cancels the number. |
| **F2** | Reset the counter, accumulator, and printer. The grid stays. |
| **F3** | Switch between mnemonics and assembled numbers (`LDA 24` is `0124`). |
| **F6** / **F7** | Slower / faster. The delay is shown next to the registers. |
| **F10** | Mute the click and the horn. |
| **1** **2** **3** **4** | Load a sample, when the edit line is empty. **Ctrl+1** through **Ctrl+4** always load one. |
| **Enter** | Write the edit line into the selected cell and move to the next cell. |

Click a cell to select it (navy outline). The operator buttons fill the edit line. `HLT`, `INP`, and `OUT` write themselves straight away, because they have no address. `LDA` and the others wait for you to type the address and press Enter.

While a program is running, the yellow cell is the program counter. A pale green cell is the one `STO` just wrote.

## The other samples

| Key | File | What it does |
|-----|------|----------------|
| 1 | `examples/wexampl1.tom` | Load 55, print it, halt. |
| 2 | `examples/wexampl2.tom` | Read two numbers from the keypad, add them, print the answer. |
| 3 | `examples/countup.tom` | Count up in fives. This is the one `make run` loads. |
| 4 | `examples/subtract.tom` | Read two numbers and print the first minus the second. |

For **2** and **4**, the yellow cell stops on `INP` and a keypad appears. Type a number and press Enter. The click has already sounded for that line; the next line waits until the number is in.

## Project layout

```
src/
  main.c            # --test, --snap, or the window
  tom.h             # the 100 cells, the operator numbers
  tom_machine.c     # one fetch-execute step
  tom_asm.c         # .tom text into the grid (two passes, so labels can point forward)
  tom_audio.c       # click, then the horn
  tom_font.c        # 8×8 text
  tom_ui.c          # grid, printer, keypad, run loop
  tom_test.c        # headless checks for the four samples
examples/
  countup.tom
  wexampl1.tom
  wexampl2.tom
  subtract.tom
```
