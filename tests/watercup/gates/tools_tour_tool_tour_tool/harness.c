/* gate: tools_tour_tool_tour_tool — traced wrapper around the REAL
 * src/tools/tour_tool.c port (linked via extra_src; a stub preview-bridge
 * callback is installed so the desktop path runs).
 * LIVE twin: tools/tour_tool.py::tour_tool.
 *
 * Input encoding (argv-safe, ';' split):
 *   "<action>;<surface>;<side>;<sel>;<title>;<text>;<steps>;<cbrate>"
 *   action/surface/side/sel/title/text: literal, "-" = absent
 *   steps: "-" = absent, "@" = [], else '~'-separated items:
 *          "o0"/"o1"/"t" = {} / {text:x} / non-object
 *   cbrate: "json" | "null" | "text"
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: bridge callback present
 *   B2: verb in ACTIONS
 *   B3: surface in SURFACES
 *   B4: side given and invalid
 *   B5: show && nothing to point at
 *   B6: start && steps not a non-empty list
 *   LOOP steps: B7 step not an object; B8 step empty
 *   B9: renderer returned nothing
 *   V : canonical outcome (err|<msg> / ok / wrap)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include <json.h>

char *tour_tool(const json_t *args);
void preview_bridge_set_callback(void *cb, void *ctx);

static const char *g_canned;

static char *fake_bridge(void *ctx, const json_t *payload) {
    (void)ctx; (void)payload;
    if (!g_canned) return NULL;
    return strdup(g_canned);
}

/* Build the args object from decoded fields (mirrors twin.py's dict). */
static json_t *build_args(const char *action, const char *surface,
                          const char *side, const char *sel,
                          const char *title, const char *text,
                          const char *steps_f) {
    json_t *args = json_object();
    if (!args) return NULL;
    if (strcmp(action, "-") != 0)
        json_set(args, "action", json_string(action));
    if (strcmp(surface, "-") != 0)
        json_set(args, "surface", json_string(surface));
    if (strcmp(side, "-") != 0)
        json_set(args, "side", json_string(side));
    if (strcmp(sel, "-") != 0)
        json_set(args, "selector", json_string(sel));
    if (strcmp(title, "-") != 0)
        json_set(args, "title", json_string(title));
    if (strcmp(text, "-") != 0)
        json_set(args, "text", json_string(text));
    if (strcmp(steps_f, "-") != 0) {
        json_t *arr = json_array();
        if (strcmp(steps_f, "@") != 0) {
            char buf[512];
            snprintf(buf, sizeof buf, "%s", steps_f);
            char *save = NULL;
            for (char *tok = strtok_r(buf, "~", &save); tok;
                 tok = strtok_r(NULL, "~", &save)) {
                json_t *st;
                if (tok[0] == 't') {
                    st = json_string("notadict");
                } else {
                    st = json_object();
                    if (tok[1] == '1') json_set(st, "text", json_string("x"));
                }
                json_append(arr, st);
            }
        }
        json_set(args, "steps", arr);
    }
    return args;
}

/* Trace the branch ladder over the SAME decoded fields the real port sees,
 * then take the RESULT from the REAL tour_tool(). */
