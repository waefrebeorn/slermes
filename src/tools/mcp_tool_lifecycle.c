/*
 * mcp_tool_lifecycle.c — C port of upstream Hermes tools/mcp_tool_lifecycle.py.
 *
 * PoP: w11_mcptoolli_f65aa5_filter_children @ tools/mcp_tool_lifecycle.py:_filter_mcp_children
 * PoP: w11_mcptoolli_f65aa5_take_reapable @ tools/mcp_tool_lifecycle.py:_take_reapable_pids
 * PoP: w11_mcptoolli_f65aa5_clear_cooldowns @ tools/mcp_tool_lifecycle.py:_clear_connect_cooldowns
 * PoP: w11_mcptoolli_f65aa5_stop_loop_if_idle @ tools/mcp_tool_lifecycle.py:_stop_mcp_loop_if_idle
 *
 * psutil cmdlines and process-group state are injected as data seams; no real
 * process is signalled and no psutil/system call is made by this port.
 */
#include "noc_w11_mcptoolli_f65aa5.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const NON_MCP_MARKERS[] = {
    "tui_gateway.slash_worker", "tui_gateway.entry",
    "-dorg.eclipse.equinox.launcher", "eclipse.jdt.ls", "org.eclipse.equinox.launcher_",
};
static const size_t N_MARKERS = sizeof(NON_MCP_MARKERS) / sizeof(NON_MCP_MARKERS[0]);

typedef struct { char **v; size_t n, cap; } strvec;
static void sv_push(strvec *s, const char *x) {
    if (!x) return;
    if (s->n == s->cap) { s->cap = s->cap ? s->cap * 2 : 8; s->v = realloc(s->v, s->cap * sizeof(char *)); }
    s->v[s->n++] = strdup(x);
}
static void sv_free(strvec *s) {
    for (size_t i = 0; i < s->n; i++) free(s->v[i]);
    free(s->v); s->v = NULL; s->n = s->cap = 0;
}
static int cmp_str(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }
static json_t *sv_sorted_unique(const strvec *s) {
    if (!s->n) return json_array();
    char **tmp = malloc(s->n * sizeof(char *));
    for (size_t i = 0; i < s->n; i++) tmp[i] = s->v[i];
    qsort(tmp, s->n, sizeof(char *), cmp_str);
    json_t *out = json_array();
    for (size_t i = 0; i < s->n; i++) {
        if (i && !strcmp(tmp[i], tmp[i - 1])) continue;
        json_append(out, json_string(tmp[i]));
    }
    free(tmp);
    return out;
}

static json_t *copy_object_minus(const json_t *src, const strvec *drop) {
    json_t *out = json_object();
    if (!src || src->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(src); i++) {
        const char *k = json_object_get_key_at(src, i);
        int gone = 0;
        for (size_t j = 0; j < drop->n; j++) if (!strcmp(drop->v[j], k)) { gone = 1; break; }
        if (!gone) json_set(out, k, json_copy(json_object_get_at(src, i)));
    }
    return out;
}

json_t *w11_mcptoolli_f65aa5_filter_children(const struct json_t *pids,
        const struct json_t *cmdlines, int psutil_available, const char *supervisor_script) {
    strvec kept = {0}, all = {0};
    if (pids && pids->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(pids); i++) {
            json_t *e = json_get(pids, i);
            if (e && e->type == JSON_STRING) sv_push(&all, e->str_val);
        }
    if (!all.n || !psutil_available) {
        json_t *out = sv_sorted_unique(&all);
        sv_free(&all); sv_free(&kept);
        return out;
    }
    for (size_t i = 0; i < all.n; i++) {
        const char *pid = all.v[i];
        json_t *argv = json_obj_get(cmdlines, pid);
        if (!argv || argv->type != JSON_ARRAY) continue; /* raced away / zombie */
        size_t n = json_len(argv);
        if (supervisor_script && n >= 2) {
            json_t *a1 = json_get(argv, 1);
            if (a1 && a1->type == JSON_STRING && !strcmp(a1->str_val, supervisor_script)) continue;
        }
        int marker_hit = 0;
        for (size_t a = 1; a < n; a++) {
            json_t *ae = json_get(argv, a);
            if (!ae || ae->type != JSON_STRING || !ae->str_val) continue;
            for (size_t m = 0; m < N_MARKERS; m++)
                if (strstr(ae->str_val, NON_MCP_MARKERS[m])) { marker_hit = 1; break; }
            if (marker_hit) break;
        }
        if (!marker_hit) sv_push(&kept, pid);
    }
    json_t *out = sv_sorted_unique(&kept);
    sv_free(&all); sv_free(&kept);
    return out;
}

