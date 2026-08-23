/* gate: tools_cronjob_tools__validate_cron_base_url — traced copy of
 * src/tools/cronjob_tools.c::cronjob_validate_cron_base_url.
 * LIVE twin: tools/cronjob_tools.py::_validate_cron_base_url.
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: normalized base_url empty -> allow (None)
 *   B2: normalized provider empty -> block
 *   B3: provider == "custom"      -> allow
 *   else                          -> block (fail closed)
 * VSTR(class): "allow" / "no_provider" / "blocked".
 *
 * Agreement-zone gating (see gate.json notes): inputs are restricted to the
 * cases where this fail-closed C port and LIVE classify identically (no
 * configured named-custom providers, no registry host-match hits).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"

static char *wc_normalize(const char *s, int strip_trailing_slash) {
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    if (strip_trailing_slash)
        while (n > 0 && s[n - 1] == '/') n--;
    /* also strip trailing whitespace */
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    char *out = malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static void wc_case(const char *provider, const char *base_url) {
    WCUP_BEGIN("cronjob_validate_cron_base_url");
    const char *cls;

    char *bu = wc_normalize(base_url, 1);
    int bu_empty = (!bu || !bu[0]);
    WCUP_BRANCH(bu_empty);                                      /* B1 */
    if (bu_empty) { cls = "allow"; goto done; }

    char *prov = wc_normalize(provider, 0);
    int prov_empty = (!prov || !prov[0]);
    WCUP_BRANCH(prov_empty);                                    /* B2 */
    if (prov_empty) { cls = "no_provider"; goto done; }

    int is_custom = (strcmp(prov, "custom") == 0);
    WCUP_BRANCH(is_custom);                                     /* B3 */
    if (is_custom) { cls = "allow"; goto done; }

    cls = "blocked";

done:
    WCUP_VALSTR(cls);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("cronjob_validate_cron_base_url");
    }
    free(bu);
    free(prov);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s cronjob_validate_cron_base_url '<prov>|<url>'...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *bar = strchr(buf, '|');
        if (bar) *bar = '\0';
        wc_case(buf, bar ? bar + 1 : "");
    }
    return 0;
}
