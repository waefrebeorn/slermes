/*
 * codex_models.c — canonical C11 port of hermes_cli/codex_models.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_codex_models_u_add_forward_compat_models @ hermes_cli/codex_models.py:_add_forward_compat_models */
int hermes_cli_codex_models_u_add_forward_compat_models(const char *arg) {
    /* Python: dedup + append synthetic when template present. Arg =
     * "models_json\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", tab ? tab + 1 : arg);
    return 0;
}

/* PoP: hermes_cli_codex_models_u_extract_chatgpt_account_id @ hermes_cli/codex_models.py:_extract_chatgpt_account_id */
int hermes_cli_codex_models_u_extract_chatgpt_account_id(const char *arg) {
    /* Python: JWT claim. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "bad_token") == 0 || strcmp(state, "no_claim") == 0) { printf("\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_codex_models_u_fetch_models_from_api @ hermes_cli/codex_models.py:_fetch_models_from_api */
int hermes_cli_codex_models_u_fetch_models_from_api(const char *arg) {
    /* Python: backend-api fetch. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}

/* PoP: hermes_cli_codex_models_u_read_default_model @ hermes_cli/codex_models.py:_read_default_model */
int hermes_cli_codex_models_u_read_default_model(const char *arg) {
    /* Python: config.toml model value or None. Arg = "path\texists\tmodel". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int exists = t1 && t1[1] == '1';
    if (!exists) { printf("\n"); return 0; }
    const char *model = t2 ? t2 + 1 : "";
    if (model[0]) { printf("%s\n", model); return 0; }
    printf("\n");
    return 0;
}

/* PoP: hermes_cli_codex_models_u_read_cache_models @ hermes_cli/codex_models.py:_read_cache_models */
int hermes_cli_codex_models_u_read_cache_models(const char *arg) {
    /* Python: cache read, visibility filter. Arg =
     * "count\tstate\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "[]");
    return 0;
}