json_t *w11_mcptoolli_f65aa5_take_reapable(int include_active,
        const struct json_t *server_name,
        const struct json_t *orphan_pids, const struct json_t *orphan_servers,
        const struct json_t *stdio_pids, const struct json_t *stdio_pgids,
        const struct json_t *stdio_starttimes) {
    int sn_is_null = (!server_name || server_name->type == JSON_NULL);
    const char *sn = (!sn_is_null && server_name->type == JSON_STRING) ? server_name->str_val : "";

    json_t *pids = json_object();
    strvec popped_orphans = {0}, popped_active = {0}, popped_pgids = {0}, popped_starts = {0};

    if (orphan_pids && orphan_pids->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(orphan_pids); i++) {
            json_t *e = json_get(orphan_pids, i);
            const char *pid = (e && e->type == JSON_STRING) ? e->str_val : NULL;
            if (!pid) continue;
            const char *owner = json_get_str(orphan_servers, pid, "orphan");
            if (sn_is_null || !strcmp(owner, sn)) {
                json_set(pids, pid, json_string(owner));
                sv_push(&popped_orphans, pid);
            }
        }
    }
    if (include_active && stdio_pids && stdio_pids->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(stdio_pids); i++) {
            const char *pid = json_object_get_key_at(stdio_pids, i);
            json_t *ov = json_object_get_at(stdio_pids, i);
            const char *owner = (ov && ov->type == JSON_STRING) ? ov->str_val : "";
            if (sn_is_null || !strcmp(owner, sn)) {
                json_set(pids, pid, json_copy(ov));
                sv_push(&popped_active, pid);
            }
        }
    }
    json_t *pgids = json_object(), *starts = json_object();
    for (size_t i = 0; i < json_object_size(pids); i++) {
        const char *pid = json_object_get_key_at(pids, i);
        json_t *pv = json_obj_get(stdio_pgids, pid);
        if (pv && pv->type != JSON_NULL) { json_set(pgids, pid, json_copy(pv)); sv_push(&popped_pgids, pid); }
        json_t *sv = json_obj_get(stdio_starttimes, pid);
        if (sv && sv->type != JSON_NULL) { json_set(starts, pid, json_copy(sv)); sv_push(&popped_starts, pid); }
    }

    json_t *ret = json_array();
    json_append(ret, pids); json_append(ret, pgids); json_append(ret, starts);

    json_t *state = json_object();
    /* orphan_pids is an ARRAY: keep every pid that was not popped. */
    {
        json_t *a = json_array();
        if (orphan_pids && orphan_pids->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(orphan_pids); i++) {
                json_t *e = json_get(orphan_pids, i);
                const char *pid = (e && e->type == JSON_STRING) ? e->str_val : NULL;
                int gone = 0;
                for (size_t j = 0; j < popped_orphans.n; j++) if (!strcmp(popped_orphans.v[j], pid)) { gone = 1; break; }
                if (!gone) json_append(a, json_copy(e));
            }
        json_set(state, "orphan_pids", a);
    }
    json_set(state, "orphan_servers", copy_object_minus(orphan_servers, &popped_orphans));
    json_set(state, "stdio_pids", copy_object_minus(stdio_pids, &popped_active));
    json_set(state, "stdio_pgids", copy_object_minus(stdio_pgids, &popped_pgids));
    json_set(state, "stdio_starttimes", copy_object_minus(stdio_starttimes, &popped_starts));

    json_t *out = json_object();
    json_set(out, "ret", ret);
    json_set(out, "state", state);
    sv_free(&popped_orphans); sv_free(&popped_active); sv_free(&popped_pgids); sv_free(&popped_starts);
    return out;
}

json_t *w11_mcptoolli_f65aa5_clear_cooldowns(const struct json_t *keys,
        const struct json_t *retry_after, const struct json_t *failures) {
    json_t *ra = (retry_after && retry_after->type == JSON_OBJECT) ? json_copy(retry_after) : json_object();
    json_t *fa = (failures && failures->type == JSON_OBJECT) ? json_copy(failures) : json_object();
    if (!keys || keys->type == JSON_NULL) {
        json_free(ra); json_free(fa);
        ra = json_object(); fa = json_object();
    } else if (keys->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(keys); i++) {
            json_t *e = json_get(keys, i);
            if (e && e->type == JSON_STRING) { json_obj_del(ra, e->str_val); json_obj_del(fa, e->str_val); }
        }
    }
    json_t *out = json_object();
    json_set(out, "retry_after", ra);
    json_set(out, "failures", fa);
    return out;
}

int w11_mcptoolli_f65aa5_stop_loop_if_idle(int loop_result) { return loop_result ? 1 : 0; }
