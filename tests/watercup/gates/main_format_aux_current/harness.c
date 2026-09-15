/* gate: main_format_aux_current — traced copy of src/hermes_cli/main_wrappers.c::
 * main_u_format_aux_current AFTER the parity fix that renders the label as a
 * single line ("custom (<short>) · <model>" / "auto[ · model]"), matching LIVE
 * hermes_cli/main.py::_format_aux_current. The translation unit drags in
 * config/curator deps so the fixed body is inlined verbatim here.
 *
 * Input argv: "<base_url>\t<provider>\t<model>".
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: arg empty            B2: base_url non-empty
 *   B3: provider auto/empty  (model presence folded into VSTR)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

/* verbatim post-fix port logic is inlined in run_one below (single-string
 * render matching LIVE _format_aux_current). */

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    WCUP_BEGIN("_format_aux_current");
    WCUP_BRANCH(!buf[0]);                                       /* B1 */
    const char *t1 = strchr(buf, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *base = buf;
    size_t base_len = t1 ? (size_t)(t1 - buf) : strlen(buf);
    const char *provider_raw = t1 ? t1 + 1 : "";
    size_t plen = t1 ? (size_t)((t2 ? t2 : buf + strlen(buf)) - (t1 + 1)) : 0;
    const char *model = t2 ? t2 + 1 : "";
    bool has_base = base_len > 0;
    WCUP_BRANCH(has_base);                                      /* B2 */
    bool provider_auto = (plen == 4 && strncmp(provider_raw, "auto", 4) == 0) || plen == 0;
    WCUP_BRANCH(!has_base && provider_auto);                    /* B3 */

    char *r;
    if (base_len > 0) {
        const char *short_base = base;
        if (strncmp(short_base, "https://", 8) == 0) short_base += 8;
        else if (strncmp(short_base, "http://", 7) == 0) short_base += 7;
        size_t blen = (size_t)((base + base_len) - short_base);
        while (blen > 0 && short_base[blen-1] == '/') blen--;
        char out[1024];
        snprintf(out, sizeof out, "custom (%.*s)%s%s", (int)blen, short_base,
                 model[0] ? " \xc2\xb7 " : "", model[0] ? model : "");
        r = strdup(out);
    } else if (provider_auto) {
        char out[1024];
        snprintf(out, sizeof out, "auto%s%s",
                 model[0] ? " \xc2\xb7 " : "", model[0] ? model : "");
        r = strdup(out);
    } else {
        char out[1024];
        if (model[0]) snprintf(out, sizeof out, "%.*s \xc2\xb7 %s", (int)plen, provider_raw, model);
        else snprintf(out, sizeof out, "%.*s", (int)plen, provider_raw);
        r = strdup(out);
    }
    WCUP_VALSTR(r);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    WCUP_END("_format_aux_current");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <base\\tprov\\tmodel...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
