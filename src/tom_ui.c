#include "tom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

void tom_draw_text(SDL_Renderer *ren, int x, int y, int scale, const char *text,
                   Uint8 r, Uint8 g, Uint8 b);
int tom_text_width(const char *text, int scale);

#define WIN_W 1100
#define WIN_H 740
#define GRID_X 16
#define GRID_Y 70
#define CELL_W 72
#define CELL_H 50
#define CELL_GAP 3
#define MAX_BUTTONS 48
#define EDIT_MAX 40

enum {
    ACT_RUN = 1,
    ACT_STEP,
    ACT_STOP,
    ACT_RESET,
    ACT_VIEW,
    ACT_SLOWER,
    ACT_FASTER,
    ACT_SOUND,
    ACT_CLEAR,
    ACT_DAT,
    ACT_OP_BASE = 100,    /* + operator number */
    ACT_SAMPLE_BASE = 200, /* + 1..4 */
    ACT_KEY_BASE = 300     /* + digit, or 310 minus, 311 enter, 312 backspace */
};

static const int SPEEDS[] = {640, 400, 240, 140, 80};
#define SPEED_COUNT ((int)(sizeof SPEEDS / sizeof SPEEDS[0]))

typedef struct Button {
    SDL_Rect r;
    int id;
    char label[20];
} Button;

typedef struct App {
    SDL_Window *win;
    SDL_Renderer *ren;
    TomMachine m;
    Button buttons[MAX_BUTTONS];
    int button_count;
    int running;
    int resume_after_input;
    int input_mode;
    int speed_index;
    Uint32 next_step;
    int selected;
    int numbers;
    int muted;
    int status_bad;
    char edit[EDIT_MAX + 1];
    char input[16];
    char status[220];
    char loaded[260];
} App;

static const SDL_Color COL_FACE = {192, 192, 192, 255};
static const SDL_Color COL_WHITE = {255, 255, 255, 255};
static const SDL_Color COL_YELLOW = {255, 255, 102, 255};
static const SDL_Color COL_GREEN = {198, 228, 198, 255};
static const SDL_Color COL_PAPER = {255, 252, 232, 255};
static const SDL_Color COL_INK = {20, 20, 20, 255};
static const SDL_Color COL_DIM = {96, 96, 96, 255};
static const SDL_Color COL_NAVY = {0, 0, 128, 255};
static const SDL_Color COL_RED = {150, 0, 0, 255};
static const SDL_Color COL_SHADOW = {128, 128, 128, 255};
static const SDL_Color COL_LIGHT = {255, 255, 255, 255};

static void set_color(SDL_Renderer *ren, SDL_Color c)
{
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
}

static void fill_rect(SDL_Renderer *ren, SDL_Rect r, SDL_Color c)
{
    set_color(ren, c);
    SDL_RenderFillRect(ren, &r);
}

static void bevel(SDL_Renderer *ren, SDL_Rect r, int sunken)
{
    SDL_Color tl = sunken ? COL_SHADOW : COL_LIGHT;
    SDL_Color br = sunken ? COL_LIGHT : COL_SHADOW;
    set_color(ren, tl);
    SDL_RenderDrawLine(ren, r.x, r.y, r.x + r.w - 1, r.y);
    SDL_RenderDrawLine(ren, r.x, r.y, r.x, r.y + r.h - 1);
    set_color(ren, br);
    SDL_RenderDrawLine(ren, r.x, r.y + r.h - 1, r.x + r.w - 1, r.y + r.h - 1);
    SDL_RenderDrawLine(ren, r.x + r.w - 1, r.y, r.x + r.w - 1, r.y + r.h - 1);
}

static SDL_Rect cell_rect(int addr)
{
    SDL_Rect r;
    r.x = GRID_X + (addr % 10) * (CELL_W + CELL_GAP);
    r.y = GRID_Y + (addr / 10) * (CELL_H + CELL_GAP);
    r.w = CELL_W;
    r.h = CELL_H;
    return r;
}

static int grid_right(void)
{
    return GRID_X + 10 * CELL_W + 9 * CELL_GAP;
}

static int grid_bottom(void)
{
    return GRID_Y + 10 * CELL_H + 9 * CELL_GAP;
}

static void set_status(App *a, const char *text, int bad)
{
    snprintf(a->status, sizeof a->status, "%s", text);
    a->status_bad = bad;
}

