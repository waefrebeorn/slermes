/*
 * t_port_platform_actions.c — parity harness for
 * src/hermes_cli/platform_actions.c (hermes_cli/platform_actions.py).
 * Fixture ops:
 *  {"kind":"add_reaction"|"set_thread_title", "granted":b, "runner":bool,
 *   "platform":..., "chat_id","message_id"/"thread_id"/"title",
 *   "connected":b, "registered":b, "client":bool, "channel_found":bool,
 *   "fetch_ok":bool, "op_ok":bool}
 * Emits {"out":<result dict>,"audit":<log line>} per op, mirroring
 * tests/sta_oracle_platform_actions.py.
 */
#include "hermes_json.h"
#include "platform_actions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

/* scripted environment + adapter impls */
typedef struct {
    int granted, has_runner, registered, connected, client_present;
    int channel_found, fetch_ok, op_ok;
    const pa_adapter_t *adapter;
} env_t;
static env_t g_env;

static int cap_granted(const char *pid, const char *cap, void *ctx) {
    (void)pid; (void)cap; return ((env_t *)ctx)->granted;
}
static void *get_runner(void *ctx) {
    return ((env_t *)ctx)->has_runner ? (void *)1 : NULL;
}
static const pa_adapter_t *runner_adapter(void *runner, const char *pv) {
    (void)runner; (void)pv;
    env_t *e = &g_env;
    return e->registered ? e->adapter : NULL;
}

static int tg_set_reaction(void *impl, const char *c, const char *m, const char *e) {
    (void)impl; (void)c; (void)m; (void)e; return g_env.op_ok ? 1 : 0;
}
static int tg_rename_dm_topic(void *impl, const char *c, long t, const char *ti) {
    (void)impl; (void)c; (void)t; (void)ti; return g_env.op_ok ? 0 : -1;
}
static int dc_rename_thread(void *impl, const char *t, const char *ti) {
    (void)impl; (void)t; (void)ti; return g_env.op_ok ? 1 : 0;
}
static void *dc_get_channel(void *client, long id) {
    (void)client; (void)id; return g_env.channel_found ? (void *)1 : NULL;
}
static void *dc_fetch_channel(void *client, long id) {
    (void)client; (void)id; return g_env.fetch_ok ? (void *)1 : NULL;
}
static void *dc_fetch_message(void *cc, void *channel, long id) {
    (void)cc; (void)channel; (void)id; return channel ? (void *)1 : NULL;
}
static int dc_add_reaction(void *cc, void *message, const char *emoji) {
    (void)cc; (void)message; (void)emoji; return g_env.op_ok ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    static const char *known[] = {"telegram", "discord", "slack"};
    pa_adapter_t adapter;
    memset(&adapter, 0, sizeof adapter);
    adapter.is_connected = 1;

    char *raw = read_file(argv[1]);
    json_t *rootj = json_parse(raw, NULL);
    if (!rootj) { fprintf(stderr, "bad fixture\n"); return 1; }
    json_t *ops = json_object_get(rootj, "ops");
    for (size_t i = 0; ops && i < json_len(ops); i++) {
        json_t *c = json_get(ops, i);
        const char *kind = json_get_str(c, "kind", "");
        memset(&g_env, 0, sizeof g_env);
        g_env.fetch_ok = json_get_bool(c, "fetch_ok", 1);
        g_env.op_ok = json_get_bool(c, "op_ok", 1);
        g_env.granted = json_get_bool(c, "granted", 0);
        g_env.has_runner = json_get_bool(c, "runner", 1);
        g_env.registered = json_get_bool(c, "registered", 1);
        g_env.connected = json_get_bool(c, "connected", 1);
        g_env.client_present = json_get_bool(c, "client", 1);
        g_env.channel_found = json_get_bool(c, "channel_found", 0);

        const char *plat = json_get_str(c, "platform", "");
        if (strcmp(plat, "telegram") == 0) {
            adapter.platform_value = "telegram";
            adapter.set_reaction = tg_set_reaction;
            adapter.rename_dm_topic = tg_rename_dm_topic;
            adapter.rename_thread = NULL;
            adapter.client = NULL;
        } else if (strcmp(plat, "discord") == 0) {
            adapter.platform_value = "discord";
            adapter.set_reaction = NULL;
            adapter.rename_dm_topic = NULL;
            adapter.rename_thread = dc_rename_thread;
            adapter.client = g_env.client_present ? (void *)1 : NULL;
            adapter.get_channel = dc_get_channel;
            adapter.fetch_channel = dc_fetch_channel;
            adapter.fetch_message = dc_fetch_message;
            adapter.add_reaction = dc_add_reaction;
        } else {
            adapter.platform_value = "slack";
            adapter.set_reaction = NULL;
            adapter.rename_dm_topic = NULL;
            adapter.rename_thread = NULL;
            adapter.client = NULL;
            adapter.get_channel = NULL;
            adapter.fetch_channel = NULL;
            adapter.fetch_message = NULL;
            adapter.add_reaction = NULL;
        }
        adapter.is_connected = g_env.connected;
        g_env.adapter = &adapter;

        platform_actions_t *pa = platform_actions_new("my-plugin");
        pa->capability_granted = cap_granted; pa->cap_ctx = &g_env;
        pa->get_runner = get_runner; pa->runner_ctx = &g_env;
        pa->runner_adapter = runner_adapter; pa->adapter_ctx = &g_env;
        pa->known_platforms = known; pa->n_known_platforms = 3;

        json_t *res = NULL;
        if (strcmp(kind, "add_reaction") == 0)
            res = platform_actions_add_reaction(pa, plat,
                json_get_str(c, "chat_id", ""), json_get_str(c, "message_id", ""),
                json_get_str(c, "emoji", "\xf0\x9f\x91\x8d"));
        else if (strcmp(kind, "set_thread_title") == 0)
            res = platform_actions_set_thread_title(pa, plat,
                json_get_str(c, "chat_id", ""), json_get_str(c, "thread_id", ""),
                json_get_str(c, "title", ""));
        json_t *o = json_new_object();
        json_object_set(o, "out", res ? res : json_new_null());
        char *audit = res ? platform_actions_audit(pa, kind, plat, res) : strdup("");
        json_object_set(o, "audit", json_string(audit));
        free(audit);
        char *line = json_dumps(o, 0);
        printf("%s\n", line);
        free(line);
        json_free(o);
        platform_actions_free(pa);  /* res owned by o via json_object_set */
    }
    json_free(rootj);
    free(raw);
    return 0;
}
