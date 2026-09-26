#include "tom.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LABELS 64
#define LABEL_LEN 32

typedef struct Label {
    char name[LABEL_LEN];
    int addr;
} Label;

typedef struct OpName {
    const char *name;
    int opcode; /* OP_* , or -1 for DAT, or -2 for ORG */
    int needs_operand;
} OpName;

static const OpName OP_NAMES[] = {
    {"HLT", OP_HLT, 0},
    {"HALT", OP_HLT, 0},
    {"LDA", OP_LDA, 1},
    {"LOAD", OP_LDA, 1},
    {"STO", OP_STO, 1},
    {"STA", OP_STO, 1},
    {"STORE", OP_STO, 1},
    {"AC+", OP_ADD, 1},
    {"ADD", OP_ADD, 1},
    {"AC-", OP_SUB, 1},
    {"SUB", OP_SUB, 1},
    {"ACX", OP_MUL, 1},
    {"AC*", OP_MUL, 1},
    {"MUL", OP_MUL, 1},
    {"AC\\", OP_DIV, 1},
    {"AC/", OP_DIV, 1},
    {"DIV", OP_DIV, 1},
    {"JMP", OP_JMP, 1},
    {"JUMP", OP_JMP, 1},
    {"JM-", OP_JMN, 1},
    {"JMN", OP_JMN, 1},
    {"JMI", OP_JMN, 1},
    {"JM0", OP_JMZ, 1},
    {"JMZ", OP_JMZ, 1},
    {"JZ", OP_JMZ, 1},
    {"J()", OP_JIND, 1},
    {"JIND", OP_JIND, 1},
    {"INP", OP_INP, 0},
    {"IN", OP_INP, 0},
    {"OUT", OP_OUT, 0},
    {"DAT", -1, 1},
    {"DATA", -1, 1},
    {"ORG", -2, 1},
};

static int icmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a++);
        int cb = tolower((unsigned char)*b++);
        if (ca != cb)
            return ca - cb;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static const OpName *lookup_op(const char *token)
{
    size_t i;
    for (i = 0; i < sizeof OP_NAMES / sizeof OP_NAMES[0]; i++) {
        if (icmp(token, OP_NAMES[i].name) == 0)
            return &OP_NAMES[i];
    }
    return NULL;
}

static void trim_inplace(char *s)
{
    char *start = s;
    char *end;
    while (*start == ' ' || *start == '\t' || *start == '\r')
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        *--end = '\0';
}

static int next_token(const char **pp, char *buf, size_t n)
{
    const char *p = *pp;
    size_t i = 0;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0') {
        buf[0] = '\0';
        *pp = p;
        return 0;
    }
    while (*p && *p != ' ' && *p != '\t') {
        if (i + 1 < n)
            buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';
    *pp = p;
    return 1;
}

static int parse_number(const char *s, int *out)
{
    char *end = NULL;
    long v;
    if (!s || !*s)
        return 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0')
        return 0;
    if (v > 1000000L || v < -1000000L)
        return 0;
    *out = (int)v;
    return 1;
}

static int find_label(const Label *labels, int count, const char *name)
{
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].addr;
    }
    return -1;
}

static int add_label(Label *labels, int *count, const char *name, int addr,
                     char *err, size_t err_n, int line_no)
{
    if (name[0] == '\0' || strlen(name) >= LABEL_LEN) {
        snprintf(err, err_n, "Line %d: label is missing or too long", line_no);
        return 0;
    }
    if (find_label(labels, *count, name) >= 0) {
        snprintf(err, err_n, "Line %d: label '%s' is used twice", line_no, name);
        return 0;
    }
    if (*count >= MAX_LABELS) {
        snprintf(err, err_n, "Line %d: too many labels", line_no);
        return 0;
    }
    snprintf(labels[*count].name, LABEL_LEN, "%s", name);
    labels[*count].addr = addr;
    (*count)++;
    return 1;
}

static int resolve_operand(const char *token, const Label *labels, int label_count,
                           int *out, char *err, size_t err_n, int line_no, int want_addr)
{
    int value;
    if (parse_number(token, &value)) {
        if (want_addr && (value < 0 || value >= TOM_MEM)) {
            snprintf(err, err_n, "Line %d: address %d is outside 00–99", line_no, value);
            return 0;
        }
        *out = value;
        return 1;
    }
    value = find_label(labels, label_count, token);
    if (value < 0) {
        snprintf(err, err_n, "Line %d: unknown label '%s'", line_no, token);
        return 0;
    }
    *out = value;
    return 1;
}

/* Two walks, because "JMP done" is allowed to appear before "done:".
   The first walk only records labels and how many cells each line uses.
   The second walk writes the cells, when every name already has an address. */
