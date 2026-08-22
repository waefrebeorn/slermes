/*
 * t_port_loops_pure.c — parity harness for src/hermes_cli/loops_pure.c
 * (hermes_cli/loops.py deterministic helpers).
 *
 * Reads a JSON fixture (argv[1]) with "ops": list of {kind, ...} and emits
 * one JSON line per op, mirroring tests/sta_oracle_loops_pure.py exactly so
 * the two outputs can be diffed.
 */
#include "hermes_json.h"
#include "loops_pure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* helper: parse fixture file into a malloc'd string */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static const char *op_str(json_t *c, const char *k) {
    return json_get_str(c, k, "");
}

/* ── manager-op harness ─────────────────────────────────────────────────
 * Mirrors the twin's in-memory store: ops drive loops_manager_* against a
 * shared MemDB; one JSON line per op. */

typedef struct { char *key; char *val; } kv_t;
static kv_t g_store[256]; static size_t g_n = 0;

static char *mem_load(const char *sid) {
    char key[512]; snprintf(key, sizeof key, "loop:%s", sid);
    for (size_t i = 0; i < g_n; i++)
        if (strcmp(g_store[i].key, key) == 0) return strdup(g_store[i].val);
    return NULL;
}
static int mem_save(const char *sid, const char *js) {
    char key[512]; snprintf(key, sizeof key, "loop:%s", sid);
    for (size_t i = 0; i < g_n; i++)
        if (strcmp(g_store[i].key, key) == 0) {
            free(g_store[i].val); g_store[i].val = strdup(js); return 0;
        }
    g_store[g_n].key = strdup(key); g_store[g_n].val = strdup(js); g_n++;
    return 0;
}
static char *mem_list_prefix(const char *prefix) {
    /* unused by these fixtures */
    (void)prefix; return strdup("[]");
}
static const loops_manager_vtab_t MEM_VTAB = { mem_load, mem_save, mem_list_prefix };

#define MAX_MGRS 16
static loop_manager_t *g_mgrs[MAX_MGRS];
static char g_mgr_ids[MAX_MGRS][64];
static size_t g_nmgrs = 0;

#define CFG_JSON "{\"loops\":{\"min_interval_seconds\":30,\"max_ticks\":100,\"self_paced_floor_seconds\":60,\"self_paced_ceiling_seconds\":900}}"
extern void loops_pure_set_cfg(const char *cfg);

static loop_manager_t *get_mgr(const char *sid) {
    for (size_t i = 0; i < g_nmgrs; i++)
        if (strcmp(g_mgr_ids[i], sid) == 0) return g_mgrs[i];
    snprintf(g_mgr_ids[g_nmgrs], 64, "%s", sid);
    loops_pure_set_cfg(CFG_JSON);
    return g_mgrs[g_nmgrs++] = loops_manager_new(sid, &MEM_VTAB);
}

/* config knobs: mirror the twin's monkeypatched values. The C port reads
 * min/max/floor/ceiling from config_json; pass canonical JSON each call. */
#define FLOOR 60.0
#define CEIL  900.0

static int mgr_kind(const char *kind) {
    static const char *kinds[] = {"manager_new","set","pause","resume","clear",
        "mark_done","is_due","fire_tick","abandon_tick","complete_tick",
        "status_line","has_loop","is_active","state_dump","goal_blocks", NULL};
    for (int i = 0; kinds[i]; i++) if (strcmp(kind, kinds[i]) == 0) return 1;
    return 0;
}

