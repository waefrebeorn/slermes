/*
 * t_slv_ui_test.c -- the Lua-driven terminal UI host (src/lua/slv_ui.c).
 *
 * The point of this test: a Lua chunk, not C, decides what appears on screen.
 * The host owns the buffer; Lua only reaches it through the `term` table.
 */
#include <stdio.h>
#include <string.h>

#include "slv_ui.h"

static int fails = 0;

static void check(const char *what, int got, int want) {
    if (got != want) {
        printf("  FAIL %s: got %d want %d\n", what, got, want);
        fails++;
    }
}

static void check_ch(const char *what, char got, char want) {
    if (got != want) {
        printf("  FAIL %s: got '%c' want '%c'\n", what, got ? got : '?', want);
        fails++;
    }
}

int main(void) {
    slv_ui *ui = slv_ui_new(10, 4);
    if (!ui) { printf("FAIL slv_ui_new returned NULL\n"); return 1; }

    check("width",  slv_ui_width(ui),  10);
    check("height", slv_ui_height(ui), 4);

    /* A Lua chunk draws the whole screen. */
    const char *src =
        "term.clear()\n"
        "term.put(0, 0, 'hello')\n"
        "term.put(2, 2, 'world')\n"
        "term.put(8, 0, 'xyz')\n"      /* must clip at the right edge */
        "term.put(-5, 3, 'nope')\n"    /* must clip at the left edge */
        "term.put(0, 9, 'off')\n";     /* must clip at the bottom edge */

    const char *err = slv_ui_run(ui, src, "=ui_test");
    if (err) { printf("FAIL slv_ui_run: %s\n", err); fails++; }

    check_ch("row0 col0", slv_ui_cell(ui, 0, 0), 'h');
    check_ch("row0 col4", slv_ui_cell(ui, 4, 0), 'o');
    check_ch("row2 col2", slv_ui_cell(ui, 2, 2), 'w');
    check_ch("row2 col6", slv_ui_cell(ui, 6, 2), 'd');
    check_ch("row0 col8", slv_ui_cell(ui, 8, 0), 'x');
    check_ch("row0 col9", slv_ui_cell(ui, 9, 0), 'y');   /* 'z' clipped */
    check_ch("row1 col0", slv_ui_cell(ui, 0, 1), ' ');   /* untouched */
    check_ch("row3 col0", slv_ui_cell(ui, 0, 3), ' ');   /* negative x clipped */
    check_ch("row0 col7", slv_ui_cell(ui, 7, 0), ' ');

    /* term.get() must agree with the host's view of the buffer. */
    const char *src2 =
        "g1 = term.get(0, 0)\n"
        "g2 = term.get(2, 2)\n"
        "g3 = term.get(0, 1)\n";
    err = slv_ui_run(ui, src2, "=ui_get");
    if (err) { printf("FAIL slv_ui_run(get): %s\n", err); fails++; }

    slv *L = slv_ui_state(ui);
    slv_value v1 = slv_get_global(L, "g1");
    slv_value v2 = slv_get_global(L, "g2");
    slv_value v3 = slv_get_global(L, "g3");
    const char *s1 = slv_tostring(v1);
    const char *s2 = slv_tostring(v2);
    const char *s3 = slv_tostring(v3);
    check_ch("term.get(0,0)", s1 ? s1[0] : '?', 'h');
    check_ch("term.get(2,2)", s2 ? s2[0] : '?', 'w');
    check_ch("term.get(0,1)", s3 ? s3[0] : '?', ' ');

    /* fill() then a single put(). */
    const char *src3 =
        "term.fill('.')\n"
        "term.put(1, 1, 'X')\n";
    err = slv_ui_run(ui, src3, "=ui_fill");
    if (err) { printf("FAIL slv_ui_run(fill): %s\n", err); fails++; }
    check_ch("fill row0",  slv_ui_cell(ui, 0, 0), '.');
    check_ch("fill row3",  slv_ui_cell(ui, 9, 3), '.');
    check_ch("put over fill", slv_ui_cell(ui, 1, 1), 'X');

    /* A Lua runtime error must be reported, not crash the host. */
    const char *bad = "term.put(0, 0, nil)\nthis_is_a_syntax_error(\n";
    const char *err2 = slv_ui_run(ui, bad, "=ui_bad");
    check("syntax error reported", err2 != NULL, 1);

    /* Bounds: an out-of-range constructor must fail cleanly. */
    check("zero width rejected", slv_ui_new(0, 4) == NULL, 1);

    slv_ui_free(ui);

    if (fails) {
        printf("=== SLV-UI TESTS: %d fail ===\n", fails);
        return 1;
    }
    printf("=== SLV-UI TESTS: 22 pass, 0 fail ===\n");
    return 0;
}
