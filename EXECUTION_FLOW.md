# Execution flow: from `main` to the horn

A step-by-step trace of what happens from `build/tom` through loading the sample, one beat of the run loop, and the halt.

`make run` loads `examples/countup.tom`. The same machine runs `examples/wexampl1.tom` when you press **1**. This trace follows count-up first, then the seminar’s first program, because that one has a recorded final state: program counter 2, accumulator 55, printer 55.

One thread does the window. A second thread only mixes audio.

---

## Phase A — process entry

**1.** The OS loads `build/tom`. `main` sees no `--test` and no `--snap`, and takes `examples/countup.tom` from the command make passes in.

**2.** `tom_ui_main` initialises SDL video and audio, opens the window, and calls `tom_load_named`.

**3.** The loader tries `examples/countup.tom`, then `../examples/countup.tom`, then the name as a bare path. Make runs from the project root, so the first path opens.

**4.** `tom_assemble` clears the 100 cells and walks the file twice. The first walk records `start` at 00, `done` at 09, and `answer`, `count`, `five`, `one` at 20–23. The second walk writes the operators, now that `JMP start` and `JM0 done` can be turned into addresses.

The grid after assembly:

| Cell | Contents | Assembled |
|------|----------|-----------|
| 00 | `LDA 20` | 0120 |
| 01 | `AC+ 22` | 0322 |
| 02 | `OUT` | 1200 |
| 03 | `STO 20` | 0220 |
| 04 | `LDA 21` | 0121 |
| 05 | `AC- 23` | 0423 |
| 06 | `STO 21` | 0221 |
| 07 | `JM0 09` | 0909 |
| 08 | `JMP 00` | 0700 |
| 09 | `HLT` | 0000 |
| 20 | `DAT 0` | the number 0 (answer) |
| 21 | `DAT 10` | the number 10 (count) |
| 22 | `DAT 5` | the number 5 |
| 23 | `DAT 1` | the number 1 |

The program counter is 0. The accumulator is 0. The printer is empty. Cell 00 is yellow. Nothing has been carried out yet. The status line says to press F5.

---

## Phase B — the beat

**5.** F5 calls `cmd_run`. The machine is not halted, so the counter stays at 00. `running` is set, and `next_step` is one delay in the future (240 ms unless F6 or F7 has moved it).

**6.** The loop keeps drawing. The yellow cell stays on 00 the whole time, which is the line about to run.

**7.** When `SDL_GetTicks` passes `next_step`, `perform_step` runs:

```c
/* src/tom_ui.c — the click, then the step, then the horn only for HLT */
if (!a->muted)
    audio_click();
step = tom_step(&a->m);
```

**8.** `audio_click` restarts the click buffer from the beginning. The callback on the audio thread mixes it into the speaker.

**9.** `tom_step` fetches cell 00, which is code. It records the operator in the instruction latch, adds 1 to the step count, and performs `LDA`.

Cell 20 holds the number 0, so the accumulator becomes 0. The program counter becomes 1.

**10.** The result is `STEP_OK`. The UI sets the next beat 240 ms ahead. The yellow cell is now 01 (`AC+ 22`). You are looking at the next line, and the click you just heard was line 00.

The same pair — wait, click, step — repeats for every line until a halt, a fault, or Esc.

---

## Phase C — the first printed 5

**11.** Cell 01, `AC+ 22`. Cell 22 holds 5. Accumulator becomes 0 + 5 = 5. Counter becomes 02.

**12.** Cell 02, `OUT`. The printer list gains 5. Counter becomes 03.

**13.** Cell 03, `STO 20`. Cell 20 is overwritten with the number 5 and painted pale green for this step. It is data now. Counter becomes 04.

**14.** Cell 04, `LDA 21`. Accumulator becomes 10 (the counter stored in cell 21).

**15.** Cell 05, `AC- 23`. Cell 23 holds 1. Accumulator becomes 9.

**16.** Cell 06, `STO 21`. Cell 21 becomes 9.

**17.** Cell 07, `JM0 09`. The accumulator is 9, not zero, so the jump is not taken. The counter increments to 08.

**18.** Cell 08, `JMP 00`. The program counter is set to 0 and the function returns without incrementing. The yellow cell jumps back to the start of the row. That jump is a click, the same as any other line.

One lap is nine lines. The printer shows 5. Answer (cell 20) is 5. Count (cell 21) is 9.

---

## Phase D — nine more laps, then the horn

**19.** The lap repeats. Each `OUT` appends the next multiple of 5. Each lap subtracts 1 from cell 21.

**20.** The lap that starts with answer = 45 and count = 1 does this:

- `LDA` / `AC+` / `OUT` prints **50** and stores 50 in cell 20.
- `LDA` / `AC-` / `STO` sets cell 21 to **0** and leaves 0 in the accumulator.
- `JM0 09` sees zero and sets the program counter to **09**.

**21.** The next beat highlights cell 09, `HLT`. The click plays. `tom_step` sets `halted` and does not move the counter:

```c
/* src/tom_machine.c */
case OP_HLT:
    m->halted = 1;
    return STEP_HALTED;
```

**22.** Back in `perform_step`, the result is `STEP_HALTED`. `running` is cleared, so the timer stops. `audio_horn` restarts the horn buffer. That buffer is silent for about 40 ms and then plays 410 Hz and 520 Hz together. The click finishes during the silence; the horn is what you hear afterwards.

**23.** The window keeps drawing. Yellow stays on cell 09. The printer shows 5, 10, 15, 20, 25, 30, 35, 40, 45, 50. Cell 20 holds 50. Cell 21 holds 0. The status line says the horn is the end of the program.

F5 from here calls `tom_reset_cpu` (counter 0, accumulator 0, printer cleared, program kept) and arms the beat again. F2 does the reset and waits.

---

## Phase E — the seminar’s first program

Press **1**. `examples/wexampl1.tom` replaces the grid:

| Cell | Contents |
|------|----------|
| 00 | `LDA 24` |
| 01 | `OUT` |
| 02 | `HLT` |
| 24 | `DAT 55` |

F5, three beats:

1. `LDA 24` — accumulator = 55, counter = 01. Click.
2. `OUT` — printer shows 55, counter = 02. Click.
3. `HLT` — counter stays on **02**. Click, then the horn.

That end state is the one on the seminar sheet: program counter 2, accumulator 55, the value 55 printed. `make test` checks those three numbers, and `make snap` writes the halted grid to `build/ex1-end.bmp`.

---

## Phase F — a keypad line

`examples/wexampl2.tom` and `examples/subtract.tom` begin with `INP`.

The beat that fetches `INP` still clicks, then `tom_step` returns `STEP_NEED_INPUT` and leaves the program counter on that cell. The run flag is cleared and the keypad panel is drawn. The yellow cell does not move while you type.

Enter calls `tom_supply_input`, which stores the number in the accumulator and advances the counter. If F5 was the thing that reached the `INP`, the run flag goes back on and the next beat waits on the following line. F8 steps once and leaves the machine stopped after the number.

Esc during the keypad clears the wait and does not advance. F5 will fetch the same `INP` again.

---

## Phase G — headless test and snapshots

`make test` never reaches Phase B. `tom_run_tests` calls `tom_step` in a loop, supplies the keypad numbers itself, and checks the printer and the final cells. A wrong total exits non-zero.

`make snap` uses the same loader and the same `tom_step`, draws into an off-screen surface, and writes BMPs. It does not open the window and does not play the horn. The first picture is the loaded grid. The second is after two printed numbers (count-up, mid-loop) or after the halt (example 1).