static void set_title(App *a)
{
    char title[320];
    const char *base = a->loaded[0] ? a->loaded : "TOM";
    const char *slash = strrchr(base, '/');
    if (!a->win)
        return;
    if (slash)
        base = slash + 1;
    snprintf(title, sizeof title, "TOM — %s", base);
    SDL_SetWindowTitle(a->win, title);
}

static void refresh_edit_from_cell(App *a)
{
    tom_format_source(&a->m.mem[a->selected], a->edit, sizeof a->edit);
}

static int load_into(App *a, const char *name)
{
    char err[200];
    char used[260];
    if (!tom_load_named(&a->m, name, err, sizeof err, used, sizeof used)) {
        set_status(a, err, 1);
        return 0;
    }
    snprintf(a->loaded, sizeof a->loaded, "%s", used);
    a->running = 0;
    a->input_mode = 0;
    a->resume_after_input = 0;
    a->selected = 0;
    a->edit[0] = '\0';
    set_title(a);
    set_status(a,
               "Sample loaded. F5 runs it. Yellow cell, a click on each line, horn at HLT. F7 is faster.",
               0);
    return 1;
}

static void store_edit(App *a)
{
    TomCell cell;
    char err[160];
    if (a->running || a->input_mode)
        return;
    if (a->edit[0] == '\0')
        return;
    if (!tom_parse_cell(a->edit, &cell, err, sizeof err)) {
        set_status(a, err, 1);
        return;
    }
    a->m.mem[a->selected] = cell;
    a->m.halted = 0;
    a->m.fault = 0;
    if (a->selected < TOM_MEM - 1)
        a->selected++;
    a->edit[0] = '\0';
    set_status(a, "Stored. Enter writes the next cell. F5 runs from the current counter.", 0);
}

static void cmd_reset(App *a)
{
    tom_reset_cpu(&a->m);
    a->running = 0;
    a->input_mode = 0;
    a->resume_after_input = 0;
    a->input[0] = '\0';
    set_status(a, "Reset. The program is still in the grid. F5 runs from 00.", 0);
}

static void perform_step(App *a, int from_run)
{
    TomStep step;

    /* The click is the line being carried out. The horn is added only
       when that line is HLT, after a short gap baked into the horn. */
    if (!a->muted)
        audio_click();
    step = tom_step(&a->m);
    if (step == STEP_NEED_INPUT) {
        a->resume_after_input = from_run;
        a->running = 0;
        a->input_mode = 1;
        a->input[0] = '\0';
        set_status(a, "Keypad: type a number and press Enter.", 0);
        return;
    }
    if (step == STEP_HALTED) {
        a->running = 0;
        if (!a->muted)
            audio_horn();
        set_status(a, "Halted. The horn is the end of the program. F5 runs it again.", 0);
        return;
    }
    if (step == STEP_FAULT) {
        a->running = 0;
        set_status(a, a->m.message[0] ? a->m.message : "Stopped on a fault.", 1);
        return;
    }
}

static void cmd_run(App *a)
{
    if (a->input_mode || a->running)
        return;
    if (a->m.halted || a->m.fault)
        tom_reset_cpu(&a->m);
    a->running = 1;
    /* Wait one beat on the yellow cell before carrying it out,
       so the eye lands on the line and then hears the click as it jumps. */
    a->next_step = SDL_GetTicks() + (Uint32)SPEEDS[a->speed_index];
    set_status(a, "Running. Esc stops. F7 faster, F6 slower.", 0);
}

static void cmd_step(App *a)
{
    if (a->input_mode)
        return;
    a->running = 0;
    if (a->m.halted || a->m.fault)
        tom_reset_cpu(&a->m);
    perform_step(a, 0);
    if (!a->m.halted && !a->m.fault && !a->input_mode)
        set_status(a, "Stepped one line. F8 again, or F5 to run.", 0);
}

static void cmd_stop(App *a)
{
    a->running = 0;
    if (a->input_mode) {
        a->m.waiting_input = 0;
        a->input_mode = 0;
        set_status(a, "Input cancelled. F5 continues from this cell.", 0);
        return;
    }
    if (a->m.halted)
        set_status(a, "Halted. F5 runs the program again from 00.", 0);
    else
        set_status(a, "Stopped. F5 continues. F2 resets the counter and the printer.", 0);
}

