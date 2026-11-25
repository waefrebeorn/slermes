/* t_port_gw_conn_normalize_flow2.c — harness for the pure-normalizer FLOW port.
 *
 * Reads JSON LINES {"func": <name>, "in": {...}} from stdin; emits JSON LINES
 * {"func","in","out":{"ret","state"}}. No seams (the Python module is pure):
 * "ret" is the function's return value (an uncaught exception rides as
 * {"__raise__": {"type","message"}}); "state" is the caller-held mutable
 * carrier used by the _Normalizer methods and the *clean helpers (sets
 * "labels"/"ids" are emitted sorted by code point).
 */
#include "hermes_json.h"
#include "cli/gw_conn_normalize_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const json_t *arg(const json_t *in, const char *k) { return json_obj_get(in, k); }

static void gc2_ensure_state(json_t *st) {
    if (!gcn2_is_obj(st)) return;
    const char *arrs[] = {"labels", "ids", "connections", "quarantined", NULL};
    for (int i = 0; arrs[i]; i++)
        if (!gcn2_is_arr(json_obj_get(st, arrs[i]))) json_set(st, arrs[i], json_array());
}

static json_t *gc2_run_case(const char *func, json_t *in) {
    json_t *state = json_obj_get(in, "state");
    state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();

    json_t *ret = NULL;
    json_t *clean = NULL;

    if (!strcmp(func, "js_string")) ret = gcn2_js_string_f(arg(in, "value"));
    else if (!strcmp(func, "js_falsy")) ret = gcn2_js_falsy_f(arg(in, "value"));
    else if (!strcmp(func, "js_str")) ret = gcn2_js_str_f(arg(in, "value"));
    else if (!strcmp(func, "label_key")) ret = gcn2_label_key_f(arg(in, "value"));
    else if (!strcmp(func, "unique_label")) ret = gcn2_unique_label_f(arg(in, "candidate"), arg(in, "taken"));
    else if (!strcmp(func, "label_slug")) ret = gcn2_label_slug_f(arg(in, "label"));
    else if (!strcmp(func, "connection_id_for_label")) ret = gcn2_connection_id_for_label_f(arg(in, "label"), arg(in, "taken"));
    else if (!strcmp(func, "norm_auth_mode")) ret = gcn2_norm_auth_mode_f(arg(in, "mode"));
    else if (!strcmp(func, "stored_auth_mode")) ret = gcn2_stored_auth_mode_f(arg(in, "kind"), arg(in, "auth_mode"), arg(in, "token"));
    else if (!strcmp(func, "_url_host")) ret = gcn2_url_host_f(arg(in, "url"));
    else if (!strcmp(func, "normalize_remote_base_url")) ret = gcn2_normalize_remote_base_url(arg(in, "raw"));
    else if (!strcmp(func, "host_label_from_base_url")) ret = gcn2_host_label_from_base_url(arg(in, "base_url"));
    else if (!strcmp(func, "sanitize_header_value")) ret = gcn2_sanitize_header_value_f(arg(in, "value"));
    else if (!strcmp(func, "normalize_remote_headers")) ret = gcn2_normalize_remote_headers(arg(in, "raw"));
    else if (!strcmp(func, "_split_ssh_host")) ret = gcn2_split_ssh_host_f(arg(in, "host"));
    else if (!strcmp(func, "_explicit_port")) ret = gcn2_explicit_port_f(arg(in, "raw"));
    else if (!strcmp(func, "normalize_ssh_config")) ret = gcn2_normalize_ssh_config_f(arg(in, "entry"));
    else if (!strcmp(func, "_ssh_fields")) ret = gcn2_ssh_fields_f(arg(in, "ssh"));
    else if (!strcmp(func, "_local_entry")) ret = gcn2_local_entry(arg(in, "label"));
    else if (!strcmp(func, "_remote_like_clean")) {
        clean = json_obj_get(state, "clean");
        if (!gcn2_is_obj(clean)) { clean = json_object(); json_set(state, "clean", clean); }
        json_t *w = gcn2_remote_like_clean_f(clean, arg(in, "entry"), arg(in, "kind"));
        ret = json_obj_get(w, "reason") ? json_copy(json_obj_get(w, "reason")) : json_null();
        json_set(state, "clean", json_copy(json_obj_get(w, "clean")));
        json_free(w);
    }
    else if (!strcmp(func, "_ssh_clean")) {
        clean = json_obj_get(state, "clean");
        if (!gcn2_is_obj(clean)) { clean = json_object(); json_set(state, "clean", clean); }
        json_t *w = gcn2_ssh_clean_f(clean, arg(in, "entry"));
        ret = json_obj_get(w, "reason") ? json_copy(json_obj_get(w, "reason")) : json_null();
        json_set(state, "clean", json_copy(json_obj_get(w, "clean")));
        json_free(w);
    }
    else if (!strcmp(func, "quarantine")) {
        gc2_ensure_state(state);
        ret = gcn2_quarantine_f(state, arg(in, "reason"), arg(in, "entry"));
    }
    else if (!strcmp(func, "add")) {
        gc2_ensure_state(state);
        ret = gcn2_add_f(state, arg(in, "item"));
    }
    else if (!strcmp(func, "_add_entry")) {
        gc2_ensure_state(state);
        ret = gcn2_add_entry_f(state, arg(in, "entry"));
    }
    else if (!strcmp(func, "normalize_registry")) ret = gcn2_normalize_registry(arg(in, "raw"));
    else if (!strcmp(func, "_url_key")) ret = gcn2_url_key_f(arg(in, "value"));
    else if (!strcmp(func, "_ssh_key")) ret = gcn2_ssh_key_f(arg(in, "conn"));
    else if (!strcmp(func, "_check_label")) ret = gcn2_check_label_f(arg(in, "label"), arg(in, "input_id"), arg(in, "registry"));
    else if (!strcmp(func, "_ssh_input")) ret = gcn2_ssh_input_f(arg(in, "data"), arg(in, "cid"), arg(in, "registry"));
    else if (!strcmp(func, "_remote_input")) ret = gcn2_remote_input_f(arg(in, "data"), arg(in, "cid"), arg(in, "kind"), arg(in, "registry"));
    else if (!strcmp(func, "normalize_connection_input")) ret = gcn2_normalize_connection_input(arg(in, "data"), arg(in, "registry"));
    else if (!strcmp(func, "merge_connection_input")) ret = gcn2_merge_connection_input(arg(in, "data"), arg(in, "existing"));
    else if (!strcmp(func, "_same_url")) ret = gcn2_same_url_f(arg(in, "a"), arg(in, "b"));
    else if (!strcmp(func, "connection_dial_fields_changed")) ret = gcn2_connection_dial_fields_changed(arg(in, "before"), arg(in, "after"));
    else if (!strcmp(func, "upsert_connection")) ret = gcn2_upsert_connection(arg(in, "registry"), arg(in, "entry"));
    else if (!strcmp(func, "remove_connection")) ret = gcn2_remove_connection(arg(in, "registry"), arg(in, "cid"));
    else if (!strcmp(func, "_require")) ret = gcn2_require_f(arg(in, "registry"), arg(in, "cid"));
    else if (!strcmp(func, "set_primary_connection")) ret = gcn2_set_primary_connection(arg(in, "registry"), arg(in, "cid"));
    else if (!strcmp(func, "set_last_used_connection")) ret = gcn2_set_last_used_connection(arg(in, "registry"), arg(in, "cid"));
    else if (!strcmp(func, "set_connection_launch_mode")) ret = gcn2_set_connection_launch_mode(arg(in, "registry"), arg(in, "mode"));
    else { ret = json_object(); json_set(ret, "error", json_string("unknown func")); }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "state", gcn2_state_proj(state));
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = gc2_run_case(func, in);
        json_t *lo = json_object();
        json_set(lo, "func", json_string(func));
        json_set(lo, "in", json_copy(in));
        json_set(lo, "out", out);
        char *ser = json_serialize(lo);
        printf("%s\n", ser ? ser : "{}");
        free(ser); json_free(lo); json_free(rec);
    }
    return 0;
}
