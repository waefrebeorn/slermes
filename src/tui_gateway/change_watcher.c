/*
 * change_watcher.c — C port of upstream Hermes tui_gateway/change_watcher.py.
 * Gate: w14_changewatc_e37815
 *
 * PoP: w14_changewatc_e37815_skin_payload @ tui_gateway/change_watcher.py:skin_payload
 * PoP: w14_changewatc_e37815_groups_changed_payload @ tui_gateway/change_watcher.py:_groups_changed_payload
 *
 * hermes_cli.skin_engine (init/get_active_skin) and tui_gateway.event_replay
 * (replay_epoch) are stubbed in the oracle; the skin object and epoch are
 * injected as explicit seams from the fixture. The live process-wide
 * itertools.count(1) is mirrored by a static counter that advances once per
 * call, so both processes stay aligned while consuming the same fixture file.
 */
#include "noc_w14_changewatc_e37815.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

/* Live: (skin.branding or {}).get("help_header", ""). */
static json_t *help_header(const json_t *skin) {
    json_t *branding = json_obj_get(skin, "branding");
    if (json_is_object(branding)) {
        json_t *v = json_obj_get(branding, "help_header");
        return v ? json_copy(v) : json_string("");
    }
    return json_string("");
}

static json_t *field_copy(const json_t *skin, const char *key) {
    json_t *v = json_obj_get(skin, key);
    return v ? json_copy(v) : json_string("");
}

json_t *w14_changewatc_e37815_skin_payload(const struct json_t *config,
                                          const struct json_t *skin) {
    (void)config;  /* init_skin_from_config is stubbed; config only selects the skin */
    if (!json_is_object(skin)) return json_object();  /* live except -> {} */
    json_t *out = json_object();
    json_set(out, "name", field_copy(skin, "name"));
    json_set(out, "colors", field_copy(skin, "colors"));
    json_set(out, "light_colors", field_copy(skin, "light_colors"));
    json_set(out, "dark_colors", field_copy(skin, "dark_colors"));
    json_set(out, "branding", field_copy(skin, "branding"));
    json_set(out, "banner_logo", field_copy(skin, "banner_logo"));
    json_set(out, "banner_hero", field_copy(skin, "banner_hero"));
    json_set(out, "tool_prefix", field_copy(skin, "tool_prefix"));
    json_set(out, "help_header", help_header(skin));
    json_set(out, "customCSS", field_copy(skin, "custom_css"));
    return out;
}

static int cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int str_in(const char *const *list, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (list[i] && !strcmp(list[i], s)) return 1;
    return 0;
}

static int json_equal(const json_t *a, const json_t *b) {
    if (!a && !b) return 1;
    if (!a || !b) return 0;
    char *sa = json_dumps(a, 0), *sb = json_dumps(b, 0);
    int eq = sa && sb && !strcmp(sa, sb);
    free(sa); free(sb);
    return eq;
}

json_t *w14_changewatc_e37815_groups_changed_payload(const struct json_t *before,
                                                     const struct json_t *after,
                                                     const char *epoch) {
    static long long counter = 1;
    long long n = counter++;
    char *moved[128];
    size_t nm = 0;
    if (json_is_object(before)) {
        for (size_t i = 0; i < json_object_size(before) && nm < 128; i++) {
            const char *k = json_object_get_key_at(before, i);
            if (!k || str_in((const char *const *)moved, nm, k)) continue;
            json_t *bv = json_object_get_at(before, i);
            json_t *av = json_obj_get(after, k);
            if (!json_equal(bv, av)) moved[nm++] = dupstr(k);
        }
    }
    if (json_is_object(after)) {
        for (size_t i = 0; i < json_object_size(after) && nm < 128; i++) {
            const char *k = json_object_get_key_at(after, i);
            if (!k || str_in((const char *const *)moved, nm, k)) continue;
            json_t *av = json_object_get_at(after, i);
            json_t *bv = json_obj_get(before, k);
            if (!json_equal(bv, av)) moved[nm++] = dupstr(k);
        }
    }
    json_t *out = json_object();
    int process_wide = str_in((const char *const *)moved, nm, "");
    if (!process_wide) {
        qsort(moved, nm, sizeof(char *), cmp_str);
        json_t *ids = json_array();
        for (size_t i = 0; i < nm; i++) json_append(ids, json_string(moved[i]));
        json_set(out, "room_ids", ids);
    }
    char bid[160];
    snprintf(bid, sizeof(bid), "%s:%lld", epoch ? epoch : "", n);
    json_set(out, "broadcast_id", json_string(bid));
    for (size_t i = 0; i < nm; i++) free(moved[i]);
    return out;
}
