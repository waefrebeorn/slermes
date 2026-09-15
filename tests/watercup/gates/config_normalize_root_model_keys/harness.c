/* gate: config_normalize_root_model_keys — LINKS the REAL C port
 * src/hermes_cli/config_py_pure.c::config_py_normalize_root_model_keys
 * (PoP: hermes_cli/config.py:_normalize_root_model_keys) via extra_src,
 * with no-op link stubs below for unrelated symbols in that translation
 * unit. Input encoding: one config JSON per argv slot.
 *
 * Ladder (twin.py mirrors EXACTLY), plus a V event hashing the compact
 * serialized normalized config:
 *   B1 nothing-to-migrate -> return unchanged
 *   B2 model not a dict -> wrap into {"default": ...}
 *   LOOP promote root provider/base_url/context_length (B3 each)
 *   B4 root api_base -> model.base_url
 *   B5 default empty -> promote model|name alias
 *   B6 default present -> drop model/name aliases
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include <port_config_py_helpers.h>
#include "json.h"

json_t *config_deep_merge(const json_t *base, const json_t *override){ (void)base;(void)override; return NULL; }
json_t *config_items_by_unique_name(const json_t *items){ (void)items; return NULL; }
char *normalize_custom_provider_entry_json(const char *entry_json, const char *provider_key){ (void)entry_json;(void)provider_key; return NULL; }
int coerce_ssl_verify(const char *value, int is_bool, int bool_val){ (void)value; return is_bool ? bool_val : 0; }
int coerce_config_version(const char *value, int is_bool){ (void)value; return is_bool ? 1 : 0; }
bool is_env_config_key(const char *key){ (void)key; return false; }
bool _validate_config_key(const char *key, char *suggestion, size_t sug_size){ (void)key;(void)suggestion;(void)sug_size; return false; }
void format_config_get_value(const char *value, int as_json, char *out, size_t out_size){ (void)value;(void)as_json; if (out && out_size) out[0]='\0'; }
void config_py_get_config_path(char *buf, size_t sz){ if (buf && sz) buf[0]='\0'; }
int config_py_atomic_config_write(const char *config_path, const json_t *data){ (void)config_path;(void)data; return -1; }
json_t *config_py_load_config_readonly(void){ return NULL; }
json_t *config_normalize_max_turns(const json_t *config_in){ (void)config_in; return NULL; }

static void run_one(const char *cfg_json) {
    char *err = NULL;
    json_t *in = json_parse(cfg_json, &err);
    WCUP_BEGIN("normalize_root_model_keys");
    json_t *res = NULL;
    if (!in) {
        WCUP_BRANCH(1);                              /* degenerate guard */
        res = json_new_object();
    } else {
        WCUP_BRANCH(0);
        res = config_py_normalize_root_model_keys(in);
        if (!res) res = json_new_object();
    }
    char *ser = json_serialize(res);
    WCUP_VALSTR(ser ? ser : "");
    free(ser);
    json_free(res);
    if (in) json_free(in);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_root_model_keys");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_root_model_keys <config-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
