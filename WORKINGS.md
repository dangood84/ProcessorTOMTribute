# How TOM works

This note is for someone who wants to build the tribute and see how a hundred memory cells become a running program: who fetches a line, who paints the yellow cell, and who plays the click and the horn.

You do not need to know SDL. The machine is plain C. SDL only opens the window, reads the mouse and keyboard, and plays the samples the program generates.

## Build / run workflows

Work from the project root. `cc` and `sdl2-config` must be on `PATH`. Output lands in `build/` (gitignored).

| What you want | Command | What you get |
|---------------|---------|--------------|
| Window with the sample | `make run` | `build/tom`, then `examples/countup.tom` |
| Headless checks | `make test` | prints `ok` lines; non-zero if a sample’s printer or counter is wrong |
| Frozen grid frames | `make snap` | `build/tom-start.bmp`, `tom-mid.bmp`, `ex1-start.bmp`, `ex1-end.bmp` |
| One program | `./build/tom examples/wexampl1.tom` | that file in the grid |
| Start over | `make clean` | deletes `build/` |

macOS (Homebrew):

```bash
brew install sdl2
make
make run
```

Debian / Raspberry Pi OS:

```bash
sudo apt install build-essential libsdl2-dev
make
./build/tom examples/countup.tom
```

Windows: `build-win.bat` from an MSYS2 prompt that already has `gcc` and SDL2. Copy `SDL2.dll` beside `build\tom.exe` if it is not on `PATH`.

`make test` never opens a window. It assembles the four files in `examples/` and runs the machine until `HLT`, feeding keypad numbers where a program asks for them.

## Mental model

```
main
  → tom_ui_main
      → tom_load_named("countup.tom")
           → tom_assemble          # two passes into mem[0..99]
      → window + audio device
      → loop
           → if the beat is due: click, tom_step, maybe horn
           → draw the grid (yellow = program counter)
           → present
```

| Layer | File | Role |
|-------|------|------|
| Entry | `main.c` | Chooses `--test`, `--snap`, or the window. Optional path to a `.tom` file. |
| Machine | `tom_machine.c` | Program counter, accumulator, one instruction, printer list. |
| Assembler | `tom_asm.c` | Text file to cells. Labels such as `done` and `answer`. |
| Sound | `tom_audio.c` | A short click, and a horn that begins with a little silence. |
| Picture | `tom_ui.c`, `tom_font.c` | 10×10 grid, printer, buttons, keypad. |
| Checks | `tom_test.c` | The seminar’s expected results, with no SDL window. |

The run loop is one thread. The audio callback is the other: it only mixes buffers that were filled at startup.

## The grid

There are 100 cells, addressed `00` to `99`, drawn left to right and then down. Program and data share those cells. Cell `00` is where execution starts.

A cell is one of three kinds:

- **empty** — nothing stored. Fetching it stops the machine with a message.
- **code** — an operator and, for most operators, an address.
- **data** — a number written by `DAT` or by `STO`.

`STO` always writes a number. The cell it writes stops being an instruction.

## Operators

These are the twelve from the seminar sheet, plus halt as zero. In the assembled view (**F3**) a cell is a single number: the operator lives in the hundreds, the address in the last two digits.

