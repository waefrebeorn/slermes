/* gate: codex_runtime_plugin_migration_translate_one_server — traced copy of
 * the FIXED src/hermes_cli/ports_wrappers.c::
 * hermes_cli_codex_runtime_plugi_u_translate_one_server (real translation
 * logic replaced the earlier echo wrapper — parity win).
 * LIVE twin: hermes_cli/codex_runtime_plugin_migration.py::_translate_one_server.
 *
 * Input: one Hermes MCP server config as a JSON object string.
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: not an object -> entry null, empty skipped
 *   B2: both command and url set (skipped "url (...)" + V)
 *   B3/B4/B5: stdio / http / neither (neither -> null entry + V "no command...")
 *     stdio sub-branches: args present, env present, cwd present
 *     http sub-branches: headers present, transport == sse (+V)
 *   LOOP timeouts [timeout, connect_timeout]: L tick, present?, numeric?
 *   B: enabled is False
 *   LOOP over config keys: L tick, dropped-with-warning?, unknown?
 * Each skipped reason is emitted as a V event (WCUP_VALSTR / c.valstr).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"
#include "watercup.h"

static const char *const KNOWN[] = {"command","args","env","cwd","url",
    "headers","transport","timeout","connect_timeout","enabled","description",NULL};
static const char *const DROPPED[] = {"sampling",NULL};

static char *wc_translate(const char *cfg_json) {
    WCUP_BEGIN("translate_one_server");
    bool ok_entry = false;
    json_t *cfg = cfg_json ? json_parse(cfg_json, NULL) : NULL;
    if (!json_is_object(cfg)) {                       /* B1 */
        WCUP_BRANCH(1);
        goto done_none;
    }
    WCUP_BRANCH(0);
    {
        json_t *out = json_object();
        json_t *skipped = json_array();
        const json_t *v;

        bool has_command = json_string_value_safe(json_obj_get(cfg, "command")) &&
                           *json_string_value_safe(json_obj_get(cfg, "command"));
        bool has_url = json_string_value_safe(json_obj_get(cfg, "url")) &&
                       *json_string_value_safe(json_obj_get(cfg, "url"));

        if (has_command && has_url) {                 /* B2 */
            WCUP_BRANCH(1);
            const char *msg = "url (both command and url set; preferring stdio)";
            WCUP_VALSTR(msg);
            json_append(skipped, json_string(msg));
            has_url = false;
        } else WCUP_BRANCH(0);

        if (has_command) {                            /* B3 stdio */
            WCUP_BRANCH(1);
            v = json_obj_get(cfg, "command");
            json_set(out, "command", json_string(json_string_value(v)));
            const json_t *args = json_obj_get(cfg, "args");
            if (args && json_is_array(args) && json_len(args) > 0) {  /* args present */
                WCUP_BRANCH(1);
                json_t *arr = json_array();
                for (size_t i = 0; i < json_len(args); i++) {
                    WCUP_LOOP_TICK();
                    const json_t *a = json_get(args, i);
                    if (a && json_is_string(a)) json_append(arr, json_string(a->str_val));
                    else json_append(arr, json_string(""));
                }
                json_set(out, "args", arr);
            } else WCUP_BRANCH(0);
            const json_t *env = json_obj_get(cfg, "env");
            if (env && json_is_object(env) && json_object_size(env) > 0) {  /* env present */
                WCUP_BRANCH(1);
                json_t *e = json_object();
                for (size_t i = 0; ; i++) {
                    const char *k = json_object_get_key_at(env, i);
                    if (!k) break;
                    WCUP_LOOP_TICK();
                    const json_t *ev = json_object_get_at(env, i);
                    if (ev && json_is_string(ev)) json_set(e, k, json_string(ev->str_val));
                    else json_set(e, k, json_string(""));
                }
                json_set(out, "env", e);
            } else WCUP_BRANCH(0);
            const char *cwd = json_string_value_safe(json_obj_get(cfg, "cwd"));
            if (cwd && *cwd) { WCUP_BRANCH(1); json_set(out, "cwd", json_string(cwd)); }
            else WCUP_BRANCH(0);
            ok_entry = true;
        } else {                                      /* not stdio */
            WCUP_BRANCH(0);
            if (has_url) {                            /* B4 http */
                WCUP_BRANCH(1);
            v = json_obj_get(cfg, "url");
            json_set(out, "url", json_string(json_string_value(v)));
            const json_t *headers = json_obj_get(cfg, "headers");
            if (headers && json_is_object(headers) && json_object_size(headers) > 0) {
                WCUP_BRANCH(1);
                json_t *h = json_object();
                for (size_t i = 0; ; i++) {
                    const char *k = json_object_get_key_at(headers, i);
                    if (!k) break;
                    WCUP_LOOP_TICK();
                    const json_t *hv = json_object_get_at(headers, i);
                    if (hv && json_is_string(hv)) json_set(h, k, json_string(hv->str_val));
                }
                json_set(out, "http_headers", h);
            } else WCUP_BRANCH(0);
            const char *transport = json_string_value_safe(json_obj_get(cfg, "transport"));
            if (transport && strcmp(transport, "sse") == 0) {
                WCUP_BRANCH(1);
                const char *msg = "transport=sse (codex auto-negotiates)";
                WCUP_VALSTR(msg);
                json_append(skipped, json_string(msg));
            } else WCUP_BRANCH(0);
            ok_entry = true;
            } else {                                  /* B5 neither */
                WCUP_BRANCH(0);
                const char *msg = "no command or url field";
                WCUP_VALSTR(msg);
                ok_entry = false;
                goto done_none_msg;
            }
        }

        /* Timeouts */
        const char *tkeys[2] = {"timeout", "connect_timeout"};
        const char *okeys[2] = {"tool_timeout_sec", "startup_timeout_sec"};
        const char *smsgs[2] = {"timeout (not numeric)", "connect_timeout (not numeric)"};
        for (int t = 0; t < 2; t++) {
            WCUP_LOOP_TICK();
            const json_t *tv = json_obj_get(cfg, tkeys[t]);
            if (!tv) { WCUP_BRANCH(0); WCUP_BRANCH(0); continue; }  /* absent: two F branches */
            WCUP_BRANCH(1);
            if (tv->type == JSON_NUMBER || tv->type == JSON_BOOL) {
                WCUP_BRANCH(1);
                json_set(out, okeys[t], json_number(tv->num_val));
            } else {
                WCUP_BRANCH(0);
                WCUP_VALSTR(smsgs[t]);
                json_append(skipped, json_string(smsgs[t]));
            }
        }

        const json_t *en = json_obj_get(cfg, "enabled");
        if (en && en->type == JSON_BOOL && !en->bool_val) {
            WCUP_BRANCH(1);
            json_set(out, "enabled", json_bool(false));
        } else WCUP_BRANCH(0);

        for (size_t i = 0; ; i++) {                   /* LOOP keys */
            const char *k = json_object_get_key_at(cfg, i);
            if (!k) break;
            WCUP_LOOP_TICK();
            bool known = false, dropped = false;
            for (int j = 0; KNOWN[j]; j++) if (strcmp(k, KNOWN[j]) == 0) known = true;
            for (int j = 0; DROPPED[j]; j++) if (strcmp(k, DROPPED[j]) == 0) dropped = true;
            char msg[160];
            if (dropped) {
                WCUP_BRANCH(1); WCUP_BRANCH(0);
                snprintf(msg, sizeof msg, "%s (no codex equivalent)", k);
                WCUP_VALSTR(msg);
                json_append(skipped, json_string(msg));
            } else if (!known) {
                WCUP_BRANCH(0); WCUP_BRANCH(1);
                snprintf(msg, sizeof msg, "%s (unknown Hermes key)", k);
                WCUP_VALSTR(msg);
                json_append(skipped, json_string(msg));
            } else {
                WCUP_BRANCH(0); WCUP_BRANCH(0);
            }
        }
        json_free(out);
        goto finalize_ok;

done_none_msg:
        json_free(skipped);
        goto done_none;
    }

finalize_ok:
    WCUP_BRANCH(ok_entry);
    goto out;
done_none:
    WCUP_BRANCH(0);

out:;
    {
        char cup[32], sval[8];
        snprintf(sval, sizeof sval, "%d", ok_entry ? 1 : 0);
        WCUP_VALSTR(sval);
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("translate_one_server");
        if (cfg) json_free(cfg);
        return NULL;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s translate_one_server <cfg-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_translate(argv[i]);
    return 0;
}