int tom_assemble(TomMachine *m, const char *source, char *err, size_t err_n)
{
    Label labels[MAX_LABELS];
    int label_count = 0;
    int pass;

    tom_clear(m);
    err[0] = '\0';

    for (pass = 1; pass <= 2; pass++) {
        const char *p = source;
        int pc = 0;
        int line_no = 0;

        while (*p) {
            char line[256];
            char token0[64], token1[64], token2[64];
            const char *cursor;
            const OpName *op;
            size_t len = 0;
            int operand = 0;

            line_no++;
            while (p[len] && p[len] != '\n' && len + 1 < sizeof line)
                len++;
            memcpy(line, p, len);
            line[len] = '\0';
            p += len;
            if (*p == '\n')
                p++;

            {
                char *semi = strchr(line, ';');
                if (semi)
                    *semi = '\0';
            }
            trim_inplace(line);
            if (line[0] == '\0')
                continue;

            /* A line is one of:
                 LDA 24
                 start LDA answer
                 done:
                 done: HLT
               The label is optional and may share the line with the operator. */
            cursor = line;
            token0[0] = token1[0] = token2[0] = '\0';
            next_token(&cursor, token0, sizeof token0);
            next_token(&cursor, token1, sizeof token1);
            next_token(&cursor, token2, sizeof token2);
            if (token0[0] == '\0')
                continue;
            {
                char extra[64];
                if (next_token(&cursor, extra, sizeof extra)) {
                    snprintf(err, err_n, "Line %d: extra text '%s'", line_no, extra);
                    return 0;
                }
            }

            if (token0[strlen(token0) - 1] == ':')
                token0[strlen(token0) - 1] = '\0';

            if (!lookup_op(token0)) {
                /* First word is a name, not an operator: "start LDA answer". */
                if (pass == 1 && !add_label(labels, &label_count, token0, pc, err, err_n, line_no))
                    return 0;
                if (token1[0] == '\0')
                    continue;
                memcpy(token0, token1, sizeof token0);
                memcpy(token1, token2, sizeof token1);
                token2[0] = '\0';
            } else if (token2[0] != '\0') {
                snprintf(err, err_n, "Line %d: extra text '%s'", line_no, token2);
                return 0;
            }

            op = lookup_op(token0);
            if (!op) {
                snprintf(err, err_n, "Line %d: unknown operator '%s'", line_no, token0);
                return 0;
            }
            if (op->needs_operand && token1[0] == '\0') {
                snprintf(err, err_n, "Line %d: %s needs an address or a number", line_no, token0);
                return 0;
            }
            if (!op->needs_operand && token1[0] != '\0') {
                snprintf(err, err_n, "Line %d: %s does not take an address", line_no, token0);
                return 0;
            }

            if (op->opcode == -2) {
                if (!resolve_operand(token1, labels, label_count, &operand, err, err_n, line_no, 1))
                    return 0;
                pc = operand;
                continue;
            }

            if (pc < 0 || pc >= TOM_MEM) {
                snprintf(err, err_n, "Line %d: cell %d is outside the grid", line_no, pc);
                return 0;
            }

            if (pass == 2) {
                TomCell *cell = &m->mem[pc];
                if (op->opcode == -1) {
                    if (!resolve_operand(token1, labels, label_count, &operand, err, err_n, line_no, 0))
                        return 0;
                    cell->kind = CELL_DATA;
                    cell->value = operand;
                } else {
                    if (op->needs_operand &&
                        !resolve_operand(token1, labels, label_count, &operand, err, err_n, line_no, 1))
                        return 0;
                    cell->kind = CELL_CODE;
                    cell->opcode = op->opcode;
                    cell->operand = operand;
                    cell->value = tom_code_word(op->opcode, operand);
                }
            }
            pc++;
        }
    }
    return 1;
}

int tom_load_path(TomMachine *m, const char *path, char *err, size_t err_n)
{
    FILE *fp;
    char *buf;
    long len;
    int ok;

    fp = fopen(path, "rb");
    if (!fp) {
        snprintf(err, err_n, "Could not open %s", path);
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        snprintf(err, err_n, "Could not read %s", path);
        return 0;
    }
    len = ftell(fp);
    if (len < 0 || len > 65536) {
        fclose(fp);
        snprintf(err, err_n, "%s is empty or too long", path);
        return 0;
    }
    rewind(fp);
    buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        snprintf(err, err_n, "Out of memory");
        return 0;
    }
    if (fread(buf, 1, (size_t)len, fp) != (size_t)len) {
        free(buf);
        fclose(fp);
        snprintf(err, err_n, "Could not read %s", path);
        return 0;
    }
    fclose(fp);
    buf[len] = '\0';
    ok = tom_assemble(m, buf, err, err_n);
    free(buf);
    return ok;
}

int tom_parse_cell(const char *line, TomCell *out, char *err, size_t err_n)
{
    TomMachine tmp;
    char source[160];
    char tok[64];
    const char *p = line;
    const char *cursor;

    while (*p == ' ' || *p == '\t')
        p++;
    cursor = p;
    next_token(&cursor, tok, sizeof tok);
    if (icmp(tok, "ORG") == 0) {
        snprintf(err, err_n, "ORG belongs in a program file, not in one cell");
        return 0;
    }
    /* A cell editor line is a one-line program. Assemble it and keep cell 00. */
    snprintf(source, sizeof source, "%s\n", line);
    if (!tom_assemble(&tmp, source, err, err_n))
        return 0;
    if (tmp.mem[0].kind == CELL_EMPTY) {
        snprintf(err, err_n, "Type an operator, such as LDA 24, or DAT 55");
        return 0;
    }
    *out = tmp.mem[0];
    return 1;
}

int tom_load_named(TomMachine *m, const char *name, char *err, size_t err_n,
                   char *used, size_t used_n)
{
    char path[512];
    const char *tries[3];
    int i;

    if (strchr(name, '/') || strchr(name, '\\')) {
        if (!tom_load_path(m, name, err, err_n))
            return 0;
        if (used && used_n)
            snprintf(used, used_n, "%s", name);
        return 1;
    }

    tries[0] = "examples/%s";
    tries[1] = "../examples/%s";
    tries[2] = "%s";
    for (i = 0; i < 3; i++) {
        snprintf(path, sizeof path, tries[i], name);
        if (tom_load_path(m, path, err, err_n)) {
            if (used && used_n)
                snprintf(used, used_n, "%s", path);
            return 1;
        }
    }
    return 0;
}