static void insert_operator(App *a, int opcode)
{
    if (a->running || a->input_mode)
        return;
    /* HLT, INP and OUT have no address, so the button writes the cell at once.
       The others wait for the address: the edit line becomes "LDA " and so on. */
    if (opcode == OP_HLT || opcode == OP_INP || opcode == OP_OUT) {
        snprintf(a->edit, sizeof a->edit, "%s", tom_mnemonic(opcode));
        store_edit(a);
        return;
    }
    snprintf(a->edit, sizeof a->edit, "%s ", tom_mnemonic(opcode));
}

static void change_speed(App *a, int dir)
{
    a->speed_index += dir;
    if (a->speed_index < 0)
        a->speed_index = 0;
    if (a->speed_index >= SPEED_COUNT)
        a->speed_index = SPEED_COUNT - 1;
}

static void apply_action(App *a, int id)
{
    if (id >= ACT_KEY_BASE) {
        int key = id - ACT_KEY_BASE;
        size_t n = strlen(a->input);
        if (!a->input_mode)
            return;
        if (key == 311) {
            /* submitted by caller via submit_input; handled below */
            return;
        }
        if (key == 312) {
            if (n > 0)
                a->input[n - 1] = '\0';
            return;
        }
        if (key == 310) {
            if (n == 0)
                snprintf(a->input, sizeof a->input, "-");
            return;
        }
        if (key >= 0 && key <= 9 && n + 1 < sizeof a->input) {
            a->input[n] = (char)('0' + key);
            a->input[n + 1] = '\0';
        }
        return;
    }

    if (a->input_mode)
        return;

    if (id >= ACT_SAMPLE_BASE && id < ACT_SAMPLE_BASE + 10) {
        const char *names[] = {"", "wexampl1.tom", "wexampl2.tom", "countup.tom", "subtract.tom"};
        int which = id - ACT_SAMPLE_BASE;
        if (which >= 1 && which <= 4)
            load_into(a, names[which]);
        return;
    }
    if (id == ACT_DAT) {
        if (!a->running && !a->input_mode)
            snprintf(a->edit, sizeof a->edit, "DAT ");
        return;
    }
    if (id >= ACT_OP_BASE && id < ACT_OP_BASE + 20) {
        insert_operator(a, id - ACT_OP_BASE);
        return;
    }
    switch (id) {
    case ACT_RUN: cmd_run(a); break;
    case ACT_STEP: cmd_step(a); break;
    case ACT_STOP: cmd_stop(a); break;
    case ACT_RESET: cmd_reset(a); break;
    case ACT_VIEW:
        a->numbers = !a->numbers;
        set_status(a, a->numbers ? "Assembled numbers. F3 returns to mnemonics."
                                 : "Mnemonics. F3 shows the assembled numbers.", 0);
        break;
    case ACT_SLOWER: change_speed(a, -1); break;
    case ACT_FASTER: change_speed(a, +1); break;
    case ACT_SOUND:
        a->muted = !a->muted;
        set_status(a, a->muted ? "Sound off." : "Sound on. Click each line, horn at HLT.", 0);
        break;
    case ACT_CLEAR:
        if (!a->running) {
            memset(&a->m.mem[a->selected], 0, sizeof a->m.mem[a->selected]);
            a->edit[0] = '\0';
            set_status(a, "Cell cleared.", 0);
        }
        break;
    default:
        break;
    }
}

static void submit_input(App *a)
{
    char *end = NULL;
    long value;
    if (!a->input_mode)
        return;
    if (a->input[0] == '\0' || strcmp(a->input, "-") == 0) {
        set_status(a, "Type a number, then Enter.", 1);
        return;
    }
    value = strtol(a->input, &end, 10);
    if (end == a->input || (end && *end != '\0')) {
        set_status(a, "That is not a whole number.", 1);
        return;
    }
    tom_supply_input(&a->m, (int)value);
    a->input_mode = 0;
    if (a->m.fault) {
        set_status(a, a->m.message, 1);
        return;
    }
    if (a->resume_after_input) {
        a->running = 1;
        a->next_step = SDL_GetTicks() + (Uint32)SPEEDS[a->speed_index];
        set_status(a, "Running.", 0);
    } else {
        set_status(a, "Number accepted. F8 steps, F5 runs.", 0);
    }
}

