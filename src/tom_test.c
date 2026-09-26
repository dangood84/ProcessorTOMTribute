#include "tom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failed;

static void expect_true(const char *name, int cond)
{
    if (cond) {
        printf("ok  %s\n", name);
        return;
    }
    printf("FAIL %s\n", name);
    g_failed = 1;
}

static int load_example(TomMachine *m, const char *dir, const char *file, char *err, size_t n)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    if (tom_load_path(m, path, err, n))
        return 1;
    snprintf(path, sizeof path, "examples/%s", file);
    return tom_load_path(m, path, err, n);
}

static int run_program(TomMachine *m, const int *inputs, int ninputs)
{
    int used = 0;
    int guard = 10000;
    while (guard--) {
        TomStep step = tom_step(m);
        if (step == STEP_HALTED)
            return 1;
        if (step == STEP_FAULT)
            return 0;
        if (step == STEP_NEED_INPUT) {
            if (used >= ninputs)
                return 0;
            tom_supply_input(m, inputs[used++]);
        }
    }
    return 0;
}

static void test_wexampl1(const char *dir)
{
    TomMachine m;
    char err[160];
    int ok = load_example(&m, dir, "wexampl1.tom", err, sizeof err);
    expect_true("wexampl1 assembles", ok);
    if (!ok) {
        printf("    %s\n", err);
        return;
    }
    ok = run_program(&m, NULL, 0);
    expect_true("wexampl1 halts", ok && m.halted && !m.fault);
    /* The seminar sheet: after the run the counter shows 2,
       the accumulator shows 55, and 55 has been printed. */
    expect_true("wexampl1 pc is 2", m.pc == 2);
    expect_true("wexampl1 acc is 55", m.acc == 55);
    expect_true("wexampl1 prints 55", m.output_count == 1 && m.output[0] == 55);
}

static void test_countup(const char *dir)
{
    TomMachine m;
    char err[160];
    int ok = load_example(&m, dir, "countup.tom", err, sizeof err);
    int i;
    int expect;
    expect_true("countup assembles", ok);
    if (!ok) {
        printf("    %s\n", err);
        return;
    }
    ok = run_program(&m, NULL, 0);
    expect_true("countup halts", ok && m.halted && !m.fault);
    expect_true("countup prints ten fives", m.output_count == 10);
    expect = 5;
    ok = m.output_count == 10;
    for (i = 0; ok && i < 10; i++) {
        if (m.output[i] != expect)
            ok = 0;
        expect += 5;
    }
    expect_true("countup sequence 5..50", ok);
    expect_true("countup answer cell is 50", m.mem[20].kind == CELL_DATA && m.mem[20].value == 50);
    expect_true("countup counter cell is 0", m.mem[21].kind == CELL_DATA && m.mem[21].value == 0);
    expect_true("countup stops on HLT", m.pc == 9 && m.mem[9].opcode == OP_HLT);
}

static void test_subtract(const char *dir)
{
    TomMachine m;
    char err[160];
    int inputs[2];
    int ok = load_example(&m, dir, "subtract.tom", err, sizeof err);
    inputs[0] = 20;
    inputs[1] = 8;
    expect_true("subtract assembles", ok);
    if (!ok) {
        printf("    %s\n", err);
        return;
    }
    ok = run_program(&m, inputs, 2);
    expect_true("subtract halts", ok && m.halted);
    expect_true("subtract prints 12", m.output_count == 1 && m.output[0] == 12);
    expect_true("subtract acc is 12", m.acc == 12);
}

static void test_add(const char *dir)
{
    TomMachine m;
    char err[160];
    int inputs[2];
    int ok = load_example(&m, dir, "wexampl2.tom", err, sizeof err);
    inputs[0] = 7;
    inputs[1] = 4;
    expect_true("add assembles", ok);
    if (!ok) {
        printf("    %s\n", err);
        return;
    }
    ok = run_program(&m, inputs, 2);
    expect_true("add halts", ok && m.halted);
    expect_true("add prints 11", m.output_count == 1 && m.output[0] == 11);
}

static void test_indirect_and_faults(void)
{
    TomMachine m;
    char err[160];
    const char *indirect =
        "LDA four\n"
        "STO slot\n"
        "J() slot\n"
        "HLT\n"
        "HLT\n"
        "ORG 10\n"
        "four DAT 4\n"
        "slot DAT 0\n";
    const char *div0 =
        "LDA one\n"
        "AC\\ zero\n"
        "HLT\n"
        "ORG 10\n"
        "one DAT 8\n"
        "zero DAT 0\n";
    const char *neg =
        "LDA neg\n"
        "JM- there\n"
        "HLT\n"
        "there OUT\n"
        "HLT\n"
        "ORG 10\n"
        "neg DAT -7\n";
    int ok;

    ok = tom_assemble(&m, indirect, err, sizeof err) && run_program(&m, NULL, 0);
    expect_true("indirect jump lands on cell 04", ok && m.halted && m.pc == 4);

    ok = tom_assemble(&m, div0, err, sizeof err);
    ok = ok && !run_program(&m, NULL, 0) && m.fault;
    expect_true("divide by zero faults", ok);

    ok = tom_assemble(&m, neg, err, sizeof err) && run_program(&m, NULL, 0);
    expect_true("JM- taken prints -7", ok && m.halted && m.output_count == 1 && m.output[0] == -7);
}

int tom_run_tests(const char *examples_dir)
{
    const char *dir = examples_dir && examples_dir[0] ? examples_dir : "examples";
    g_failed = 0;
    test_wexampl1(dir);
    test_countup(dir);
    test_subtract(dir);
    test_add(dir);
    test_indirect_and_faults();
    if (g_failed) {
        printf("tests failed\n");
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
