/* gate: config_preserve_env_ref_templates — traced copy of
 * src/hermes_cli/config_py_pure.c::config_py_preserve_env_ref_templates
 * (LIVE twin: hermes_cli/config.py::_preserve_env_ref_templates).
 *
 * Input per argv token: JSON [current, raw, loaded_expanded].
 *
 * Rung ladder (must match twin.py EXACTLY, same rung order; recursion
 * wrapped in R/r events):
 *   B1: current is string
 *     B2: raw is string AND contains a ${...} template
 *       B3: current == raw                    -> keep raw
 *       B4: loaded_expanded == current        -> keep raw
 *       B5: expand(raw) == current            -> keep raw
 *       V : "keep_raw" | "keep_current"
 *   B6: current is object -> per-key L tick + recursive call (R/r)
 *   B7: current is array  (inputs never hit the true arm)
 *   V : leaf decision tag ("keep_raw"/"keep_current") at each string leaf;
 *       scalar passthrough leaves tag "passthrough".
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

/* re.search(r"\${[^}]+}") equivalent */
static int has_env_template(const char *s)
{
    if (!s) return 0;
    const char *p = s;
    while ((p = strstr(p, "${")) != NULL) {
        if (strchr(p + 2, '}')) return 1;
        p += 2;
    }
    return 0;
}

/* mirrors config_expand_env_vars_local: unresolved refs stay verbatim,
 * ${VAR:-def} honors the default. */
static char *expand_env(const char *input)
{
    if (!input) return NULL;
    size_t cap = strlen(input) + 1, pos = 0;
    char *result = malloc(cap);
    if (!result) return NULL;
    const char *p = input;
    while (*p) {
        if (*p == '$' && *(p + 1) == '{') {
            const char *start = p + 2;
            const char *end = strchr(start, '}');
            if (!end) { result[pos++] = *p++; continue; }
            const char *colon = NULL;
            for (const char *s = start; s < end; s++)
                if (*s == ':' && *(s + 1) == '-') { colon = s; break; }
            char var[256]; size_t vl;
            const char *def = NULL; size_t dl = 0;
            if (colon) { vl = (size_t)(colon - start); def = colon + 2; dl = end - def; }
            else vl = (size_t)(end - start);
            if (vl >= sizeof(var)) vl = sizeof(var) - 1;
            memcpy(var, start, vl); var[vl] = '\0';
            const char *val = getenv(var);
            char *dheap = NULL;
            if (!val && def) {
                dheap = malloc(dl + 1);
                if (dheap) { memcpy(dheap, def, dl); dheap[dl] = '\0'; }
                val = dheap;
            }
            if (val) {
                size_t v2 = strlen(val);
                if (pos + v2 + 1 > cap) { cap = pos + v2 + 16; result = realloc(result, cap); }
                memcpy(result + pos, val, v2); pos += v2;
            } else {
                size_t lit = (size_t)(end - p) + 1;
                if (pos + lit + 1 > cap) { cap = pos + lit + 16; result = realloc(result, cap); }
                memcpy(result + pos, p, lit); pos += lit;
            }
            free(dheap);
            p = end + 1;
        } else {
            if (pos + 2 > cap) { cap += 16; result = realloc(result, cap); }
            result[pos++] = *p++;
        }
    }
    result[pos] = '\0';
    return result;
}

static json_t *trace_preserve(wcup_t *wc, const json_t *current,
                              const json_t *raw, const json_t *loaded);

static void emit_leaf(wcup_t *wc, const char *tag)
{
    wcup_valstr(wc, tag);
}