static void add_button(App *a, SDL_Rect r, int id, const char *label)
{
    Button *b;
    if (a->button_count >= MAX_BUTTONS)
        return;
    b = &a->buttons[a->button_count++];
    b->r = r;
    b->id = id;
    snprintf(b->label, sizeof b->label, "%s", label);
}

static void draw_button(App *a, SDL_Rect r, int id, const char *label)
{
    SDL_Rect face;
    SDL_Color raised = {224, 224, 224, 255};
    int tw;
    add_button(a, r, id, label);
    /* A dark outline, then a lighter face, so the operator keys read as buttons
       on the grey window rather than as loose captions. */
    set_color(a->ren, (SDL_Color){64, 64, 64, 255});
    SDL_RenderDrawRect(a->ren, &r);
    face = r;
    face.x += 2;
    face.y += 2;
    face.w -= 4;
    face.h -= 4;
    if (face.w < 1)
        face.w = 1;
    if (face.h < 1)
        face.h = 1;
    fill_rect(a->ren, face, raised);
    bevel(a->ren, face, 0);
    tw = tom_text_width(label, 1);
    tom_draw_text(a->ren, r.x + (r.w - tw) / 2, r.y + (r.h - 8) / 2, 1, label, 0, 0, 0);
}

static void draw_centered(SDL_Renderer *ren, SDL_Rect r, int y, int scale, const char *text, SDL_Color c)
{
    int tw = tom_text_width(text, scale);
    int x = r.x + (r.w - tw) / 2;
    if (x < r.x + 2)
        x = r.x + 2;
    tom_draw_text(ren, x, y, scale, text, c.r, c.g, c.b);
}

static void draw_cell(App *a, int addr)
{
    SDL_Rect r = cell_rect(addr);
    SDL_Color fill = COL_WHITE;
    char addr_text[8];
    char top[24];
    char bot[24];
    int scale;
    int y_top;

    /* Yellow is the cell the program counter is on: the line being carried out.
       Pale green is the cell STO just wrote, so the data movement is visible. */
    if (addr == a->m.pc && a->m.pc >= 0 && a->m.pc < TOM_MEM)
        fill = COL_YELLOW;
    else if (addr == a->m.last_write)
        fill = COL_GREEN;
    fill_rect(a->ren, r, fill);
    set_color(a->ren, COL_SHADOW);
    SDL_RenderDrawRect(a->ren, &r);
    if (!a->running && addr == a->selected) {
        SDL_Rect box = r;
        box.x += 1;
        box.y += 1;
        box.w -= 2;
        box.h -= 2;
        set_color(a->ren, COL_NAVY);
        SDL_RenderDrawRect(a->ren, &box);
    }

    snprintf(addr_text, sizeof addr_text, "%02d", addr);
    tom_draw_text(a->ren, r.x + 3, r.y + 2, 1, addr_text, COL_DIM.r, COL_DIM.g, COL_DIM.b);

    tom_format_parts(&a->m.mem[addr], a->numbers, top, sizeof top, bot, sizeof bot);
    if (top[0] == '\0')
        return;
    scale = tom_text_width(top, 2) <= r.w - 6 ? 2 : 1;
    y_top = bot[0] ? r.y + 14 : r.y + 20;
    draw_centered(a->ren, r, y_top, scale, top, COL_INK);
    if (bot[0])
        draw_centered(a->ren, r, r.y + 32, scale, bot, COL_INK);
}

static void draw_printer(App *a, SDL_Rect panel)
{
    SDL_Rect paper;
    int i;
    int line_h = 16;
    int first;
    int visible;
    char line[32];

    fill_rect(a->ren, panel, COL_FACE);
    bevel(a->ren, panel, 1);
    tom_draw_text(a->ren, panel.x + 10, panel.y + 8, 1, "PRINTER", 0, 0, 0);
    paper.x = panel.x + 8;
    paper.y = panel.y + 22;
    paper.w = panel.w - 16;
    paper.h = panel.h - 32;
    fill_rect(a->ren, paper, COL_PAPER);
    visible = paper.h / line_h;
    if (visible < 1)
        visible = 1;
    first = 0;
    if (a->m.output_count > visible)
        first = a->m.output_count - visible;
    for (i = first; i < a->m.output_count; i++) {
        snprintf(line, sizeof line, "%d", a->m.output[i]);
        tom_draw_text(a->ren, paper.x + 8, paper.y + 4 + (i - first) * line_h, 2,
                      line, 0, 80, 0);
    }
}

