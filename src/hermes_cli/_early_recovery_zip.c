/*
 * _early_recovery_zip.c — real C11 port of upstream Hermes
 *   hermes_cli/_early_recovery_zip.py pure journal parser.
 * PoP: w18_earlyreco_d648f9_parse_zip_swap_journal @ hermes_cli/_early_recovery_zip.py:_parse_zip_swap_journal
 */
#include "hermes_json.h"
#include "noc_w18_earlyreco_d648f9.h"

static int one_of(const char *s, const char *const *opts) {
    if (!s) return 0;
    for (int i = 0; opts[i]; i++) if (!strcmp(s, opts[i])) return 1;
    return 0;
}

/* re.fullmatch(r"[0-9a-f]{12}", gen) */
static int valid_gen(const char *g) {
    if (!g || strlen(g) != 12) return 0;
    for (int i = 0; i < 12; i++) {
        char c = g[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

struct json_t *w18_earlyreco_d648f9_parse_zip_swap_journal(const char *raw) {
    if (!raw) return json_null();
    json_t *data = json_parse(raw, NULL);
    if (!data || data->type != JSON_OBJECT) { json_free(data); return json_null(); }

    const json_t *phase = json_obj_get(data, "phase");
    const char *phase_s = (phase && phase->type == JSON_STRING) ? phase->str_val : NULL;
    static const char *const PHASES[] = { "staging", "swapping", "committed", NULL };
    if (!one_of(phase_s, PHASES)) { json_free(data); return json_null(); }

    const json_t *gen = json_obj_get(data, "gen");
    const char *gen_s = (gen && gen->type == JSON_STRING) ? gen->str_val : NULL;
    if (!valid_gen(gen_s)) { json_free(data); return json_null(); }

    const json_t *entries = json_obj_get(data, "entries");
    if (!entries || entries->type != JSON_ARRAY) { json_free(data); return json_null(); }
    for (size_t i = 0; i < entries->c.count; i++) {
        const json_t *e = entries->c.items[i];
        if (!e || e->type != JSON_ARRAY || e->c.count != 4) { json_free(data); return json_null(); }
        const json_t *name = e->c.items[0], *existed = e->c.items[1];
        const json_t *staged = e->c.items[2], *live = e->c.items[3];
        if (!name || name->type != JSON_STRING || !name->str_val) { json_free(data); return json_null(); }
        const char *nm = name->str_val;
        if (!*nm || !strcmp(nm, ".") || !strcmp(nm, "..") || strchr(nm, '/') || strchr(nm, '\\')) {
            json_free(data); return json_null();
        }
        if (!existed || existed->type != JSON_BOOL) { json_free(data); return json_null(); }
        if (!staged || staged->type != JSON_STRING) { json_free(data); return json_null(); }
        if (!live || live->type != JSON_STRING) { json_free(data); return json_null(); }
    }

    const json_t *temps = json_obj_get(data, "temps");
    if (temps && temps->type != JSON_OBJECT) { json_free(data); return json_null(); }
    if (temps) {
        for (size_t i = 0; i < temps->c.count; i++) {
            const json_t *v = temps->c.items[i];
            if (!v || v->type != JSON_STRING) { json_free(data); return json_null(); }
        }
    }

    /* Build (phase, gen, entries, temps); json_string/json_bool copy their inputs. */
    json_t *out = json_object();
    json_set(out, "phase", json_string(phase_s));
    json_set(out, "gen", json_string(gen_s));
    json_t *oe = json_array();
    for (size_t i = 0; i < entries->c.count; i++) {
        const json_t *e = entries->c.items[i];
        json_t *row = json_array();
        json_append(row, json_string(e->c.items[0]->str_val));
        json_append(row, json_bool(e->c.items[1]->bool_val));
        json_append(row, json_string(e->c.items[2]->str_val ? e->c.items[2]->str_val : ""));
        json_append(row, json_string(e->c.items[3]->str_val ? e->c.items[3]->str_val : ""));
        json_append(oe, row);
    }
    json_set(out, "entries", oe);
    json_t *ot = json_object();
    if (temps) {
        for (size_t i = 0; i < temps->c.count; i++) {
            const char *k = temps->c.keys[i];
            const json_t *v = temps->c.items[i];
            json_set(ot, k, json_string(v->str_val ? v->str_val : ""));
        }
    }
    json_set(out, "temps", ot);
    json_free(data);
    return out;
}
