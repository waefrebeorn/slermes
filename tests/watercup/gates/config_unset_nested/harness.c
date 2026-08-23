/* gate: config_unset_nested — LINKS the REAL C port
 * src/hermes_cli/config_py_pure.c::config_py_unset_nested
 * (PoP: hermes_cli/config.py:_unset_nested) via extra_src, with no-op link
 * stubs below for unrelated symbols in that translation unit (unreachable on
 * the traced path). Input encoding: configJSON\x1fdotted_key.
 * Value: "1"/"0" removal verdict; trace mirrors the port's segment/walk/
 * delete/prune ladder. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"
#include <port_config_py_helpers.h>
#include "json.h"

json_t *config_deep_merge(const json_t *base, const json_t *override){ (void)base;(void)override; return NULL; }
json_t *config_items_by_unique_name(const json_t *items){ (void)items; return NULL; }
json_t *config_normalize_max_turns(const json_t *config_in){ (void)config_in; return NULL; }
char *normalize_custom_provider_entry_json(const char *entry_json, const char *provider_key){ (void)entry_json;(void)provider_key; return NULL; }
int coerce_ssl_verify(const char *value, int is_bool, int bool_val){ (void)value; return is_bool ? bool_val : 0; }
int coerce_config_version(const char *value, int is_bool){ (void)value; return is_bool ? 1 : 0; }
bool is_env_config_key(const char *key){ (void)key; return false; }
bool _validate_config_key(const char *key, char *suggestion, size_t sug_size){ (void)key;(void)suggestion;(void)sug_size; return false; }
void format_config_get_value(const char *value, int as_json, char *out, size_t out_size){ (void)value;(void)as_json; if (out && out_size) out[0] = '\0'; }
void config_py_get_config_path(char *buf, size_t sz){ if (buf && sz) buf[0] = '\0'; }
int config_py_atomic_config_write(const char *config_path, const json_t *data){ (void)config_path;(void)data; return -1; }
json_t *config_py_load_config_readonly(void){ return NULL; }

static void run_one(json_t *config, const char *key) {
    int removed = 0;
    WCUP_BEGIN("unset_nested");
    if (!config || !key || !*key) {
        WCUP_BRANCH(1);
    } else {
        WCUP_BRANCH(0);
        /* segment split */
        char *segs[64]; int nseg = 0;
        char *dup = strdup(key);
        char *seg = dup;
        int bad = 0;
        for (char *p = dup; ; p++) {
            if (*p == '\0' || *p == '.') {
                size_t len = (size_t)(p - seg);
                WCUP_LOOP_TICK();
                if (len == 0 || nseg >= 64) { WCUP_BRANCH(1); bad = 1; break; }
                WCUP_BRANCH(0);
                segs[nseg++] = seg;
                if (*p == '\0') break;
                *(char *)p = '\0';
                seg = p + 1;
            }
        }
        if (bad) {
            WCUP_BRANCH(1);
        } else {
            WCUP_BRANCH(0);
            json_t *cur = config;
            int ok = 1;
            for (int i = 0; i < nseg - 1 && ok; i++) {
                const char *sg = segs[i];
                if (cur->type == JSON_ARRAY) {
                    WCUP_BRANCH(1);
                    char *end = NULL;
                    long idx = strtol(sg, &end, 10);
                    if (end == sg || *end != '\0' || idx < 0 || idx >= (long)cur->c.count) { WCUP_BRANCH(1); ok = 0; }
                    else { WCUP_BRANCH(0); cur = cur->c.items[idx]; }
                } else if (cur->type == JSON_OBJECT) {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(1);
                    json_t *nx = json_obj_get(cur, sg);
                    if (!nx) { WCUP_BRANCH(1); ok = 0; }
                    else { WCUP_BRANCH(0); cur = nx; }
                } else {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(1);
                    ok = 0;
                }
            }
            if (!ok) {
                WCUP_BRANCH(1);
            } else {
                WCUP_BRANCH(0);
                const char *last = segs[nseg - 1];
                if (cur->type == JSON_ARRAY) {
                    WCUP_BRANCH(1);
                    char *end = NULL;
                    long idx = strtol(last, &end, 10);
                    if (end != last && *end == '\0' && idx >= 0 && idx < (long)cur->c.count) {
                        WCUP_BRANCH(1); removed = 1;
                        json_array_remove(cur, (size_t)idx);
                    } else WCUP_BRANCH(0);
                } else if (cur->type == JSON_OBJECT) {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(1);
                    if (json_obj_get(cur, last)) {
                        WCUP_BRANCH(1); removed = 1;
                        json_object_del(cur, last);
                    } else WCUP_BRANCH(0);
                } else {
                    WCUP_BRANCH(0);
                    WCUP_BRANCH(0);
                }
            }
        }
        free(dup);
    }
    char v[4]; snprintf(v, sizeof v, "%d", removed ? 1 : 0);
    WCUP_VALSTR(v);
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("unset_nested");
}
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s unset_nested <cfg\\x1fkey>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char *s = strdup(argv[i]);
        char *sep = strchr(s, '\x1f');
        const char *key = "";
        if (sep) { *sep = '\0'; key = sep + 1; }
        char *jerr = NULL;
        json_t *cfg = json_parse(s, &jerr);
        run_one(cfg, key);
        if (cfg) json_free(cfg);
        if (jerr) free(jerr);
        free(s);
    }
    return 0;
}
