/*
 * t_port_plugin_stream_hooks.c — parity harness for
 * src/agent/plugin_stream_hooks.c (agent/plugin_stream_hooks.py).
 *
 * Ops (JSON "ops", argv[1]); mirrors tests/sta_oracle_plugin_stream_hooks.py:
 *   {"kind":"setup","hooks":{"hook":["cb_a",...]}}
 *   {"kind":"dispatchers_for","hook":...}
 *   {"kind":"enqueue","hook":...,"payload":{...}}
 *   {"kind":"wait_idle"}
 *   {"kind":"calls"}                      -> recorded [name, payload] pairs
 *   {"kind":"reconfigure","hook":...,"names":[...]}
 *   {"kind":"has_stream_observer_hooks"}
 *   {"kind":"reasoning_enabled"} | {"kind":"has_reasoning"}
 *   {"kind":"shutdown"} | {"kind":"enqueue_after_shutdown","hook":...}
 */
#include "hermes_json.h"
#include "plugin_stream_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAMES 16
#define MAX_PAYLOADS 4096

static char g_names[MAX_NAMES][64];
static json_t *g_calls[MAX_NAMES][MAX_PAYLOADS]; /* received payload copies */
static int g_call_n[MAX_NAMES];

static void recorder(void *ctx, const json_t *payload)
{
    int idx = (int)(intptr_t)ctx;
    if (g_call_n[idx] < MAX_PAYLOADS)
        g_calls[idx][g_call_n[idx]++] = json_copy(payload);
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static int find_or_add(const char *name)
{
    for (int i = 0; i < MAX_NAMES; i++)
        if (g_names[i][0] && strcmp(g_names[i], name) == 0) return i;
    for (int i = 0; i < MAX_NAMES; i++) {
        if (!g_names[i][0]) {
            snprintf(g_names[i], sizeof g_names[i], "%s", name);
            return i;
        }
    }
    return -1;
}

static void set_registry(const char *hook, json_t *names_arr)
{
    psh_callback_t cbs[MAX_NAMES];
    size_t n = 0;
    if (names_arr && names_arr->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(names_arr); i++) {
            const char *nm = json_get_str(json_get(names_arr, i), "", NULL);
            int idx = find_or_add(nm ? nm : "");
            cbs[n].name = g_names[idx];
            cbs[n].fn = recorder;
            cbs[n].ctx = (void *)(intptr_t)idx;
            n++;
        }
    }
    plugin_stream_hooks_set_consumer_registry(hook, cbs, n);
}

/* emit all calls as [[name, payload], ...] in name-registration order */
static void emit_calls(json_t *o)
{
    json_t *arr = json_new_array();
    for (int i = 0; i < MAX_NAMES; i++) {
        if (!g_names[i][0]) continue;
        for (int k = 0; k < g_call_n[i]; k++) {
            json_t *pair = json_new_array();
            json_array_append(pair, json_string(g_names[i]));
            json_array_append(pair,
                g_calls[i][k] ? json_copy(g_calls[i][k]) : json_new_null());
            json_array_append(arr, pair);
        }
    }
    json_object_set(o, "out", arr);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_new_object();
            if (strcmp(kind, "setup") == 0 || strcmp(kind, "reconfigure") == 0) {
                json_t *hooks = json_object_get(c, "hooks");
                json_t *hook_one = json_object_get(c, "hook");
                if (hooks && hooks->type == JSON_OBJECT) {
                    size_t nk = json_object_size(hooks);
                    for (size_t k = 0; k < nk; k++) {
                        const char *hk = json_object_get_key_at(hooks, k);
                        if (hk) set_registry(hk, json_object_get(hooks, hk));
                    }
                } else if (hook_one && hook_one->type == JSON_STRING) {
                    set_registry(hook_one->str_val, json_object_get(c, "names"));
                }
                json_object_set(o, "out", json_new_null());
            } else if (strcmp(kind, "dispatchers_for") == 0) {
                json_object_set(o, "out", json_int((long long)
                    plugin_stream_hooks_dispatchers_for(
                        json_get_str(c, "hook", ""))));
            } else if (strcmp(kind, "enqueue") == 0) {
                json_t *pl = json_object_get(c, "payload");
                int q = enqueue_plugin_stream_hook(json_get_str(c, "hook", ""),
                                                   pl && pl->type == JSON_OBJECT
                                                       ? pl : NULL);
                plugin_stream_hooks_wait_idle();
                json_object_set(o, "out", json_bool(q));
            } else if (strcmp(kind, "enqueue_after_shutdown") == 0) {
                int q = enqueue_plugin_stream_hook(
                    json_get_str(c, "hook", ""), NULL);
                json_object_set(o, "out", json_bool(q));
            } else if (strcmp(kind, "wait_idle") == 0) {
                plugin_stream_hooks_wait_idle();
                json_object_set(o, "out", json_new_null());
            } else if (strcmp(kind, "calls") == 0) {
                emit_calls(o);
            } else if (strcmp(kind, "has_stream_observer_hooks") == 0) {
                json_object_set(o, "out",
                                json_bool(has_stream_observer_hooks()));
            } else if (strcmp(kind, "reasoning_enabled") == 0) {
                json_object_set(o, "out",
                                json_bool(stream_reasoning_deltas_enabled()));
            } else if (strcmp(kind, "has_reasoning") == 0) {
                json_object_set(o, "out",
                                json_bool(has_reasoning_stream_observer_hooks()));
            } else if (strcmp(kind, "shutdown") == 0) {
                shutdown_plugin_stream_hook_dispatcher(1.0);
                json_object_set(o, "out", json_new_null());
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
