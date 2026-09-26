#include "tom.h"

#include <stdio.h>
#include <string.h>

void tom_clear(TomMachine *m)
{
    memset(m, 0, sizeof *m);
    m->last_write = -1;
}

void tom_reset_cpu(TomMachine *m)
{
    /* The program stays in the grid. Only the counters and the printer clear,
       which is what Reset did between runs in class. */
    m->pc = 0;
    m->acc = 0;
    m->ir_op = 0;
    m->ir_operand = 0;
    m->ir_valid = 0;
    m->halted = 0;
    m->waiting_input = 0;
    m->fault = 0;
    m->message[0] = '\0';
    m->output_count = 0;
    m->last_write = -1;
    m->steps = 0;
}

int tom_code_word(int opcode, int operand)
{
    if (opcode == OP_HLT)
        return 0;
    if (opcode == OP_INP)
        return 1100;
    if (opcode == OP_OUT)
        return 1200;
    return opcode * 100 + operand;
}

const char *tom_mnemonic(int opcode)
{
    switch (opcode) {
    case OP_HLT: return "HLT";
    case OP_LDA: return "LDA";
    case OP_STO: return "STO";
    case OP_ADD: return "AC+";
    case OP_SUB: return "AC-";
    case OP_MUL: return "ACX";
    case OP_DIV: return "AC\\";
    case OP_JMP: return "JMP";
    case OP_JMN: return "JM-";
    case OP_JMZ: return "JM0";
    case OP_JIND: return "J()";
    case OP_INP: return "INP";
    case OP_OUT: return "OUT";
    default: return "?";
    }
}

void tom_format_source(const TomCell *cell, char *buf, size_t n)
{
    if (n == 0)
        return;
    buf[0] = '\0';
    if (cell->kind == CELL_DATA) {
        snprintf(buf, n, "DAT %d", cell->value);
        return;
    }
    if (cell->kind != CELL_CODE)
        return;
    if (cell->opcode == OP_HLT || cell->opcode == OP_INP || cell->opcode == OP_OUT)
        snprintf(buf, n, "%s", tom_mnemonic(cell->opcode));
    else
        snprintf(buf, n, "%s %d", tom_mnemonic(cell->opcode), cell->operand);
}

void tom_format_parts(const TomCell *cell, int numbers,
                      char *top, size_t top_n, char *bot, size_t bot_n)
{
    if (top_n)
        top[0] = '\0';
    if (bot_n)
        bot[0] = '\0';

    if (cell->kind == CELL_EMPTY)
        return;

    if (cell->kind == CELL_DATA) {
        if (numbers)
            snprintf(top, top_n, "%d", cell->value);
        else {
            snprintf(top, top_n, "DAT");
            snprintf(bot, bot_n, "%d", cell->value);
        }
        return;
    }

    if (numbers) {
        snprintf(top, top_n, "%04d", tom_code_word(cell->opcode, cell->operand));
        return;
    }

    snprintf(top, top_n, "%s", tom_mnemonic(cell->opcode));
    if (cell->opcode != OP_HLT && cell->opcode != OP_INP && cell->opcode != OP_OUT)
        snprintf(bot, bot_n, "%02d", cell->operand);
}

static void tom_fault(TomMachine *m, const char *text)
{
    m->fault = 1;
    m->waiting_input = 0;
    snprintf(m->message, sizeof m->message, "%s", text);
}

static int tom_read(const TomMachine *m, int addr, int *out)
{
    const TomCell *cell;

    if (addr < 0 || addr >= TOM_MEM)
        return 0;
    cell = &m->mem[addr];
    if (cell->kind == CELL_DATA)
        *out = cell->value;
    else if (cell->kind == CELL_CODE)
        *out = cell->value;
    else
        *out = 0;
    return 1;
}

static int tom_check_addr(TomMachine *m, int addr, const char *what)
{
    if (addr >= 0 && addr < TOM_MEM)
        return 1;
    snprintf(m->message, sizeof m->message,
             "%s address %d is outside the grid (00–99)", what, addr);
    m->fault = 1;
    return 0;
}

