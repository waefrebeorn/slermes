/* gate: base_safe_url_for_log — traced copy of
 * src/gateway/platforms/base.c::safe_url_for_log.
 * LIVE twin: gateway/platforms/base.py::safe_url_for_log.
 * Input format: "<max_len>|<url>".
 * Branch ladder (same rung order both sides):
 *   B1 max_len<=0  B2 empty url  B3 scheme:// present
 *   B4 path non-"/" present  B5 len<=max_len  B6 max_len<=3   V result
 * Inputs avoid query strings/fragments (LIVE urlsplit strips them; the C port
 * keeps them — documented divergence outside this gate's sub-contract).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void trace_one(const char *input) {
    char tmp[2048]; snprintf(tmp, sizeof tmp, "%s", input);
    char *pipe = strchr(tmp, '|');
    int max_len = 80;
    const char *url = "";
    if (pipe) { *pipe = '\0'; max_len = atoi(tmp); url = pipe + 1; }
    else url = tmp;

    WCUP_BEGIN("safe_url_for_log");
    char safe[4096]; safe[0] = '\0';
    const char *raw = url;
    if (max_len <= 0) { WCUP_BRANCH(1); safe[0]='\0'; goto out; }
    WCUP_BRANCH(0);
    if (!raw || !raw[0]) { WCUP_BRANCH(1); goto out; }
    WCUP_BRANCH(0);

    char *dcolon = strstr(raw, "://");
    if (dcolon) {
        WCUP_BRANCH(1);
        char *netloc = dcolon + 3;
        char *slash = strchr(netloc, '/');
        char path[2048]; path[0] = '\0';
        if (slash) { snprintf(path, sizeof path, "%s", slash); *slash = '\0'; }
        char *at = strrchr(netloc, '@');
        char *nl = at ? at + 1 : netloc;
        snprintf(safe, sizeof safe, "%.*s://%s", (int)(dcolon - raw), raw, nl);
        if (path[0] && strcmp(path, "/") != 0) {
            WCUP_BRANCH(1);
            char *base = strrchr(path, '/');
            if (base && base[1]) snprintf(safe + strlen(safe), sizeof(safe) - strlen(safe), "/.../%s", base + 1);
            else snprintf(safe + strlen(safe), sizeof(safe) - strlen(safe), "/...");
        } else {
            WCUP_BRANCH(0);
        }
    } else {
        WCUP_BRANCH(0);
        snprintf(safe, sizeof safe, "%s", raw);
    }

    if (strlen(safe) <= (size_t)max_len) { WCUP_BRANCH(1); goto out; }
    WCUP_BRANCH(0);
    if (max_len <= 3) {
        WCUP_BRANCH(1);
        int ml = max_len < 4095 ? max_len : 4095;
        memset(safe, '.', (size_t)ml); safe[ml] = '\0';
        goto out;
    }
    WCUP_BRANCH(0);
    {
        char trunc[4096];
        snprintf(trunc, sizeof trunc, "%.*s...", max_len - 3 > 0 ? max_len - 3 : 0, safe);
        snprintf(safe, sizeof safe, "%s", trunc);
    }
out:;
    WCUP_VALSTR(safe);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("safe_url_for_log");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s safe_url_for_log <max|url>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