static void draw_edit(App *a, SDL_Rect r)
{
    char label[32];
    fill_rect(a->ren, r, COL_WHITE);
    bevel(a->ren, r, 1);
    snprintf(label, sizeof label, "Cell %02d", a->selected);
    tom_draw_text(a->ren, r.x + 6, r.y + 6, 1, label, COL_DIM.r, COL_DIM.g, COL_DIM.b);
    tom_draw_text(a->ren, r.x + 70, r.y + 4, 2, a->edit, 0, 0, 0);
    if (!a->running && !a->input_mode) {
        SDL_Rect caret;
        caret.x = r.x + 70 + tom_text_width(a->edit, 2);
        caret.y = r.y + 4;
        caret.w = 8;
        caret.h = 16;
        fill_rect(a->ren, caret, COL_NAVY);
    }
}

static void draw_modal(App *a)
{
    SDL_Rect box;
    SDL_Rect field;
    char shown[24];
    int i;
    int x;
    const char *keys = "0123456789";

    box.w = 520;
    box.h = 160;
    box.x = (WIN_W - box.w) / 2;
    box.y = (WIN_H - box.h) / 2;
    fill_rect(a->ren, box, COL_FACE);
    bevel(a->ren, box, 0);
    tom_draw_text(a->ren, box.x + 16, box.y + 12, 2, "KEYPAD", 0, 0, 0);
    tom_draw_text(a->ren, box.x + 16, box.y + 36, 1, "TOM is waiting for a number", 0, 0, 0);
    field.x = box.x + 16;
    field.y = box.y + 52;
    field.w = box.w - 32;
    field.h = 28;
    fill_rect(a->ren, field, COL_WHITE);
    bevel(a->ren, field, 1);
    snprintf(shown, sizeof shown, "%s", a->input);
    tom_draw_text(a->ren, field.x + 8, field.y + 6, 2, shown, 0, 0, 0);

    x = box.x + 16;
    for (i = 0; i < 10; i++) {
        SDL_Rect k;
        char lab[2];
        k.x = x;
        k.y = box.y + 96;
        k.w = 32;
        k.h = 28;
        lab[0] = keys[i];
        lab[1] = '\0';
        draw_button(a, k, ACT_KEY_BASE + i, lab);
        x += 36;
    }
    {
        SDL_Rect k;
        k.x = x;
        k.y = box.y + 96;
        k.w = 32;
        k.h = 28;
        draw_button(a, k, ACT_KEY_BASE + 310, "-");
        k.x += 36;
        k.w = 48;
        draw_button(a, k, ACT_KEY_BASE + 311, "Enter");
    }
}