TomStep tom_step(TomMachine *m)
{
    TomCell *cell;
    int value;
    int addr;

    if (m->fault)
        return STEP_FAULT;
    if (m->halted)
        return STEP_HALTED;
    /* INP does not move the program counter until a number arrives,
       so a second step must not fetch INP again and click twice. */
    if (m->waiting_input)
        return STEP_NEED_INPUT;
    if (!tom_check_addr(m, m->pc, "Program counter"))
        return STEP_FAULT;

    cell = &m->mem[m->pc];
    m->last_write = -1;

    if (cell->kind != CELL_CODE) {
        if (cell->kind == CELL_EMPTY)
            snprintf(m->message, sizeof m->message,
                     "Cell %02d is empty — there is no instruction to carry out", m->pc);
        else
            snprintf(m->message, sizeof m->message,
                     "Cell %02d holds the number %d, not an instruction", m->pc, cell->value);
        m->fault = 1;
        return STEP_FAULT;
    }

    m->ir_valid = 1;
    m->ir_op = cell->opcode;
    m->ir_operand = cell->operand;
    m->steps++;

    switch (cell->opcode) {
    case OP_HLT:
        /* Leave the program counter on the HLT itself.
           After the seminar's first program the counter showed 2,
           which is the address of HLT, not the cell after it. */
        m->halted = 1;
        snprintf(m->message, sizeof m->message, "Halted at %02d", m->pc);
        return STEP_HALTED;

    case OP_LDA:
        if (!tom_check_addr(m, cell->operand, "LDA"))
            return STEP_FAULT;
        tom_read(m, cell->operand, &value);
        m->acc = value;
        break;

    case OP_STO:
        if (!tom_check_addr(m, cell->operand, "STO"))
            return STEP_FAULT;
        /* STO writes a number. The cell stops being an instruction. */
        m->mem[cell->operand].kind = CELL_DATA;
        m->mem[cell->operand].opcode = 0;
        m->mem[cell->operand].operand = 0;
        m->mem[cell->operand].value = m->acc;
        m->last_write = cell->operand;
        break;

    case OP_ADD:
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
        if (!tom_check_addr(m, cell->operand, tom_mnemonic(cell->opcode)))
            return STEP_FAULT;
        tom_read(m, cell->operand, &value);
        if (cell->opcode == OP_DIV && value == 0) {
            snprintf(m->message, sizeof m->message,
                     "Divide by zero at cell %02d", m->pc);
            m->fault = 1;
            return STEP_FAULT;
        }
        if (cell->opcode == OP_ADD)
            m->acc += value;
        else if (cell->opcode == OP_SUB)
            m->acc -= value;
        else if (cell->opcode == OP_MUL)
            m->acc *= value;
        else
            m->acc /= value; /* truncates toward zero, same as C integer division */
        break;

    case OP_JMP:
        if (!tom_check_addr(m, cell->operand, "JMP"))
            return STEP_FAULT;
        m->pc = cell->operand;
        return STEP_OK;

    case OP_JMN:
        if (m->acc < 0) {
            if (!tom_check_addr(m, cell->operand, "JM-"))
                return STEP_FAULT;
            m->pc = cell->operand;
            return STEP_OK;
        }
        break;

    case OP_JMZ:
        if (m->acc == 0) {
            if (!tom_check_addr(m, cell->operand, "JM0"))
                return STEP_FAULT;
            m->pc = cell->operand;
            return STEP_OK;
        }
        break;

    case OP_JIND:
        /* The operand is not the destination. It is the cell whose
           number is the destination — the seminar's "jump indirect". */
        if (!tom_check_addr(m, cell->operand, "J()"))
            return STEP_FAULT;
        tom_read(m, cell->operand, &addr);
        if (!tom_check_addr(m, addr, "Indirect jump"))
            return STEP_FAULT;
        m->pc = addr;
        return STEP_OK;

    case OP_INP:
        m->waiting_input = 1;
        snprintf(m->message, sizeof m->message, "Keypad: waiting for a number");
        return STEP_NEED_INPUT;

    case OP_OUT:
        if (m->output_count < TOM_OUT_MAX)
            m->output[m->output_count++] = m->acc;
        break;

    default:
        tom_fault(m, "Unknown operator");
        return STEP_FAULT;
    }

    m->pc++;
    if (m->pc >= TOM_MEM) {
        tom_fault(m, "Program counter walked off the end of the grid");
        return STEP_FAULT;
    }
    return STEP_OK;
}

void tom_supply_input(TomMachine *m, int value)
{
    if (!m->waiting_input || m->fault)
        return;
    m->acc = value;
    m->waiting_input = 0;
    m->message[0] = '\0';
    m->pc++;
    if (m->pc >= TOM_MEM)
        tom_fault(m, "Program counter walked off the end of the grid");
}
