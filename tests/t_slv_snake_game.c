/*
 * t_slv_snake_game.c -- gate test for games/wubu_snake.lua.
 *
 * Loads the self-playing Snake game through the real slerm-Lua host and
 * asserts the rendered frame: walls, a snake head + body, food, score/HUD,
 * and no Lua error. This turns the game into a verified artifact rather than
 * a demo that only a human looks at -- it proves the interpreter can run a
 * real multi-tick Lua program with tables, closures, and math.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slv_ui.h"

static int fails = 0;
static void ok(int cond, const char *what) {
    if (!cond) { printf("  FAIL %s\n", what); fails++; }
}

int main(void) {
    /* Use the same default geometry the demo harness uses. */
    slv_ui *ui = slv_ui_new(60, 20);
    if (!ui) { printf("FAIL slv_ui_new\n"); return 1; }

    /* Read the game source from disk (the harness is run from the repo root). */
    FILE *f = fopen("games/wubu_snake.lua", "rb");
    if (!f) { printf("FAIL cannot open games/wubu_snake.lua (run from repo root)\n"); slv_ui_free(ui); return 1; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *src = (char *)malloc((size_t)n + 1);
    size_t got = fread(src, 1, (size_t)n, f); src[got] = '\0'; fclose(f);

    const char *err = slv_ui_run(ui, src, "=wubu_snake");
    ok(err == NULL, "no Lua error running wubu_snake.lua");
    if (err) printf("  error: %s\n", err);

    /* Walls: top row and bottom border are drawn with '-'. */
    ok(slv_ui_cell(ui, 0, 1) == '-', "top wall drawn");
    ok(slv_ui_cell(ui, 0, 18) == '-', "bottom wall drawn");

    /* Count snake head '@', body 'o', and food '*' across the whole board. */
    int heads = 0, body = 0, food = 0;
    for (int y = 2; y <= 17; y++) {
        for (int x = 1; x <= 58; x++) {
            char c = slv_ui_cell(ui, x, y);
            if (c == '@') heads++;
            else if (c == 'o') body++;
            else if (c == '*') food++;
        }
    }
    ok(heads == 1, "exactly one snake head");
    ok(body >= 3, "snake body has segments");
    ok(food >= 1, "food is on the board");

    /* HUD: bottom row must contain a score label. */
    const char *hud = slv_ui_line(ui, 19);
    ok(hud != NULL && strstr(hud, "score") != NULL, "HUD shows score");

    free(src);
    slv_ui_free(ui);
    if (fails == 0) printf("PASS t_slv_snake_game  (self-play snake rendered, 7 checks)\n");
    return fails ? 1 : 0;
}
