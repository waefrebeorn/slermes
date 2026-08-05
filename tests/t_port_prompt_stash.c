/* t_port_prompt_stash.c — differential oracle driver for hermes_cli/prompt_stash.py.
 * Exercises the port; prints one compact JSON object per case for the Python
 * oracle to compare.
 */

#include "port_prompt_stash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jb(bool v) { return v ? "true" : "false"; }
/* minimal JSON string escaper for oracle output */
static void json_escape_print(const char *s) {
    fputc('"', stdout);
    for (const char *p = s; p && *p; p++) {
        switch (*p) {
        case '"': fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        default: fputc(*p, stdout);
        }
    }
    fputc('"', stdout);
}
static const char *act(ctrl_s_action_t a) {
    switch (a) {
    case CTRL_S_NOOP: return "noop";
    case CTRL_S_STASHED: return "stashed";
    case CTRL_S_RESTORED: return "restored";
    case CTRL_S_OPEN_PANEL: return "open_panel";
    case CTRL_S_CLOSE_PANEL: return "close_panel";
    }
    return "unknown";
}

/* deterministic clock */
static double clk_state;
static double fixed_clock(void) { return clk_state; }

static prompt_stash_t *mk(void) { return prompt_stash_new(PROMPT_STASH_MAX_ITEMS, fixed_clock); }

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    /* --- build_preview --- */
    {
        char *p1 = prompt_stash_build_preview("hello world", 60);
        char *p2 = prompt_stash_build_preview("line1\nline2\tend", 60);
        char *p3 = prompt_stash_build_preview("a very long draft that should be ellipsized past the width limit here", 20);
        printf("{\"case\":\"build_preview\",\"p1\":");
        json_escape_print(p1);
        printf(",\"p2\":");
        json_escape_print(p2);
        printf(",\"p3\":");
        json_escape_print(p3);
        printf("}\n");
        free(p1); free(p2); free(p3);
    }

    /* --- stash / len / pop / peek --- */
    {
        prompt_stash_t *ps = mk();
        clk_state = 1.0;
        bool s1 = prompt_stash_stash(ps, "first", NULL);
        clk_state = 2.0;
        bool s2 = prompt_stash_stash(ps, "second", NULL);
        printf("{\"case\":\"stash_basic\",\"s1\":%s,\"s2\":%s,\"len\":%zu,\"top_is_second\":%s}\n",
               jb(s1), jb(s2), prompt_stash_len(ps),
               jb(strcmp(prompt_stash_peek(ps, 0)->text, "second") == 0));
        char *t = NULL; json_t *im = NULL;
        bool popped = prompt_stash_pop(ps, 0, &t, &im);
        printf("{\"case\":\"pop\",\"popped\":%s,\"text\":%s,\"len_after\":%zu}\n",
               jb(popped), jb(strcmp(t, "second") == 0), prompt_stash_len(ps));
        free(t); if (im) json_free(im);
        prompt_stash_free(ps);
    }

    /* --- blank stash is no-op --- */
    {
        prompt_stash_t *ps = mk();
        bool s = prompt_stash_stash(ps, "   ", NULL);
        printf("{\"case\":\"stash_blank\",\"noop\":%s,\"len\":%zu}\n", jb(!s), prompt_stash_len(ps));
        prompt_stash_free(ps);
    }

    /* --- cap / trim --- */
    {
        prompt_stash_t *ps = prompt_stash_new(3, fixed_clock);
        clk_state = 0;
        for (int i = 0; i < 6; i++) {
            char buf[16]; snprintf(buf, sizeof buf, "d%d", i);
            clk_state += 1.0;
            prompt_stash_stash(ps, buf, NULL);
        }
        printf("{\"case\":\"cap\",\"len\":%zu,\"top\":%s}\n",
               prompt_stash_len(ps), jb(strcmp(prompt_stash_peek(ps, 0)->text, "d5") == 0));
        prompt_stash_free(ps);
    }

    /* --- panel cursor / delete / restore --- */
    {
        prompt_stash_t *ps = mk();
        for (int i = 0; i < 3; i++) { char b[8]; snprintf(b, sizeof b, "d%d", i); clk_state+=1; prompt_stash_stash(ps, b, NULL); }
        bool opened = prompt_stash_open_panel(ps);
        int c1 = prompt_stash_move_cursor(ps, 1);
        int c2 = prompt_stash_move_cursor(ps, 5); /* clamp */
        int c3 = prompt_stash_move_cursor(ps, -9); /* clamp to 0 */
        /* delete at cursor */
        size_t before = prompt_stash_len(ps);
        bool del = prompt_stash_delete_at_cursor(ps);
        printf("{\"case\":\"panel\",\"opened\":%s,\"c1\":%d,\"c2_clamped\":%d,\"c3_clamped\":%d,\"del\":%s,\"before\":%zu,\"after\":%zu}\n",
               jb(opened), c1, c2, c3, jb(del), before, prompt_stash_len(ps));
        prompt_stash_free(ps);
    }

    /* --- indicator / placeholder --- */
    {
        prompt_stash_t *ps = mk();
        char *ind0 = prompt_stash_indicator(ps);
        char *hint0 = prompt_stash_placeholder_hint(ps);
        prompt_stash_stash(ps, "draft A", NULL);
        char *ind1 = prompt_stash_indicator(ps);
        char *hint1 = prompt_stash_placeholder_hint(ps);
        prompt_stash_stash(ps, "draft B", NULL);
        char *hint2 = prompt_stash_placeholder_hint(ps);
        printf("{\"case\":\"indicator\",\"ind0\":%s,\"hint0\":%s,\"ind1\":%s,\"hint1_has_restore\":%s,\"hint2_has_browse\":%s}\n",
               jb(strcmp(ind0, "") == 0), jb(strcmp(hint0, "") == 0),
               jb(strcmp(ind1, "\xF0\x9F\x93\x8C 1") == 0),
               jb(strstr(hint1, "restore") != NULL),
               jb(strstr(hint2, "browse") != NULL));
        free(ind0); free(hint0); free(ind1); free(hint1); free(hint2);
        prompt_stash_free(ps);
    }

    /* --- resolve_ctrl_s gesture table --- */
    {
        prompt_stash_t *ps = mk();
        char *ot = NULL; json_t *oi = NULL;
        /* empty buffer, empty stash -> noop */
        ctrl_s_action_t a1 = prompt_stash_resolve_ctrl_s(ps, "   ", NULL, &ot, &oi);
        /* non-blank -> stash */
        ctrl_s_action_t a2 = prompt_stash_resolve_ctrl_s(ps, "hello", NULL, &ot, &oi);
        /* now 1 item, empty buffer -> restored */
        ctrl_s_action_t a3 = prompt_stash_resolve_ctrl_s(ps, "", NULL, &ot, &oi);
        bool restored_text = (ot && strcmp(ot, "hello") == 0);
        /* add two, then open panel */
        prompt_stash_stash(ps, "a", NULL);
        prompt_stash_stash(ps, "b", NULL);
        ctrl_s_action_t a4 = prompt_stash_resolve_ctrl_s(ps, "", NULL, &ot, &oi);
        bool panel_open = prompt_stash_bool(ps) && true;
        /* while open, ctrl_s closes */
        ctrl_s_action_t a5 = prompt_stash_resolve_ctrl_s(ps, "x", NULL, &ot, &oi);
        printf("{\"case\":\"gesture\",\"a1\":\"%s\",\"a2\":\"%s\",\"a3\":\"%s\",\"a3_text\":%s,\"a4\":\"%s\",\"panel_open\":%s,\"a5\":\"%s\"}\n",
               act(a1), act(a2), act(a3), jb(restored_text), act(a4), jb(panel_open), act(a5));
        prompt_stash_free(ps);
    }

    return 0;
}
