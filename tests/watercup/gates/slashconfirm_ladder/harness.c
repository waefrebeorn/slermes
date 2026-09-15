/* gate: slashconfirm_ladder — traced copies of
 * lib/libslashconfirm/slash_confirm.c::slashconfirm_resolve /
 * slashconfirm_clear_if_stale, driven against an in-memory fixture whose
 * created_at is injectable so both sides see identical clock state.
 * (LIVE twin: tools/slash_confirm.py::resolve / clear_if_stale.)
 *
 * Input tags (argv):
 *   "res:<key>|<confirm_id>|<choice>|<age>|<timeout>"
 *   "stale:<key>|<age>|<timeout>"
 *
 * Branch ladder res: (must match twin.py EXACTLY)
 *   B1 pending entry found / B2 confirm_id matches / B3 within timeout /
 *   B4 handler present
 *   VSTR(class ran|stale|mismatch|missing|nohandler)
 * Branch ladder stale:
 *   B1 entry found / B2 entry is stale
 *   VSTR("1"/"0")
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include "watercup.h"

#define MAX_KEY 64

typedef struct {
    char session_key[MAX_KEY];
    char confirm_id[64];
    char command[32];
    double created_at;
    int has_handler;
} entry_t;

static entry_t g_entry;
static int g_has_entry = 0;

static char *fake_handler(const char *choice)
{
    char *out = malloc(64);
    snprintf(out, 64, "ran:%s", choice);
    return out;
}

static void fixture_register(const char *key, const char *cid, double age)
{
    snprintf(g_entry.session_key, sizeof g_entry.session_key, "%s", key);
    snprintf(g_entry.confirm_id, sizeof g_entry.confirm_id, "%s", cid);
    snprintf(g_entry.command, sizeof g_entry.command, "reload-mcp");
    g_entry.created_at = (double)time(NULL) - age;
    g_entry.has_handler = 1;
    g_has_entry = 1;
}

static void wc_resolve(const char *key, const char *cid, const char *choice,
                       double age, double timeout)
{
    fixture_register(key, cid, age);
    WCUP_BEGIN("slashconfirm_resolve");
    const char *label = "missing";

    WCUP_BRANCH(g_has_entry);                        /* B1 */
    if (!g_has_entry) goto done;

    int id_match = strcmp(g_entry.confirm_id, cid) == 0;
    WCUP_BRANCH(id_match);                           /* B2 */
    if (!id_match) { label = "mismatch"; goto done; }

    g_has_entry = 0;                                 /* pop before running */
    double now = (double)time(NULL);
    int in_time = (now - g_entry.created_at) <= timeout;
    WCUP_BRANCH(in_time);                            /* B3 */
    if (!in_time) { label = "stale"; goto done; }

    WCUP_BRANCH(g_entry.has_handler != 0);           /* B4 */
    if (!g_entry.has_handler) { label = "nohandler"; goto done; }
    label = "ran";

done:
    WCUP_VALSTR(label);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("slashconfirm_resolve");
}

static void wc_stale(const char *key, double age, double timeout)
{
    fixture_register(key, "id-1", age);
    WCUP_BEGIN("slashconfirm_clear_if_stale");

    WCUP_BRANCH(g_has_entry);                        /* B1 */
    int stale = 0;
    if (g_has_entry) {
        double now = (double)time(NULL);
        stale = ((double)time(NULL) - g_entry.created_at) > timeout;
        (void)now;
        WCUP_BRANCH(stale);                          /* B2 */
        if (stale) g_has_entry = 0;
    } else {
        WCUP_BRANCH(0);
    }
    char res[8];
    snprintf(res, sizeof res, "%d", stale);
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("slashconfirm_clear_if_stale");
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s fn <input>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        const char *in = argv[i];
        char f1[64], f2[64], f3[64];
        if (strncmp(in, "res:", 4) == 0) {
            const char *p = in + 4;
            const char *b1 = strchr(p, '|');
            snprintf(f1, sizeof f1, "%.*s", (int)(b1 - p), p); p = b1 + 1;
            const char *b2 = strchr(p, '|');
            snprintf(f2, sizeof f2, "%.*s", (int)(b2 - p), p); p = b2 + 1;
            const char *b3 = strchr(p, '|');
            snprintf(f3, sizeof f3, "%.*s", (int)(b3 - p), p); p = b3 + 1;
            double age = atof(p);
            const char *b4 = strchr(p, '|');
            double timeout = b4 ? atof(b4 + 1) : 300.0;
            wc_resolve(f1, f2, f3, age, timeout);
        } else if (strncmp(in, "stale:", 6) == 0) {
            const char *p = in + 6;
            const char *b1 = strchr(p, '|');
            snprintf(f1, sizeof f1, "%.*s", (int)(b1 - p), p); p = b1 + 1;
            const char *b2 = strchr(p, '|');
            double age = atof(p);
            double timeout = b2 ? atof(b2 + 1) : 300.0;
            wc_stale(f1, age, timeout);
        } else {
            fprintf(stderr, "bad input: %s\n", in);
            return 2;
        }
    }
    return 0;
}