| Number | Name | Assembled form | Effect |
|--------|------|----------------|--------|
| 0 | `HLT` | `0000` | Stop. The program counter stays on this cell. |
| 1 | `LDA` | `01nn` | Accumulator = contents of cell `nn`. |
| 2 | `STO` | `02nn` | Cell `nn` = accumulator. |
| 3 | `AC+` | `03nn` | Accumulator += contents of cell `nn`. |
| 4 | `AC-` | `04nn` | Accumulator −= contents of cell `nn`. |
| 5 | `ACX` | `05nn` | Accumulator ×= contents of cell `nn`. |
| 6 | `AC\` | `06nn` | Accumulator /= contents of cell `nn`. Divide by zero stops with a fault. |
| 7 | `JMP` | `07nn` | Program counter = `nn`. |
| 8 | `JM-` | `08nn` | Jump if the accumulator is negative. |
| 9 | `JM0` | `09nn` | Jump if the accumulator is zero. |
| 10 | `J()` | `10nn` | Read cell `nn`. That number is the new program counter. |
| 11 | `INP` | `1100` | Wait for the keypad, then put that number in the accumulator. |
| 12 | `OUT` | `1200` | Append the accumulator to the printer. |

`LDA 24` is `0124`. `INP` and `OUT` are `1100` and `1200` so they are not read as `LDA 0` (`0100`) or as a small piece of data. `HLT` is `0000`. An empty cell is also numerically zero; the cell kind is what makes a real `HLT` show up as `HLT` and an unused cell stay blank.

Loading a code cell loads its assembled number. Loading a data cell loads the number stored there. An empty cell loads as 0. Integer division truncates toward zero, the same rule as C.

`J()` is the indirect jump from the sheet. The address on the line is not the destination. The number stored at that address is the destination. The test program in `tom_test.c` stores `4` and jumps to cell `04`.

## Text programs

A `.tom` file is plain text, one line per cell. A semicolon starts a comment. Names can sit in front of an operator, and `JMP` is allowed to name a label that appears later. That is why the assembler walks the file twice: the first walk records names and addresses, the second writes cells.

```
start   LDA answer
        JM0 done
        JMP start
done    HLT
        ORG 20
answer  DAT 0
```

`ORG 20` sets the next cell number. It does not emit an instruction. The editor under the grid accepts one line at a time (`LDA 24`, `DAT 55`, `HLT`) and does not accept `ORG`.

## One step

`tom_step` refuses to run when the machine has already halted, faulted, or is sitting on `INP`. Otherwise it fetches `mem[pc]`.

- `HLT` sets the halted flag and leaves `pc` where it is. After example 1 the counter shows 2, which is the `HLT`, and the yellow cell stays there.
- `JMP`, `JM-`, `JM0`, and `J()` write a new `pc` and return without the usual increment.
- `INP` sets `waiting_input` and leaves `pc` on the `INP` until `tom_supply_input` stores the number and advances.
- Every other operator then does `pc++`.

A fault (empty cell, data fetched as code, bad address, divide by zero) sets a message and does not play the horn. The horn is only a clean `HLT`.

## The beat, the click, and the horn

The window does not call `tom_step` as fast as it can draw. **F5** arms a timer (240 ms at the default speed, shown as `240 ms`). While the timer is waiting, the yellow cell is the line about to be carried out. When the beat arrives the UI plays the click and then calls `tom_step`. The highlight moves with the program counter, and the next beat waits on that new cell.

So the order you hear is the order from the seminar: the line is lit, then the click as the machine takes it, then the next line. On `HLT` the click still plays, and the horn starts as well. The horn buffer begins with about 40 ms of silence, so the click is heard on its own and the horn follows.

The click is a short decaying tick around 1700 Hz with a small low thump. The horn is two tones a major third apart (410 Hz and 520 Hz) plus a little second harmonic, which is the rasp of a small electric car horn. Both are generated into float buffers at startup and mixed in the SDL audio callback. **F10** stops the UI from triggering them. If the audio device cannot be opened, the machine still runs.

## What the window draws

- **Yellow** — `pc`. Before the first step that is cell 00.
- **Pale green** — the cell `STO` wrote on the latest step, so you can see the data move.
- **Navy outline** — the cell the editor will write, while the machine is stopped.
- **Printer** — the list from `OUT`, newest lines kept in view.
- **NOW** — the same cell as the program counter, as a mnemonic, or as the assembled number when **F3** is on.

**F5** after a halt calls `tom_reset_cpu` first: counter, accumulator, printer, and flags clear, and the program remains in the grid. **F2** does that reset without starting the run.

Keypad input is a small panel over the grid. Digits come from the keyboard or from the on-screen keys. Enter calls `tom_supply_input`. If the run button was what reached the `INP`, the run resumes after the number. A single step does not resume on its own.
