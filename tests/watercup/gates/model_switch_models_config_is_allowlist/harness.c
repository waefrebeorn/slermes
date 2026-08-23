/* gate: model_switch_models_config_is_allowlist — traced copy of
 * src/hermes_cli/model_switch_wrappers.c::model_switch_config_is_allowlist
 * (LIVE twin: hermes_cli/model_switch.py::_models_config_is_allowlist).
 *
 * Input is a JSON-encoded ``models:`` config value. Branch ladder
 * (twin.py mirrors EXACTLY):
 *   B1: null input            -> false
 *   B2: unparseable JSON      -> false
 *   B3: string -> bool(strip) / object -> false / array -> true / other -> false
 *   V1: result
 */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

/* Minimal top-level JSON classifier for the gate's input class:
 * returns 's' string, 'o' object, 'a' array, 'i' invalid.
 * For strings, *str_out/*str_len give the (unescaped-lite) contents. */
static char classify_json(const char *s, const char **str_out, size_t *str_len) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (*s == '"') {
        const char *start = ++s;
        while (*s && *s != '"') {
            if (*s == '\\' && s[1]) s++;
            s++;
        }
        if (*s != '"') return 'i';
        *str_out = start;
        *str_len = (size_t)(s - start);
        return 's';
    }
    if (*s == '{') {
        /* balanced-count braces */
        int depth = 0;
        for (; *s; s++) {
            if (*s == '{') depth++;
            else if (*s == '}') { depth--; if (!depth) return 'o'; }
        }
        return 'i';
    }
    if (*s == '[') {
        int depth = 0;
        for (; *s; s++) {
            if (*s == '[') depth++;
            else if (*s == ']') { depth--; if (!depth) return 'a'; }
        }
        return 'i';
    }
    /* null / true / false / number scalars */
    if (strncmp(s, "null", 4) == 0 || strncmp(s, "true", 4) == 0 ||
        strncmp(s, "false", 5) == 0 || *s == '-' || *s == '+' ||
        (*s >= '0' && *s <= '9') || *s == '.')
        return 'x';
    return 'i';
}

static void wc_is_allowlist(const char *value_json) {
    WCUP_BEGIN("models_config_is_allowlist");
    bool result = false;
    if (!value_json) {                          /* B1 */
        WCUP_BRANCH(1);
        goto emit;
    }
    WCUP_BRANCH(0);
    {
        const char *sv = NULL; size_t sl = 0;
        char kind = classify_json(value_json, &sv, &sl);
        if (kind == 'i') {                      /* B2 unparseable */
            WCUP_BRANCH(1);
            goto emit;
        }
        WCUP_BRANCH(0);
        {
            bool is_str = (kind == 's');
            WCUP_BRANCH(is_str);                /* B3 */
            if (is_str) {
                size_t i = 0;
                while (i < sl && (sv[i] == ' ' || sv[i] == '\t')) i++;
                result = i < sl;
            } else {
                bool is_obj = (kind == 'o');
                WCUP_BRANCH(is_obj);            /* B4 */
                if (is_obj) result = false;     /* dict = metadata, not allowlist */
                else {
                    bool is_arr = (kind == 'a');
                    WCUP_BRANCH(is_arr);        /* B5 */
                    result = is_arr;
                }
            }
        }
    }
emit:
    WCUP_VALSTR(result ? "True" : "False");     /* V1 */
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_config_is_allowlist");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_config_is_allowlist <json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_is_allowlist(argv[i]);
    return 0;
}
