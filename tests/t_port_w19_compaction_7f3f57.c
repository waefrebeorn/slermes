#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_compaction_7f3f57.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *kv(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return (v && v->type != JSON_NULL) ? v : NULL;
}

static int has(json_t *o, const char *k) { return kv(o, k) != NULL; }

/* Key present at all - a JSON null counts, which matters where a null differs
 * from the dataclass default (carried_messages). */
static int present(json_t *o, const char *k) { return o && json_obj_get(o, k) != NULL; }

static const char *opt_s(json_t *o, const char *k) {
    json_t *v = kv(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static long long ll(json_t *o, const char *k, long long def) {
    json_t *v = kv(o, k);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return def;
}

static double num(json_t *o, const char *k, double def) {
    json_t *v = kv(o, k);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) return atof(v->str_val);
    return def;
}

static int flag(json_t *o, const char *k, int def) {
    json_t *v = kv(o, k);
    if (!v) return def;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "rows")) {
            out = w19_compaction_7f3f57_rows(json_obj_get(in, "rows"));
        } else if (!strcmp(func, "compaction_payload")) {
            out = w19_compaction_7f3f57_compaction_payload(
                json_obj_get(in, "messages"), json_obj_get(in, "model_config_patch"),
                has(in, "watermark"), ll(in, "watermark", 0), opt_s(in, "lock_holder"),
                ll(in, "tail_count", 0), json_obj_get(in, "carried_messages"),
                present(in, "carried_messages"), json_obj_get(in, "covered_ids"),
                json_obj_get(in, "unresolved_held"), json_obj_get(in, "held_for_cas"));
        } else if (!strcmp(func, "publication_payload")) {
            out = w19_compaction_7f3f57_publication_payload(
                opt_s(in, "child_session_id"), opt_s(in, "source"), json_obj_get(in, "messages"),
                opt_s(in, "model"), json_obj_get(in, "model_config"), opt_s(in, "system_prompt"),
                opt_s(in, "cwd"), opt_s(in, "profile_name"), opt_s(in, "compression_lock_holder"),
                flag(in, "require_compression_lease", 1), flag(in, "require_lease_refresh", 0),
                num(in, "lease_ttl_seconds", 300.0), has(in, "watermark"), ll(in, "watermark", 0),
                has(in, "watermark_ceiling"), ll(in, "watermark_ceiling", 0));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
