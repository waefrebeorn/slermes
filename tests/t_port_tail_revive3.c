/* t_port_tail_revive3.c — harness for the tail group-3 revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace}:
 *   ret   — the port's return (bool / object; {"__raise__": type,
 *           "message": msg} when the live function would raise)
 *   trace — ordered seam calls {ev, args, ret}
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused on every call), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>} / {"__script__": true,
 * "raise": {"type": T, "message": M}}. Wrap list RETURNS as
 * {"__script__": true, "ret": [...]} — a bare array is a per-call queue.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The fixed (often void) stub signatures take their Python arguments from
 * module-static call-context slots inside each port file; this harness
 * installs them per case. BaseException kinds (SystemExit,
 * KeyboardInterrupt, GeneratorExit, CancelledError) pass through every
 * `except Exception` site via the per-file pending-raise channels.
 *
 * curator.c is #included whole; its unrelated externs (cron_list_jobs,
 * skill_usage_*) are no-op stubs below so the harness links with libjson
 * alone. They are never reached by the tested function.
 *
 * Gate (explicit objects — libjson only):
 *   bash tests/run_one_oracle.sh tail_revive3 oracle_tail_revive3.py "" "lib/libjson/json.o"
 */
#include "tools/browser_lightpanda_gaps.c"
#include "hermes_cli/web_server_gaps.c"
#include "tools/file_tools_gaps.c"
#include "agent/redact_gaps.c"
#include "hermes_cli/backup_gaps.c"
#include "hermes_cli/auth_gaps.c"
#include "tools/skills_hub_gaps.c"
#include "hermes_cli/proxy/sse_done_gaps.c"
#include "hermes_cli/local_runtime/growth_gaps.c"
#include "gateway/platforms/api_server_runs_gaps.c"
#include "agent/memory_manager_gaps.c"
#include "tools/code_execution_tool_gaps.c"
#include "gateway/status_gaps.c"
#include "agent/curator.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── curator.c extern stubs (never called by the tested merge) ── */
char *cron_list_jobs(void) { return NULL; }
void skill_usage_load(const char *hermes_home, skill_usage_map_t *out_map) {
    (void)hermes_home; (void)out_map;
}
int skill_usage_set_state(const char *hermes_home, const char *skill_name,
                          const char *state) {
    (void)hermes_home; (void)skill_name; (void)state;
    return 0;
}
int skill_usage_archive(const char *hermes_home, const char *skill_name,
                        char *out_msg) {
    (void)hermes_home; (void)skill_name; (void)out_msg;
    return 0;
}
const char *skill_usage_latest_activity(const skill_usage_record_t *record,
                                        char *out_buf) {
    (void)record; (void)out_buf;
    return NULL;
}
int skill_usage_activity_count(const skill_usage_record_t *record) {
    (void)record;
    return 0;
}

/* ── scripted seam machinery (shared by all port io channels) ── */
static json_t *g_io_map;   /* borrowed in.io */
static json_t *g_cursors;  /* owned per-seam queue cursors */
static json_t *g_trace;    /* owned ordered event array */

static json_t *tr3_raise_spec(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *tr3_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT &&
        json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT)
            return tr3_raise_spec(json_get_str(raise, "type", "RuntimeError"),
                                  json_get_str(raise, "message", ""));
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *tr3_script_next(const char *name) {
    json_t *entry = g_io_map ? json_obj_get(g_io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return tr3_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(g_cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return tr3_raise_spec("RuntimeError", msg);
        }
        json_set(g_cursors, name, json_number(cur + 1));
        return tr3_entry_result(json_get(entry, idx));
    }
    return tr3_entry_result(entry);
}

/* One dispatcher serves every port's io channel (identical call shape).
 * Records {ev, args, ret} in order; ret is the scripted value or the
 * raise spec (fail-closed RuntimeError included). Returns a NEW node. */
static json_t *tr3_dispatch(void *io, const char *name, json_t *args) {
    (void)io;
    json_t *ret = tr3_script_next(name);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", json_copy(ret));
    json_append(g_trace, ev);
    return ret;
}

static lp_io_t g_lp_chan = { tr3_dispatch };
static ft_io_t g_ft_chan = { tr3_dispatch };
static gr_io_t g_gr_chan = { tr3_dispatch };
static ar_io_t g_ar_chan = { tr3_dispatch };
static mm_io_t g_mm_chan = { tr3_dispatch };
static ce_io_t g_ce_chan = { tr3_dispatch };
static st_io_t g_st_chan = { tr3_dispatch };

/* Take the first parked BaseException/propagated raise (one per case). */
static json_t *tr3_take_pending(void) {
    json_t **slots[] = {
        &g_lp_pending_raise, &g_ft_pending_raise, &g_gr_pending_raise,
        &g_ar_pending_raise, &g_mm_pending_raise, &g_ce_pending_raise,
        &g_cur_pending_raise, &g_st_pending_raise
    };
    for (size_t i = 0; i < sizeof slots / sizeof slots[0]; i++) {
        if (*slots[i]) {
            json_t *r = *slots[i];
            *slots[i] = NULL;
            return r;
        }
    }
    return NULL;
}