static void draw_all(App *a)
{
    SDL_Rect printer;
    SDL_Rect edit;
    char reg[64];
    char speed[32];
    int i;
    int x;
    int op_y = grid_bottom() + 10;
    int edit_y = op_y + 32;
    int ctrl_y = edit_y + 32;
    const char *ops[] = {
        "HLT", "LDA", "STO", "AC+", "AC-", "ACX", "AC\\", "JMP", "JM-", "JM0", "J()", "INP", "OUT"
    };
    const int opcodes[] = {
        OP_HLT, OP_LDA, OP_STO, OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_JMP, OP_JMN, OP_JMZ, OP_JIND, OP_INP, OP_OUT
    };
    const char *samples[] = {"1 First", "2 Add", "3 Fives", "4 Subtract"};

    a->button_count = 0;
    fill_rect(a->ren, (SDL_Rect){0, 0, WIN_W, WIN_H}, COL_FACE);

    tom_draw_text(a->ren, 16, 8, 2, "TOM", 0, 0, 0);
    tom_draw_text(a->ren, 16 + tom_text_width("TOM", 2) + 10, 14, 1,
                  "Totally Obedient Moron", COL_DIM.r, COL_DIM.g, COL_DIM.b);

    x = 360;
    for (i = 0; i < 4; i++) {
        SDL_Rect b;
        b.x = x;
        b.y = 8;
        b.w = 110;
        b.h = 24;
        draw_button(a, b, ACT_SAMPLE_BASE + i + 1, samples[i]);
        x += 116;
    }

    {
        char now_text[32];
        now_text[0] = '\0';
        if (a->m.pc >= 0 && a->m.pc < TOM_MEM) {
            const TomCell *now = &a->m.mem[a->m.pc];
            if (a->numbers && now->kind == CELL_CODE)
                snprintf(now_text, sizeof now_text, "%04d", tom_code_word(now->opcode, now->operand));
            else
                tom_format_source(now, now_text, sizeof now_text);
        }
        if (now_text[0] == '\0')
            snprintf(now_text, sizeof now_text, "empty");
        snprintf(reg, sizeof reg, "PC %02d", a->m.pc);
        tom_draw_text(a->ren, 16, 42, 2, reg, 0, 0, 0);
        snprintf(reg, sizeof reg, "ACC %d", a->m.acc);
        tom_draw_text(a->ren, 130, 42, 2, reg, 0, 0, 0);
        snprintf(reg, sizeof reg, "NOW %s", now_text);
        tom_draw_text(a->ren, 320, 42, 2, reg, 0, 0, 0);
        snprintf(speed, sizeof speed, "%d ms", SPEEDS[a->speed_index]);
        tom_draw_text(a->ren, 720, 48, 1, speed, COL_DIM.r, COL_DIM.g, COL_DIM.b);
        if (a->muted)
            tom_draw_text(a->ren, 780, 48, 1, "muted", COL_RED.r, COL_RED.g, COL_RED.b);
    }

    for (i = 0; i < TOM_MEM; i++)
        draw_cell(a, i);

    printer.x = grid_right() + 12;
    printer.y = GRID_Y;
    printer.w = WIN_W - printer.x - 16;
    printer.h = grid_bottom() - GRID_Y;
    draw_printer(a, printer);

    x = 16;
    for (i = 0; i < 13; i++) {
        SDL_Rect b;
        b.x = x;
        b.y = op_y;
        b.w = 70;
        b.h = 26;
        draw_button(a, b, ACT_OP_BASE + opcodes[i], ops[i]);
        x += 74;
    }
    {
        SDL_Rect b;
        b.x = x;
        b.y = op_y;
        b.w = 70;
        b.h = 26;
        draw_button(a, b, ACT_DAT, "DAT");
    }

    edit.x = 16;
    edit.y = edit_y;
    edit.w = WIN_W - 32;
    edit.h = 26;
    draw_edit(a, edit);

    {
        struct {
            int id;
            const char *label;
            int w;
        } row[] = {
            {ACT_RUN, "Run F5", 90},
            {ACT_STEP, "Step F8", 90},
            {ACT_STOP, "Stop", 70},
            {ACT_RESET, "Reset F2", 100},
            {ACT_VIEW, a->numbers ? "Mnemonics" : "Numbers", 110},
            {ACT_SLOWER, "Slower", 80},
            {ACT_FASTER, "Faster", 80},
            {ACT_SOUND, a->muted ? "Sound" : "Mute", 70},
            {ACT_CLEAR, "Clear", 70},
        };
        x = 16;
        for (i = 0; i < (int)(sizeof row / sizeof row[0]); i++) {
            SDL_Rect b;
            b.x = x;
            b.y = ctrl_y;
            b.w = row[i].w;
            b.h = 26;
            draw_button(a, b, row[i].id, row[i].label);
            x += row[i].w + 6;
        }
    }

    tom_draw_text(a->ren, 16, WIN_H - 18, 1, a->status,
                  a->status_bad ? COL_RED.r : 0,
                  a->status_bad ? COL_RED.g : 0,
                  a->status_bad ? COL_RED.b : 0);

    if (a->input_mode)
        draw_modal(a);
}

static int button_at(App *a, int x, int y)
{
    int i;
    SDL_Point p;
    p.x = x;
    p.y = y;
    for (i = a->button_count - 1; i >= 0; i--) {
        if (SDL_PointInRect(&p, &a->buttons[i].r))
            return a->buttons[i].id;
    }
    return 0;
}

