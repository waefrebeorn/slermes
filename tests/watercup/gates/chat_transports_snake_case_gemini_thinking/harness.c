/* gate: chat_transports_snake_case_gemini_thinking — links the REAL C port
 * src/agent/chat_completions_ports.c::cct_snake_case_gemini_thinking_config
 * (post-parity-fix). LIVE twin: agent/transports/chat_completions.py::
 * _snake_case_gemini_thinking_config.
 *
 * Input argv: one config JSON per input.
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: config is a non-empty object      B2: includeThoughts is bool
 *   B3: thinkingLevel non-empty string    B4: thinkingBudget numeric
 *   VSTR(compact JSON of translated dict, "" == NULL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "watercup.h"

char *cct_snake_case_gemini_thinking_config(const char *config_json);

/* Out-of-path externals (hashing + google-native adapter; never on this path). */
void crypto_sha256(const void *in, size_t len, unsigned char out[32]) { (void)in; (void)len; (void)out; }
char *google_normalize_thinking_config(const char *c) { (void)c; return NULL; }

static void run_one(const char *cfg_json) {
    json_t *cfg = cfg_json ? json_parse(cfg_json, NULL) : NULL;
    bool obj = cfg && cfg->type == JSON_OBJECT && json_object_size(cfg) > 0;
    json_t *it = obj ? json_obj_get(cfg, "includeThoughts") : NULL;
    bool it_bool = it && it->type == JSON_BOOL;
    json_t *tl = obj ? json_obj_get(cfg, "thinkingLevel") : NULL;
    bool tl_ok = tl && tl->type == JSON_STRING && tl->str_val;
    if (tl_ok) {  /* strip check */
        const char *s = tl->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        if (!*s) tl_ok = false;
    }
    json_t *tb = obj ? json_obj_get(cfg, "thinkingBudget") : NULL;
    bool tb_num = tb && tb->type == JSON_NUMBER;

    WCUP_BEGIN("_snake_case_gemini_thinking_config");
    WCUP_BRANCH(obj);                                   /* B1 */
    WCUP_BRANCH(it_bool);                               /* B2 */
    WCUP_BRANCH(tl_ok);                                 /* B3 */
    WCUP_BRANCH(tb_num);                                /* B4 */

    char *r = cct_snake_case_gemini_thinking_config(cfg_json);
    WCUP_VALSTR(r ? r : "");
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    free(r);
    if (cfg) json_free(cfg);
    WCUP_END("_snake_case_gemini_thinking_config");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <config_json...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