static void tr3_reset_ctx(void) {
    g_lp_io = NULL; g_lp_pid = 0; g_lp_port = NULL; g_lp_start_time = NULL;
    g_dlae_ssh_session_token = NULL; g_dlae_ssh_owner_nonce = NULL;
    g_ft_io = NULL;
    g_rd_pattern = NULL;
    g_oauth_entry = NULL;
    g_sh_query = NULL;
    g_sse_headers = NULL;
    g_gr_base_url = NULL; g_gr_io = NULL;
    g_ar_path = NULL; g_ar_io = NULL;
    g_mm_agent = NULL; g_mm_io = NULL;
    g_ce_python_path = NULL; g_ce_sys_executable = NULL;
    g_ce_sys_prefix = NULL; g_ce_io = NULL;
    g_st_path = NULL; g_st_label = NULL; g_st_io = NULL;
}

static char *tr3_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = tr3_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        int in_owned = 0;
        if (!in) { in = json_object(); in_owned = 1; }

        tr3_reset_ctx();
        json_free(tr3_take_pending());
        g_io_map = json_obj_get(in, "io");
        g_cursors = json_object();
        g_trace = json_array();

        json_t *ret = NULL;
        int known = 1;
        if (strcmp(func, "is_lightpanda_process") == 0) {
            g_lp_pid = (int)json_get_num(in, "pid", 0);
            g_lp_port = json_obj_get(in, "port");
            g_lp_start_time = json_obj_get(in, "start_time");
            g_lp_io = &g_lp_chan;
            int r = is_lightpanda_process();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "desktop_loopback_auth_exempt") == 0) {
            unsetenv("HERMES_DESKTOP");
            unsetenv("HERMES_DASHBOARD_SESSION_TOKEN");
            json_t *env = json_obj_get(in, "env");
            if (env && env->type == JSON_OBJECT) {
                for (size_t i = 0; i < json_object_size(env); i++) {
                    const char *k = json_object_get_key_at(env, i);
                    json_t *v = json_object_get_at(env, i);
                    if (k && v && v->type == JSON_STRING)
                        setenv(k, v->str_val, 1);
                }
            }
            g_dlae_ssh_session_token = json_get_str(in, "ssh_session_token", NULL);
            g_dlae_ssh_owner_nonce = json_get_str(in, "ssh_owner_nonce", NULL);
            int r = desktop_loopback_auth_exempt(json_get_str(in, "host", NULL));
            ret = json_bool(r != 0);
        } else if (strcmp(func, "is_openai_family_main") == 0) {
            g_ft_io = &g_ft_chan;
            int r = is_openai_family_main();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "looks_like_opaque_credential") == 0) {
            int r = looks_like_opaque_credential(json_get_str(in, "value", ""));
            ret = json_bool(r != 0);
        } else if (strcmp(func, "has_top_level_alternation") == 0) {
            g_rd_pattern = json_get_str(in, "pattern", "");
            int r = has_top_level_alternation();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "has_nested_unbounded_repeat") == 0) {
            g_rd_pattern = json_get_str(in, "pattern", "");
            int r = has_nested_unbounded_repeat();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "in_excluded_root_dir") == 0) {
            int r = in_excluded_root_dir(json_get_str(in, "path", ""));
            ret = json_bool(r != 0);
        } else if (strcmp(func, "is_oauth_pool_payload") == 0) {
            g_oauth_entry = json_obj_get(in, "entry");
            int r = is_oauth_pool_payload();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "query_is_concrete") == 0) {
            g_sh_query = json_get_str(in, "query", "");
            int r = query_is_concrete();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "content_type_is_sse") == 0) {
            g_sse_headers = json_obj_get(in, "headers");
            int r = content_type_is_sse();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "is_managed_endpoint") == 0) {
            g_gr_base_url = json_get_str(in, "base_url", NULL);
            g_gr_io = &g_gr_chan;
            int r = is_managed_endpoint();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "uses_room_run_auth") == 0) {
            g_ar_path = json_get_str(in, "path", "");
            g_ar_io = &g_ar_chan;
            int r = uses_room_run_auth();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "memory_provider_tools_exposed") == 0) {
            g_mm_agent = json_obj_get(in, "agent");
            g_mm_io = &g_mm_chan;
            int r = memory_provider_tools_exposed();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "uses_hermes_python_environment") == 0) {
            g_ce_python_path = json_get_str(in, "python_path", "");
            g_ce_sys_executable = json_get_str(in, "sys_executable", "");
            g_ce_sys_prefix = json_get_str(in, "sys_prefix", "");
            g_ce_io = &g_ce_chan;
            int r = uses_hermes_python_environment();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "strict_path_exists") == 0) {
            g_st_path = json_get_str(in, "path", "");
            g_st_label = json_get_str(in, "label", "");
            g_st_io = &g_st_chan;
            int r = strict_path_exists();
            ret = json_bool(r != 0);
        } else if (strcmp(func, "metadata") == 0) {
            json_t *r = curator_merge_request_overrides(
                json_obj_get(in, "runtime_overrides"),
                json_obj_get(in, "slot_extra_body"));
            ret = r ? r : json_null();
        } else {
            known = 0;
            ret = json_object();
            json_set_hdr(ret, "error", "unknown func");
        }

        /* A parked raise replaces the mapped return (live propagation). */
        json_t *pending = tr3_take_pending();
        if (pending) {
            json_free(ret);
            ret = pending;
        }
        (void)known;

        json_t *out = json_object();
        json_set(out, "ret", ret ? ret : json_null());
        json_set(out, "trace", json_copy(g_trace));

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(g_trace);
        json_free(g_cursors);
        g_io_map = NULL;
        tr3_reset_ctx();
        if (in_owned) json_free(in);
        json_free(rec);
    }
    return 0;
}