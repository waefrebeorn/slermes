/* t_port_gw_connections_flow2.c — harness for gateway_connections.py FLOW port.
 *
 * Reads JSON LINES {"func": <name>, "in": {...}} from stdin; emits JSON LINES
 * {"func","in","out":{"ret","state"}}. No mutable carrier (all ported funcs are
 * pure or seam-driven). "ret" carries an uncaught exception as
 * {"__raise__":{"type","message"}}. The "norm" input field is the INJECTED
 * result of normalize_remote_base_url for _entry_url/_bound_url.
 */
#include "hermes_json.h"
#include "cli/gw_connections_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const json_t *arg(const json_t *in, const char *k) { return json_obj_get(in, k); }

static json_t *gc2_run_case(const char *func, json_t *in) {
    json_t *ret = NULL;

    if (!strcmp(func, "revision")) ret = gco2_revision(arg(in, "doc"));
    else if (!strcmp(func, "plain")) ret = gco2_plain(arg(in, "secret"));
    else if (!strcmp(func, "kept_envelope")) ret = gco2_kept_envelope(arg(in, "token"), arg(in, "existing"));
    else if (!strcmp(func, "envelope")) ret = gco2_envelope(arg(in, "value"));
    else if (!strcmp(func, "token_preview")) ret = gco2_token_preview(arg(in, "value"));
    else if (!strcmp(func, "public_connection")) ret = gco2_public_connection(arg(in, "entry"));
    else if (!strcmp(func, "public_registry")) ret = gco2_public_registry(arg(in, "reg"));
    else if (!strcmp(func, "incoming_headers")) ret = gco2_incoming_headers(arg(in, "raw"), arg(in, "existing"));
    else if (!strcmp(func, "signed_in_ids_gone")) ret = gco2_signed_in_ids_gone(arg(in, "before"), arg(in, "after"));
    else if (!strcmp(func, "entry_url")) ret = gco2_entry_url(arg(in, "entry"), arg(in, "norm"));
    else if (!strcmp(func, "bound_url")) ret = gco2_bound_url(arg(in, "token_set"), arg(in, "norm"));
    else { ret = json_object(); json_set(ret, "error", json_string("unknown func")); }

    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : (json_t *)json_null());
    json_set(out, "state", json_object());
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