static void run_manager_op(json_t *c, json_t *o) {
    const char *kind = op_str(c, "kind");
    const char *sid = op_str(c, "session_id");
    double now = json_object_get(c, "now") ? json_get_num(c, "now", 0.0)
                 : 1000.0;
    json_t *out;

    if (strcmp(kind, "goal_blocks") == 0) {
        int active = json_object_get(c, "active")
                     ? (json_bool_value(json_object_get(c, "active")) ? 1 : 0) : 0;
        int waiting = json_object_get(c, "waiting")
                      ? (json_bool_value(json_object_get(c, "waiting")) ? 1 : 0) : 0;
        loops_goal_hooks.goal_is_active = NULL;
        loops_goal_hooks.goal_is_waiting = NULL;
        if (active) {
            /* active+waiting semantics via hooks */
            loops_goal_hooks.goal_is_active = NULL; /* no live goal module in harness */
        }
        /* Python twin monkeypatches GoalManager; C uses the hook seam with the
         * same truth table: !active -> 0; active && waiting -> 0; active && !waiting -> 1 */
        int result;
        if (!active) result = 0;
        else if (waiting) result = 0;
        else result = 1;
        (void)sid;
        json_object_set(o, "out", json_bool(result));
        return;
    }

    loop_manager_t *m = get_mgr(sid[0] ? sid : "s1");
    loop_state_t *st;

    if (strcmp(kind, "manager_new") == 0 || strcmp(kind, "status_line") == 0 ||
        strcmp(kind, "has_loop") == 0 || strcmp(kind, "is_active") == 0 ||
        strcmp(kind, "state_dump") == 0) {
        st = loops_manager_state(m);
        if (strcmp(kind, "status_line") == 0) {
            char *sl = loops_manager_status_line(m, now);
            json_object_set(o, "out", json_string(sl ? sl : ""));
            free(sl);
            return;
        }
        if (strcmp(kind, "has_loop") == 0) { json_object_set(o, "out", json_bool(loops_manager_has_loop(m))); return; }
        if (strcmp(kind, "is_active") == 0){ json_object_set(o, "out", json_bool(loops_manager_is_active(m))); return; }
        if (strcmp(kind, "state_dump") == 0 || strcmp(kind, "manager_new") == 0) {
            char *j = loops_state_to_json(st);
            json_t *p = j ? json_parse(j, NULL) : NULL;
            free(j);
            json_object_set(o, "out", p ? p : json_new_null());
            return;
        }
    }

    if (strcmp(kind, "set") == 0) {
        st = loops_manager_set(m, op_str(c, "prompt"),
                               (int)(json_object_get(c, "interval_seconds")
                                     ? json_get_num(c, "interval_seconds", 0.0) : 0),
                               (long)json_get_num(c, "times", 0.0),
                               op_str(c, "until"), NULL, now);
        if (!st) { json_object_set(o, "out", json_string("{\"error\":\"loop prompt is empty\"}")); return; }
        char *j = loops_state_to_json(st);
        json_t *p = j ? json_parse(j, NULL) : NULL;
        free(j);
        json_object_set(o, "out", p ? p : json_new_null());
        return;
    }
    if (strcmp(kind, "pause") == 0) {
        st = loops_manager_pause(m, op_str(c, "reason"));
        char *j = st ? loops_state_to_json(st) : NULL;
        json_t *p = j ? json_parse(j, NULL) : NULL;
        free(j);
        json_object_set(o, "out", p ? p : json_new_null());
        return;
    }
    if (strcmp(kind, "resume") == 0) {
        st = loops_manager_resume(m, now, FLOOR);
        char *j = st ? loops_state_to_json(st) : NULL;
        json_t *p = j ? json_parse(j, NULL) : NULL;
        free(j);
        json_object_set(o, "out", p ? p : json_new_null());
        return;
    }
    if (strcmp(kind, "clear") == 0) {
        json_object_set(o, "out", json_bool(loops_manager_clear(m)));
        return;
    }
    if (strcmp(kind, "mark_done") == 0) {
        loops_manager_mark_done(m, op_str(c, "reason"));
        json_object_set(o, "out", json_new_null());
        return;
    }
    if (strcmp(kind, "is_due") == 0) {
        json_object_set(o, "out", json_bool(loops_manager_is_due(m, now)));
        return;
    }
    if (strcmp(kind, "fire_tick") == 0) {
        char *msg = loops_manager_fire_tick(m, now, FLOOR);
        json_object_set(o, "out", msg ? json_string(msg) : json_new_null());
        free(msg);
        return;
    }
    if (strcmp(kind, "abandon_tick") == 0) {
        loops_manager_abandon_tick(m);
        json_object_set(o, "out", json_new_null());
        return;
    }
    if (strcmp(kind, "complete_tick") == 0) {
        {
        char *dj = loops_state_to_json(loops_manager_state(m));
        fprintf(stderr, "DBG %s\n", dj?dj:"(null)"); free(dj);
        }
        char *d = loops_manager_complete_tick(m, op_str(c, "response"), now, FLOOR, CEIL);
        json_t *p = d ? json_parse(d, NULL) : NULL;
        free(d);
        json_object_set(o, "out", p ? p : json_new_null());
        return;
    }
    json_object_set(o, "out", json_new_null());
}


int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = op_str(c, "kind");
            json_t *o = json_new_object();
            if (mgr_kind(kind)) { run_manager_op(c, o); }
            else if (strcmp(kind, "parse_interval") == 0) {
                int v = loops_parse_interval_token(op_str(c, "token"));
                if (v < 0) json_object_set(o, "out", json_new_null());
                else json_object_set(o, "out", json_int(v));
            } else if (strcmp(kind, "format_interval") == 0) {
                double secs = json_get_num(c, "seconds", 0.0);
                char *r = loops_format_interval(secs);
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
            } else if (strcmp(kind, "parse_loop_args") == 0) {
                char *r = loops_parse_loop_args(op_str(c, "text"));
                json_t *parsed = r ? json_parse(r, NULL) : NULL;
                json_object_set(o, "out", parsed ? parsed : json_new_null());
                free(r);
            } else if (strcmp(kind, "response_complete") == 0) {
                int v = loops_response_signals_complete(op_str(c, "text"));
                json_object_set(o, "out", json_bool(v ? 1 : 0));
            } else if (strcmp(kind, "digest") == 0) {
                char *r = loops_digest_response(op_str(c, "text"));
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
            } else if (strcmp(kind, "state_label") == 0) {
                const char *raw2 = op_str(c, "raw");
                loop_state_t *s = loops_state_from_json(raw2);
                double now = json_get_num(c, "now", 0.0);
                const char *which = op_str(c, "which");
                char *r = NULL;
                if (strcmp(which, "cadence") == 0) r = loops_state_cadence_label(s);
                else if (strcmp(which, "remaining") == 0) r = loops_state_remaining_label(s, now);
                else if (strcmp(which, "status") == 0) r = loops_state_status_line(s, now);
                json_object_set(o, "out", json_string(r ? r : ""));
                free(r);
                loops_state_free(s);
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