static json_t *trace_preserve(wcup_t *wc, const json_t *current,
                              const json_t *raw, const json_t *loaded)
{
    wcup_rec_in(wc);
    json_t *result = NULL;

    if (current && current->type == JSON_STRING) {
        wcup_branch(wc, 1);  /* B1 */
        const char *cstr = current->str_val;
        int raw_is_tmpl = raw && raw->type == JSON_STRING &&
                          has_env_template(raw->str_val);
        wcup_branch(wc, raw_is_tmpl);                          /* B2 */
        if (raw_is_tmpl) {
            const char *rstr = raw->str_val;
            int eq_raw = strcmp(cstr, rstr) == 0;
            wcup_branch(wc, eq_raw);                           /* B3 */
            if (!eq_raw) {
                int le_str = loaded && loaded->type == JSON_STRING &&
                             loaded->str_val;
                int eq_loaded = le_str && strcmp(cstr, loaded->str_val) == 0;
                wcup_branch(wc, eq_loaded);                    /* B4 */
                if (!eq_loaded) {
                    char *exp = expand_env(rstr);
                    int eq_exp = exp && strcmp(exp, cstr) == 0;
                    free(exp);
                    wcup_branch(wc, eq_exp);                   /* B5 */
                    if (eq_exp) { emit_leaf(wc, "keep_raw"); result = json_copy(raw); goto out; }
                } else {
                    emit_leaf(wc, "keep_raw"); result = json_copy(raw); goto out;
                }
            } else {
                emit_leaf(wc, "keep_raw"); result = json_copy(raw); goto out;
            }
            emit_leaf(wc, "keep_current");
            result = json_copy(current);
            goto out;
        }
        emit_leaf(wc, "keep_current");
        result = json_copy(current);
        goto out;
    }
    wcup_branch(wc, 0);                                        /* B1 */

    if (current && current->type == JSON_OBJECT) {
        wcup_branch(wc, 1);  /* B6 */
        json_t *out = json_object();
        for (size_t i = 0; i < current->c.count; i++) {
            wcup_loop(wc);
            const char *key = current->c.keys[i];
            json_t *rv = (raw && raw->type == JSON_OBJECT)
                ? json_obj_get((json_t *)raw, key) : NULL;
            json_t *lv = (loaded && loaded->type == JSON_OBJECT)
                ? json_obj_get((json_t *)loaded, key) : NULL;
            json_set(out, key, trace_preserve(wc, current->c.items[i], rv, lv));
        }
        result = out;
        goto out;
    }
    wcup_branch(wc, 0);                                        /* B6 */

    if (current && current->type == JSON_ARRAY) {
        wcup_branch(wc, 1);  /* B7 */
        /* positional fallback path (name-matched path needs named objects;
         * inputs never reach this arm — kept for structural completeness) */
        json_t *out = json_array();
        for (size_t i = 0; i < current->c.count; i++) {
            wcup_loop(wc);
            json_t *rv = (raw && raw->type == JSON_ARRAY && i < raw->c.count)
                ? raw->c.items[i] : NULL;
            json_t *lv = (loaded && loaded->type == JSON_ARRAY && i < loaded->c.count)
                ? loaded->c.items[i] : NULL;
            json_append(out, trace_preserve(wc, current->c.items[i], rv, lv));
        }
        result = out;
        goto out;
    }
    wcup_branch(wc, 0);                                        /* B7 */
    emit_leaf(wc, "passthrough");
    result = json_copy(current ? current : json_null());

out:
    wcup_rec_out(wc);
    return result;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s preserve_env_ref_templates '[cur,raw,loaded]'...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *jerr = NULL;
        json_t *root = json_parse(argv[i], &jerr);
        if (!root || root->type != JSON_ARRAY || root->c.count < 2) {
            fprintf(stderr, "bad input: %s\n", argv[i]);
            return 2;
        }
        json_t *cur = root->c.items[0];
        json_t *raw = root->c.items[1];
        json_t *loaded = root->c.count > 2 ? root->c.items[2] : NULL;
        wcup_t wc_local; /* unused shim to satisfy macros inside helpers */
        (void)wc_local;
        /* top-level cup: one BEGIN/FINALIZE per input around the recursion */
        WCUP_BEGIN("preserve_env_ref_templates");
        json_t *res = trace_preserve(&_wc, cur, raw, loaded);
        (void)res;
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("preserve_env_ref_templates");
    }
    return 0;
}
