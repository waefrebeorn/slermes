/*
 * t_port_final10.c — parity harness for the upstream-sync final-10 ports:
 *
 *   tools/browser_extension_router.c      route_browser_tool decision matrix
 *   hermes_cli/process_identity.c         parse_spawn_tag
 *   hermes_cli/relay_plugin_cutover.c     legacy keys + configured env vars
 *   hermes_cli/update_inventory.c         _restart_mechanism, supervisor
 *   hermes_cli/worktree_cmd.c             _fmt_size
 *   gateway/browser_control_artifacts.c   scope key, MIME, bounded filename
 *   gateway/browser_control_broker.c      protocol_supported, filter_caps
 *   gateway/control_socket.c              home_hash + path resolution
 *
 * Reads a JSON-array fixture from argv[1]; emits one JSON line per case.
 * The Python twin (tests/sta_oracle_final10.py) recomputes each case from
 * the LIVE Python modules; the runner diffs byte-for-byte.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "relay_plugin_cutover.h"
#include "worktree_cmd.h"
#include "process_identity.h"
#include "browser_control_artifacts.h"
#include "browser_control_broker.h"
#include "control_socket.h"
#include "browser_extension_router.h"
#include "update_inventory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- fake broker for the route_browser_tool matrix ------------------ */
typedef struct {
    int scope_return;    /* 1 → scope_for_session returns a handle */
    int lane_registered; /* lane_registered return value */
    int select_return;
    const char *dispatch_result;
} fake_broker_t;

static fake_broker_t g_fake;

static int bcr_flag(json_t *c, const char *key)
{
    json_t *v = json_obj_get(c, key);
    if (!v) return 0;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return 0;
}

static void *fb_scope_for_session(void *ctx, const char *s, const char *t,
                                  const char *p, const char *f)
{
    (void)ctx; (void)s; (void)t; (void)p; (void)f;
    return g_fake.scope_return ? (void *)&g_fake : NULL;
}
static int fb_select(void *ctx, void *scope, const char *cap)
{
    (void)ctx; (void)scope; (void)cap;
    return g_fake.select_return;
}
static char *fb_dispatch(void *ctx, void *scope, const char *action,
                         const char *args, const char *tid)
{
    (void)ctx; (void)scope; (void)action; (void)args; (void)tid;
    return strdup(g_fake.dispatch_result);
}
static int fb_lane_registered(void *ctx, const char *s, const char *t,
                              const char *p, const char *f)
{
    (void)ctx; (void)s; (void)t; (void)p; (void)f;
    return g_fake.lane_registered;
}
static char *fb_fallback(void *ctx)
{
    (void)ctx;
    return strdup("FALLBACK");
}

