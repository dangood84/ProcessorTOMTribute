#ifndef TOM_H
#define TOM_H

#include <stddef.h>

/* One hundred cells, addressed 00–99, drawn as a 10×10 grid.
   That is the whole machine: program and data share the same boxes. */
#define TOM_MEM 100
#define TOM_OUT_MAX 128

/* Operator numbers from the seminar sheet.
   The assembled word is operator × 100 + address, so LDA 24 is 0124.
   INP and OUT have no address; they are stored as 1100 and 1200 so they
   are not mistaken for LDA 0 (0100) or for a small piece of data.
   HLT is 0. An empty cell is also numerically 0 — the cell kind tells
   them apart, which is why the grid can show a blank and a real HLT. */
enum {
    OP_HLT = 0,
    OP_LDA = 1,
    OP_STO = 2,
    OP_ADD = 3,   /* AC+ */
    OP_SUB = 4,   /* AC- */
    OP_MUL = 5,   /* ACX */
    OP_DIV = 6,   /* AC\ */
    OP_JMP = 7,
    OP_JMN = 8,   /* JM-  jump if accumulator is negative */
    OP_JMZ = 9,   /* JM0  jump if accumulator is zero */
    OP_JIND = 10, /* J()  jump to the address stored in a cell */
    OP_INP = 11,
    OP_OUT = 12
};

typedef enum {
    CELL_EMPTY = 0,
    CELL_CODE,
    CELL_DATA
} TomKind;

typedef enum {
    STEP_OK = 0,
    STEP_HALTED,
    STEP_NEED_INPUT,
    STEP_FAULT
} TomStep;

typedef struct TomCell {
    TomKind kind;
    int opcode;
    int operand;
    int value; /* DAT contents. Code cells keep the assembled word here too. */
} TomCell;

typedef struct TomMachine {
    TomCell mem[TOM_MEM];
    int pc;
    int acc;
    int ir_op;
    int ir_operand;
    int ir_valid;
    int halted;         /* stopped because HLT ran */
    int waiting_input;  /* sitting on INP until the keypad answers */
    int fault;
    char message[160];
    int output[TOM_OUT_MAX];
    int output_count;
    int last_write;     /* cell STO just changed, or -1 */
    int steps;
} TomMachine;

void tom_clear(TomMachine *m);
void tom_reset_cpu(TomMachine *m);
int tom_code_word(int opcode, int operand);
const char *tom_mnemonic(int opcode);
void tom_format_source(const TomCell *cell, char *buf, size_t n);
void tom_format_parts(const TomCell *cell, int numbers,
                      char *top, size_t top_n, char *bot, size_t bot_n);

TomStep tom_step(TomMachine *m);
void tom_supply_input(TomMachine *m, int value);

int tom_assemble(TomMachine *m, const char *source, char *err, size_t err_n);
int tom_load_path(TomMachine *m, const char *path, char *err, size_t err_n);
int tom_parse_cell(const char *line, TomCell *out, char *err, size_t err_n);
int tom_load_named(TomMachine *m, const char *name, char *err, size_t err_n,
                   char *used, size_t used_n);

void audio_init(void);
void audio_shutdown(void);
void audio_click(void);
void audio_horn(void);

int tom_ui_main(const char *sample, const char *snap_start, const char *snap_mid);
int tom_run_tests(const char *examples_dir);

#endif