static int cell_at(int x, int y)
{
    int col;
    int row;
    int gx = x - GRID_X;
    int gy = y - GRID_Y;
    if (gx < 0 || gy < 0)
        return -1;
    col = gx / (CELL_W + CELL_GAP);
    row = gy / (CELL_H + CELL_GAP);
    if (col > 9 || row > 9)
        return -1;
    if (gx % (CELL_W + CELL_GAP) >= CELL_W)
        return -1;
    if (gy % (CELL_H + CELL_GAP) >= CELL_H)
        return -1;
    return row * 10 + col;
}

static void append_edit(App *a, const char *text)
{
    size_t n;
    size_t add;
    if (a->running || a->input_mode || !text)
        return;
    n = strlen(a->edit);
    add = strlen(text);
    if (n + add > EDIT_MAX)
        return;
    memcpy(a->edit + n, text, add + 1);
}

static void append_input_text(App *a, const char *text)
{
    size_t n;
    if (!a->input_mode || !text || text[0] == '\0')
        return;
    n = strlen(a->input);
    if (text[0] == '-' && n == 0 && n + 1 < sizeof a->input) {
        a->input[0] = '-';
        a->input[1] = '\0';
        return;
    }
    if (text[0] >= '0' && text[0] <= '9' && n + 1 < sizeof a->input) {
        a->input[n] = text[0];
        a->input[n + 1] = '\0';
    }
}

static int save_surface(SDL_Surface *surface, const char *path)
{
    return surface && SDL_SaveBMP(surface, path) == 0;
}

static void describe(const App *a, const char *tag)
{
    int i;
    printf("%s pc=%02d acc=%d outs=", tag, a->m.pc, a->m.acc);
    for (i = 0; i < a->m.output_count; i++) {
        if (i)
            printf(",");
        printf("%d", a->m.output[i]);
    }
    if (a->m.fault)
        printf(" fault=%s", a->m.message);
    if (a->m.halted)
        printf(" halted");
    printf("\n");
}