static json_t *emit(const char *op, json_t *payload)
{
    json_t *line = json_object();
    json_set(line, "op", json_string(op));
    if (payload)
        json_set(line, "out", payload);
    else
        json_set(line, "out", json_null());
    char *text = json_serialize(line);
    printf("%s\n", text ? text : "{}");
    free(text);
    json_free(line);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s fixture.json\n", argv[0]); return 2; }
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) return 2;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *text = malloc((size_t)sz + 1);
    size_t rd = fread(text, 1, (size_t)sz, fp);
    fclose(fp);
    text[rd] = '\0';
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) return 2;

    for (size_t i = 0; i < json_len(cases); i++) {
        json_t *c = json_get(cases, i);
        if (!c) continue;
        json_t *opn = json_obj_get(c, "op");
        const char *op = (opn && opn->type == JSON_STRING) ? opn->str_val : "";

        if (strcmp(op, "cutover_keys") == 0) {
            json_t *vals = json_obj_get(c, "values");
            const char **arr = NULL;
            size_t n = 0;
            if (vals && vals->type == JSON_ARRAY && json_len(vals)) {
                arr = malloc(json_len(vals) * sizeof(char *));
                for (size_t j = 0; j < json_len(vals); j++) {
                    json_t *v = json_get(vals, j);
                    if (v && v->type == JSON_STRING) arr[n++] = v->str_val;
                }
            }
            size_t out_n = 0;
            char **keys =
                cutover_legacy_relay_plugin_keys(arr, n, &out_n);
            json_t *outa = json_array();
            for (size_t j = 0; j < out_n; j++)
                json_append(outa, json_string(keys[j]));
            emit(op, outa);
            free(arr);
            for (size_t j = 0; j < out_n; j++) free(keys[j]);
            free(keys);
        } else if (strcmp(op, "cutover_env") == 0) {
            json_t *pairs = json_obj_get(c, "env");
            size_t n = pairs && pairs->type == JSON_ARRAY ? json_len(pairs) : 0;
            const char **names = calloc(n ? n : 1, sizeof(char *));
            const char **values = calloc(n ? n : 1, sizeof(char *));
            for (size_t j = 0; j < n; j++) {
                json_t *p = json_get(pairs, j);
                if (!p || p->type != JSON_OBJECT) continue;
                json_t *nm = json_obj_get(p, "name");
                json_t *vl = json_obj_get(p, "value");
                names[j] = (nm && nm->type == JSON_STRING) ? nm->str_val : "";
                values[j] = (vl && vl->type == JSON_STRING) ? vl->str_val : "";
            }
            size_t out_n = 0;
            char **vars = cutover_configured_legacy_relay_env_vars(
                names, values, n, &out_n);
            json_t *outa = json_array();
            for (size_t j = 0; j < out_n; j++)
                json_append(outa, json_string(vars[j]));
            emit(op, outa);
            free(names); free(values);
            for (size_t j = 0; j < out_n; j++) free(vars[j]);
            free(vars);
        } else if (strcmp(op, "fmt_size") == 0) {
            int has = 0, mb = 0;
            json_t *hn = json_obj_get(c, "has_size");
            json_t *mbn = json_obj_get(c, "size_mb");
            has = hn ? hn->bool_val : 0;
            mb = mbn ? (int)mbn->num_val : 0;
            char *out = worktree_cmd_fmt_size(has, mb);
            emit(op, json_string(out));
            free(out);
        } else if (strcmp(op, "parse_spawn_tag") == 0) {
            json_t *raw = json_obj_get(c, "raw");
            process_identity_spawn_tag_t tag;
            json_t *outo = NULL;
            if (process_identity_parse_spawn_tag(
                    (raw && raw->type == JSON_STRING) ? raw->str_val : NULL,
                    &tag) == 0) {
                outo = json_object();
                json_set(outo, "install", json_string(tag.install));
                json_set(outo, "purpose", json_string(tag.purpose));
                json_set(outo, "spawner_pid", json_int((int)tag.spawner_pid));
                if (tag.has_spawner_create)
                    json_set(outo, "spawner_create",
                             json_double(tag.spawner_create));
                else
                    json_set(outo, "spawner_create", json_null());
            }
            emit(op, outo);
        } else if (strcmp(op, "scope_key") == 0) {
            json_t *pn = json_obj_get(c, "principal");
            json_t *fn = json_obj_get(c, "family");
            bca_scope_t scope = {
                (pn && pn->type == JSON_STRING) ? pn->str_val : "",
                (fn && fn->type == JSON_STRING) ? fn->str_val : "",
            };
            char key[65];
            bca_error_t rc = bca_artifact_scope_key(&scope, key);
            if (rc == BCA_OK)
                emit(op, json_string(key));
            else
                emit(op, json_null()); /* ArtifactError */
        } else if (strcmp(op, "normalize_mime") == 0) {
            json_t *v = json_obj_get(c, "value");
            char out[128];
            bca_normalize_content_type(v ? v->str_val : "", out, sizeof(out));
            emit(op, json_string(out));
        } else if (strcmp(op, "bounded_filename") == 0) {
            json_t *v = json_obj_get(c, "value");
            char *out = bca_bounded_filename(v ? v->str_val : "");
            emit(op, json_string(out));
            free(out);
        } else if (strcmp(op, "proto_supported") == 0) {
            json_t *v = json_obj_get(c, "value");
            emit(op, json_bool(bcb_browser_control_protocol_supported(v)));
        } else if (strcmp(op, "filter_caps") == 0) {
            json_t *caps = json_obj_get(c, "caps");
            json_t *dm = json_obj_get(c, "developer_mode");
            int devmode = dm ? (dm->type == JSON_NULL ? -1 : dm->bool_val)
                             : -1;
            size_t n = 0;
            char **allowed = bcb_filter_browser_control_capabilities(
                caps, devmode, &n);
            /* canonical (sorted) order for diffing — Python returns an
             * unordered frozenset; both sides emit sorted lists. */
            for (size_t a = 0; a + 1 < n; a++)
                for (size_t b = a + 1; b < n; b++)
                    if (strcmp(allowed[a], allowed[b]) > 0) {
                        char *tmp = allowed[a];
                        allowed[a] = allowed[b];
                        allowed[b] = tmp;
                    }
            json_t *outa = json_array();
            for (size_t j = 0; j < n; j++) json_append(outa, json_string(allowed[j]));
            emit(op, outa);
            for (size_t j = 0; j < n; j++) free(allowed[j]);
            free(allowed);
        } else if (strcmp(op, "home_hash") == 0) {
            json_t *v = json_obj_get(c, "path");
            char *h = control_socket_home_hash(v ? v->str_val : "");
            emit(op, h ? json_string(h) : json_null());
            free(h);
        } else if (strcmp(op, "socket_paths") == 0) {
            json_t *v = json_obj_get(c, "path");
            const char *home = v ? v->str_val : "/tmp";
            char *def = control_socket_default_socket_path(home);
            char *fallback = control_socket_fallback_socket_path(home);
            char *pointer = NULL;
            char *bind =
                control_socket_resolve_server_socket_path(home, &pointer);
            json_t *outo = json_object();
            json_set(outo, "default_len", json_int((int)strlen(def)));
            json_set(outo, "fallback_is_tmp", json_bool(
                strncmp(fallback, "/tmp/", 5) == 0));
            json_set(outo, "bind_is_default",
                     json_bool(strcmp(bind, def) == 0));
            json_set(outo, "has_pointer", json_bool(pointer != NULL));
            emit(op, outo);
            free(def); free(fallback); free(bind); free(pointer);
        } else if (strcmp(op, "restart_mechanism") == 0) {
            json_t *sn = json_obj_get(c, "supervisor");
            json_t *pn = json_obj_get(c, "profile");
            char *out = update_inventory_restart_mechanism(
                sn ? sn->str_val : "manual", pn ? pn->str_val : "default");
            emit(op, json_string(out));
            free(out);
        } else if (strcmp(op, "supervisor_for_pid") == 0) {
            /* outside-in scan with an empty service-pid set: pure branch */
            char out[16];
            update_inventory_detect_supervisor_for_pid(12345, NULL, 0, out);
            emit(op, json_string(out));
        } else if (strcmp(op, "route") == 0) {
            g_fake.scope_return = bcr_flag(c, "scope");
            g_fake.lane_registered = bcr_flag(c, "lane");
            g_fake.select_return = bcr_flag(c, "select");
            json_t *dr = json_obj_get(c, "dispatch_result");
            g_fake.dispatch_result =
                (dr && dr->type == JSON_STRING) ? dr->str_val : "{\"ok\":1}";
            json_t *en = json_obj_get(c, "enabled");
            json_t *prin = json_obj_get(c, "principal");
            json_t *fam = json_obj_get(c, "family");
            bcr_broker_vtab_t vtab = {NULL, fb_scope_for_session, fb_select,
                                      fb_dispatch, fb_lane_registered};
            char *result = NULL;
            bcr_route_result_t rc = bcr_route_browser_tool(
                "browser_navigate", "{\"x\":1}", fb_fallback, NULL, &vtab,
                en ? en->bool_val : 0, "sess1", "task1",
                (prin && prin->type == JSON_STRING) ? prin->str_val : "",
                (fam && fam->type == JSON_STRING) ? fam->str_val : "", "tc1",
                &result);
            json_t *outo = json_object();
            const char *names[] = {"ROUTED", "FELL_BACK", "ERROR"};
            json_set(outo, "status",
                     json_string(rc == BCR_ROUTED  ? "ROUTED"
                                 : rc == BCR_FELL_BACK ? "FELL_BACK"
                                                       : "ERROR"));
            json_set(outo, "result",
                     result ? json_string(result) : json_null());
            emit(op, outo);
            free(result);
        } else {
            emit("unknown", json_string(op));
        }
    }
    json_free(cases);
    return 0;
}