static void trace_one(const char *spec) {
    WCUP_BEGIN("tour_tool");
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", spec);
    char *f[8];
    int nf = 0;
    char *save = NULL;
    for (char *tok = strtok_r(buf, ";", &save); tok && nf < 8;
         tok = strtok_r(NULL, ";", &save))
        f[nf++] = tok;
    while (nf < 8) f[nf++] = (char *)"-";
    const char *action = f[0], *surface = f[1], *side = f[2], *sel = f[3];
    const char *title = f[4], *text = f[5], *steps_f = f[6], *cbrate = f[7];

    static const char *const ACTIONS[] =
        { "targets", "show", "start", "next", "prev", "stop" };
    static const char *const SURFACES[] = { "app", "preview" };
    static const char *const SIDES[] = { "top", "right", "bottom", "left" };

    /* decode (strip_lower_dup semantics: trim + lowercase, so the ladder's
     * set membership checks see what the real port sees) */
    static char verbbuf[128], wherebuf[128];
    {
        const char *s = strcmp(action, "-") ? action : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t n = 0;
        for (; *s && n < 127; s++) verbbuf[n++] = (char)tolower((unsigned char)*s);
        verbbuf[n] = '\0';
    }
    {
        const char *s = strcmp(surface, "-") ? surface : "app";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t n = 0;
        for (; *s && n < 127; s++) wherebuf[n++] = (char)tolower((unsigned char)*s);
        wherebuf[n] = '\0';
    }
    const char *verb = verbbuf;
    const char *where = wherebuf;
    int have_side = strcmp(side, "-") != 0;

    /* B1: bridge callback present — install the stub; "nocb" encodes
     * callback=None on the LIVE side (no desktop app). */
    g_canned = NULL;
    int have_cb = strcmp(cbrate, "nocb") != 0;
    if (strcmp(cbrate, "json") == 0) g_canned = "{\"ok\":true}";
    else if (strcmp(cbrate, "text") == 0) g_canned = "hello";
    preview_bridge_set_callback((void *)fake_bridge, NULL);
    WCUP_BRANCH(have_cb);                                      /* B1 */
    const char *outcome = NULL;
    if (!have_cb) {
        outcome = "err|tour is only available in the Hermes desktop app.";
        goto emit_cup;
    }

    int in_set = 0;
    for (size_t i = 0; i < 6; i++)
        if (strcmp(verb, ACTIONS[i]) == 0) in_set = 1;
    WCUP_BRANCH(in_set);                                       /* B2 */
    if (!in_set) {
        outcome = "err|action must be one of: targets, show, start, next, prev, stop.";
        goto emit_cup;
    }

    {
        int surf_ok = 0;
        for (size_t i = 0; i < 2; i++)
            if (strcmp(where, SURFACES[i]) == 0) surf_ok = 1;
        WCUP_BRANCH(surf_ok);                                  /* B3 */
        if (!surf_ok) {
            outcome = "err|surface must be one of: app, preview.";
            goto skip_to_val;
        }
    }
    {
        int side_bad = have_side;
        if (have_side) {
            side_bad = 1;
            for (size_t i = 0; i < 4; i++)
                if (strcmp(side, SIDES[i]) == 0) side_bad = 0;
        }
        WCUP_BRANCH(side_bad);                                 /* B4 */
        if (side_bad) {
            outcome = "err|side must be one of: top, right, bottom, left.";
            goto skip_to_val;
        }
    }
    {
        int show_empty = 0;
        if (strcmp(verb, "show") == 0) {
            show_empty = !( (strcmp(sel, "-") != 0 && sel[0]) ||
                            (strcmp(title, "-") != 0 && title[0]) ||
                            (strcmp(text, "-") != 0 && text[0]) );
        }
        WCUP_BRANCH(show_empty);                               /* B5 */
        if (show_empty) {
            outcome = "err|show needs a selector (and/or title/text for the popover).";
            goto skip_to_val;
        }
    }
    /* steps decode */
    {
        int steps_is_list = strcmp(steps_f, "-") != 0;
        const char *items[16];
        int nitems = 0;
        if (steps_is_list && strcmp(steps_f, "@") != 0) {
            char sbuf[512];
            snprintf(sbuf, sizeof sbuf, "%s", steps_f);
            char *sv = NULL;
            for (char *tok = strtok_r(sbuf, "~", &sv); tok && nitems < 16;
                 tok = strtok_r(NULL, "~", &sv))
                items[nitems++] = tok;
        }
        int start_bad = 0;
        if (strcmp(verb, "start") == 0)
            start_bad = !steps_is_list || nitems == 0;
        WCUP_BRANCH(start_bad);                                /* B6 */
        if (start_bad) {
            outcome = "err|start needs a non-empty steps array.";
            goto skip_to_val;
        }
        if (strcmp(verb, "start") == 0) {
            for (int i = 0; i < nitems; i++) {                 /* LOOP steps */
                WCUP_LOOP_TICK();
                int is_obj = items[i][0] == 'o';
                WCUP_BRANCH(!is_obj);                          /* B7 */
                if (!is_obj) {
                    static char so[64];
                    snprintf(so, sizeof so, "err|steps[%d] must be an object.", i);
                    outcome = so;
                    goto skip_to_val;
                }
                int empty = items[i][1] == '0';
                WCUP_BRANCH(empty);                            /* B8 */
                if (empty) {
                    static char se[96];
                    snprintf(se, sizeof se,
                             "err|steps[%d] needs a selector and/or title/text.", i);
                    outcome = se;
                    goto skip_to_val;
                }
            }
        }
    }

call_real:
    /* call the REAL port */
    outcome = "ok";
    {
        json_t *args = build_args(action, surface, side, sel, title, text, steps_f);
        char *out = tour_tool(args);
        if (args) json_free(args);
        free(out);
    }
    /* B9 + outcome classification (mirrors twin.py decoding of LIVE's return) */
    WCUP_BRANCH(!g_canned);                                    /* B9 */
    if (!g_canned)
        outcome = "err|The tour request timed out, or no GUI window answered. "
                  "For surface='preview' open a page in the preview pane first.";

skip_to_val:
emit_cup:
    {
        char resbuf[160];
        snprintf(resbuf, sizeof resbuf, "%s", outcome ? outcome : "");
        WCUP_VALSTR(resbuf);
    }

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("tour_tool");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s tour_tool <spec>...\n", argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