int tom_ui_main(const char *sample, const char *snap_start, const char *snap_mid)
{
    App app;
    int snap = snap_start && snap_mid;
    Uint32 flags;

    memset(&app, 0, sizeof app);
    app.speed_index = 2;
    app.selected = 0;
    app.m.last_write = -1;

    flags = SDL_INIT_VIDEO;
    if (!snap)
        flags |= SDL_INIT_AUDIO;
    if (SDL_Init(flags) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    /* Snapshots draw into a software surface. Reading the window's back
       buffer is a frame behind on this Mac, so the picture would be stale. */
    if (snap) {
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, WIN_W, WIN_H, 24, SDL_PIXELFORMAT_RGB24);
        int guard = 400;
        if (!surface) {
            fprintf(stderr, "could not create snapshot surface\n");
            SDL_Quit();
            return 1;
        }
        app.ren = SDL_CreateSoftwareRenderer(surface);
        if (!app.ren) {
            fprintf(stderr, "could not create software renderer\n");
            SDL_FreeSurface(surface);
            SDL_Quit();
            return 1;
        }
        if (!load_into(&app, sample && sample[0] ? sample : "countup.tom")) {
            fprintf(stderr, "could not load sample\n");
            SDL_DestroyRenderer(app.ren);
            SDL_FreeSurface(surface);
            SDL_Quit();
            return 1;
        }
        draw_all(&app);
        if (!save_surface(surface, snap_start))
            fprintf(stderr, "could not write %s\n", snap_start);
        describe(&app, "start");
        while (guard-- && !app.m.halted && !app.m.fault && app.m.output_count < 2) {
            if (app.m.waiting_input)
                break;
            tom_step(&app.m);
        }
        if (!app.m.halted && !app.m.fault && !app.m.waiting_input)
            tom_step(&app.m);
        draw_all(&app);
        if (!save_surface(surface, snap_mid))
            fprintf(stderr, "could not write %s\n", snap_mid);
        describe(&app, "mid");
        SDL_DestroyRenderer(app.ren);
        SDL_FreeSurface(surface);
        SDL_Quit();
        return 0;
    }

    audio_init();
    app.win = SDL_CreateWindow("TOM",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!app.win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        audio_shutdown();
        SDL_Quit();
        return 1;
    }
    app.ren = SDL_CreateRenderer(app.win, -1, SDL_RENDERER_ACCELERATED);
    if (!app.ren)
        app.ren = SDL_CreateRenderer(app.win, -1, SDL_RENDERER_SOFTWARE);
    if (!app.ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(app.win);
        audio_shutdown();
        SDL_Quit();
        return 1;
    }

    if (!load_into(&app, sample && sample[0] ? sample : "countup.tom")) {
        set_status(&app, "No sample file found next to the program. Type a line and press Enter.", 1);
        tom_clear(&app.m);
    }

    SDL_StartTextInput();

    {
        int alive = 1;
        while (alive) {
            SDL_Event ev;
            int swallow_text = 0;
            Uint32 now;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT)
                    alive = 0;
                else if (ev.type == SDL_DROPFILE) {
                    load_into(&app, ev.drop.file);
                    SDL_free(ev.drop.file);
                } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                    int id = button_at(&app, ev.button.x, ev.button.y);
                    int addr;
                    if (id == ACT_KEY_BASE + 311)
                        submit_input(&app);
                    else if (id)
                        apply_action(&app, id);
                    else if (!app.input_mode && (addr = cell_at(ev.button.x, ev.button.y)) >= 0) {
                        app.selected = addr;
                        refresh_edit_from_cell(&app);
                    }
                } else if (ev.type == SDL_KEYDOWN) {
                    SDL_Keycode key = ev.key.keysym.sym;
                    int command = 0;
                    if (ev.key.repeat && key != SDLK_BACKSPACE)
                        continue;
                    if (app.input_mode) {
                        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                            submit_input(&app);
                            command = 1;
                        } else if (key == SDLK_BACKSPACE) {
                            size_t n = strlen(app.input);
                            if (n > 0)
                                app.input[n - 1] = '\0';
                            command = 1;
                        } else if (key == SDLK_ESCAPE) {
                            cmd_stop(&app);
                            command = 1;
                        }
                    } else if (key == SDLK_F5) {
                        cmd_run(&app);
                        command = 1;
                    } else if (key == SDLK_F8) {
                        cmd_step(&app);
                        command = 1;
                    } else if (key == SDLK_F2) {
                        cmd_reset(&app);
                        command = 1;
                    } else if (key == SDLK_ESCAPE) {
                        cmd_stop(&app);
                        command = 1;
                    } else if (key == SDLK_F3) {
                        apply_action(&app, ACT_VIEW);
                        command = 1;
                    } else if (key == SDLK_F6) {
                        change_speed(&app, -1);
                        command = 1;
                    } else if (key == SDLK_F7) {
                        change_speed(&app, +1);
                        command = 1;
                    } else if (key == SDLK_F10) {
                        apply_action(&app, ACT_SOUND);
                        command = 1;
                    } else if ((ev.key.keysym.mod & KMOD_CTRL) && key >= SDLK_1 && key <= SDLK_4) {
                        apply_action(&app, ACT_SAMPLE_BASE + (key - SDLK_1 + 1));
                        command = 1;
                    } else if (app.edit[0] == '\0' && !app.running && key >= SDLK_1 && key <= SDLK_4) {
                        apply_action(&app, ACT_SAMPLE_BASE + (key - SDLK_1 + 1));
                        command = 1;
                    } else if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
                        store_edit(&app);
                        command = 1;
                    } else if (key == SDLK_BACKSPACE) {
                        size_t n = strlen(app.edit);
                        if (n > 0)
                            app.edit[n - 1] = '\0';
                        command = 1;
                    }
                    /* A command key also produces a text event ("1", Enter).
                       Swallow that so it is not typed into the edit line as well. */
                    if (command)
                        swallow_text = 1;
                } else if (ev.type == SDL_TEXTINPUT) {
                    if (swallow_text)
                        swallow_text = 0;
                    else if (app.input_mode)
                        append_input_text(&app, ev.text.text);
                    else
                        append_edit(&app, ev.text.text);
                }
            }

            now = SDL_GetTicks();
            if (app.running && !app.input_mode && now >= app.next_step) {
                perform_step(&app, 1);
                app.next_step = now + (Uint32)SPEEDS[app.speed_index];
            }

            draw_all(&app);
            SDL_RenderPresent(app.ren);
            SDL_Delay(10);
        }
    }

    SDL_StopTextInput();
    SDL_DestroyRenderer(app.ren);
    SDL_DestroyWindow(app.win);
    audio_shutdown();
    SDL_Quit();
    return 0;
}
