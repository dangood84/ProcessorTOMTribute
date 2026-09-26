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
- **SDL2** (`sdl2-config` on your `PATH`)

macOS (Homebrew), including an M1 MacBook Pro on Sonoma:

```bash
brew install sdl2
```

Debian / Raspberry Pi OS:

```bash
sudo apt install build-essential libsdl2-dev
```

### Windows 10

From an MSYS2 UCRT prompt, with gcc and SDL2 installed:

```bat
build-win.bat
build\tom.exe examples\countup.tom
```

If Windows cannot find `SDL2.dll`, copy it next to `tom.exe`. Match the DLL to the compiler (64-bit gcc needs the 64-bit DLL).

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
