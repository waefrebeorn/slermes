/* gate: message_sanitization_reapply_reasoning_echo
 * Calls the REAL C port src/agent/message_sanitization_pure.c::
 * msg_sanitize_reapply_reasoning_echo against LIVE Python
 * agent/message_sanitization.py::reapply_reasoning_echo.
 *
 * Input argv format: "<pad><TAB><msg>;<msg>;...
 *   <msg> := role|rc|reasoning|tc
 *     rc / reasoning: "-" = key absent, "~" = empty string, else literal text
 *     tc: "1" = tool_calls present, "0" = absent
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"

int msg_sanitize_reapply_reasoning_echo(json_t *api_messages, int needs_thinking_pad);

/* out-of-path externals (not on this fn's path) */
void hermes_log(int lvl, const char *fmt, ...) {(void)lvl; (void)fmt;}
void crypto_sha256(const unsigned char *d, size_t l, unsigned char o[32]) {(void)d; (void)l; (void)o;}

static void run_one(const char *input) {
    WCUP_BEGIN("reapply_reasoning_echo");
    char tmp[4096];
    snprintf(tmp, sizeof tmp, "%s", input ? input : "");
    char *tab = strchr(tmp, '\t');
    int pad = 0;
    const char *msgs = tmp;
    if (tab) { *tab = '\0'; pad = tmp[0] == '1'; msgs = tab + 1; }

    /* Build a JSON array mirroring the encoded messages. */
    char js[8192];
    size_t o = 0;
    o += (size_t)snprintf(js + o, sizeof js - o, "[");
    /* encode each field into JSON manually */
    const char *p = msgs;
    int first = 1;
    while (*p) {
        char tok[1024];
        size_t t = 0;
        while (*p && *p != ';') { if (t + 1 < sizeof tok) tok[t++] = *p; p++; }
        if (*p == ';') p++;
        tok[t] = '\0';
        char f1[256] = "", f2[256] = "-", f3[256] = "-", f4[2] = "0";
        int fi = 0; size_t fo = 0;
        for (size_t k = 0; k <= t; k++) {
            if (k == t || tok[k] == '|') {
                if (fi == 0) { f1[fo] = 0; } else if (fi == 1) { f2[fo] = 0; } else if (fi == 2) { f3[fo] = 0; } else { f4[fo] = 0; }
                fi++; fo = 0;
                if (fi > 3) break;
                continue;
            }
            if (fi == 0) { if (fo < 255) f1[fo++] = tok[k]; }
            else if (fi == 1) { if (fo < 255) f2[fo++] = tok[k]; }
            else if (fi == 2) { if (fo < 255) f3[fo++] = tok[k]; }
            else { f4[0] = tok[k]; }
        }
        /* normalize sentinel values */
        const char *rc = strcmp(f2, "-") == 0 ? NULL : (strcmp(f2, "~") == 0 ? "" : f2);
        const char *rs = strcmp(f3, "-") == 0 ? NULL : (strcmp(f3, "~") == 0 ? "" : f3);
        if (!first) js[o++] = ',';
        first = 0;
        o += (size_t)snprintf(js + o, sizeof js - o,
            "{\"role\":\"%s\"", f1);
        if (rc) o += (size_t)snprintf(js + o, sizeof js - o, ",\"reasoning_content\":\"%s\"", rc);
        if (rs) o += (size_t)snprintf(js + o, sizeof js - o, ",\"reasoning\":\"%s\"", rs);
        if (f4[0] == '1') o += (size_t)snprintf(js + o, sizeof js - o, ",\"tool_calls\":[{\"id\":\"t1\"}]");
        o += (size_t)snprintf(js + o, sizeof js - o, "}");
    }
    o += (size_t)snprintf(js + o, sizeof js - o, "]");

    json_t *arr = json_parse(js, NULL);
    int changed = msg_sanitize_reapply_reasoning_echo(arr, pad);

    /* Trace the same ladder as LIVE (structure over the encoded walk). */
    {
        /* re-walk the token list purely for branch parity */
        const char *q = msgs;
        while (*q) {
            char tokc[1024];
            size_t t = 0;
            while (*q && *q != ';') { if (t + 1 < sizeof tokc) tokc[t++] = *q; q++; }
            if (*q == ';') q++;
            tokc[t] = '\0';
            /* role is field 0 */
            char role[256] = ""; size_t ro = 0;
            for (size_t k = 0; k < t && tokc[k] != '|'; k++) { if (ro < 255) role[ro++] = tokc[k]; }
            role[ro] = 0;
            int is_assistant = strcmp(role, "assistant") == 0;
            if (!is_assistant) { WCUP_BRANCH(0); continue; }
            WCUP_BRANCH(1);
            /* find rc field presence & value */
            char f2c[256] = "-"; int fi = 0; size_t fo = 0;
            for (size_t k = 0; k <= t; k++) {
                if (k == t || tokc[k] == '|') {
                    if (fi == 1) f2c[fo] = 0;
                    fi++; fo = 0;
                    if (fi > 1) break;
                    continue;
                }
                if (fi == 1) { if (fo < 255) f2c[fo++] = tokc[k]; }
            }
            int has_truthy_rc = (strcmp(f2c, "-") != 0 && strcmp(f2c, "~") != 0 && f2c[0]);
            if (pad) {
                WCUP_BRANCH(1);
                WCUP_BRANCH(has_truthy_rc);
            } else {
                WCUP_BRANCH(0);
                WCUP_BRANCH(strcmp(f2c, "-") != 0);
            }
        }
    }

    char res[32];
    snprintf(res, sizeof res, "%d", changed);
    WCUP_VALSTR(res);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("reapply_reasoning_echo");
    if (arr) json_free(arr);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s reapply_reasoning_echo <pad<TAB>msgs>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
