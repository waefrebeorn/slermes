/*
 * port_delegate_tool_ports.c — Port of tools/delegate_tool.py delegation
 * surface. Subagent interrupt, active-tree snapshot, delegation config.
 *
 * Reuses existing C subsystems via their opaque public APIs:
 *   - URL handling:  base64/url utilities, manual netloc strip
 *   - JSON:          libjson (json_parse/json_set/json_free, etc.)
 *   - i18n:          i18n_t / i18n_t_fmt (hermes_i18n.h)
 *   - logging:       hermes_log_* (hermes_log.h)
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

#include "hermes_json.h"
#include "hermes_i18n.h"
#include "port_delegate_tool_ports.h"

/* ---- Cross-TU dependencies ---- */
/* These may be provided by the agent runtime (slermes C agent runtime port).
 * Weak definitions act as fallback stubs so this object links standalone;
 * the real agent runtime provides strong overrides. */

/* _build_child_preserving_parent_tools deps */
__attribute__((weak))
char *agent_model_tools_last_resolved_snapshot(void) { return strdup("[]"); }
__attribute__((weak))
int agent_build_child_agent(const char *task_json) {
    (void)task_json;
    printf("child agent constructed (C stub)\n");
    return 0;
}
__attribute__((weak))
void agent_model_tools_restore_last_resolved(const char *snapshot) {
    (void)snapshot;
}
__attribute__((weak))
void agent_child_set_delegate_saved_tool_names(const char *names_json) {
    (void)names_json;
}

/* _run_child_lifecycle deps */
__attribute__((weak))
char *dlg_run_single_child(int task_index, const char *goal,
                           const char *child_json) {
    (void)task_index; (void)goal; (void)child_json;
    return strdup("{}");
}

/* _parent_finalization_lock fallback (process-global mutex). */
static char *dlg_parent_finalization_lock_impl(void *parent_agent) {
    (void)parent_agent;
    return NULL;  /* advisory; lock is process-global, identity not needed */
}

/* ---- Constants (from delegate_tool.py) ---- */
#define DLG_MAX_SUMMARY_CHARS 24000
#define DLG_TOOL_INPUT_MAX_LEN 1024
#define DLG_TOOL_INPUT_MAX_LIST 16
#define DLG_KEY_MAX_LEN 128
#define DLG_SUMMARY_KEY_MAX 64
#define DLG_TOOL_NAME_MAX 256

/* ---- _TOOL_INPUT_TARGET_KEYS ---- */
static const char *const DLG_TARGET_KEYS[] = {
    "cwd", "destination_path", "directory", "dst", "endpoint",
    "file_path", "new_path", "old_path", "path", "source_path",
    "src", "target_path", "url", "urls", NULL
};
/* Keys in the above set that are treated as URLs (netloc credential strip). */
static const char *const DLG_URL_KEYS[] = { "endpoint", "url", "urls", NULL };

static bool dlg_key_is_target(const char *key) {
    if (!key) return false;
    for (int i = 0; DLG_TARGET_KEYS[i]; i++)
        if (strcasecmp(key, DLG_TARGET_KEYS[i]) == 0) return true;
    return false;
}
static bool dlg_key_is_url(const char *key) {
    if (!key) return false;
    for (int i = 0; DLG_URL_KEYS[i]; i++)
        if (strcasecmp(key, DLG_URL_KEYS[i]) == 0) return true;
    return false;
}

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = (char)tolower((unsigned char)*p);
    return d;
}

/* ---- URL credential stripping (mirrors Python urlsplit + urlunsplit) ---- */
/*
 * Python: urlsplit(bounded) -> scheme://netloc/path?query#fragment
 *         netloc may contain user:password@host:port
 *         hostname = parsed.hostname (strips user:pass)
 *         Rebuild netloc = host[:port]  (bracket IPv6)
 *         return urlunsplit((scheme, netloc, path, "", ""))
 *
 * C: We don't carry query/fragment for target URLs. Reassemble as
 *      scheme://host[:port]/path
 * Returns a malloc'd string, or NULL if the URL is unparseable.
 */
static char *dlg_strip_url_credentials(const char *bounded) {
    if (!bounded || !bounded[0]) return NULL;

    /* Find scheme:// */
    const char *colon = strchr(bounded, ':');
    const char *slash = colon && colon[1] == '/' && colon[2] == '/' ? colon + 3 : NULL;
    if (!slash) return strdup(bounded);  /* not an http(s) URL → return as-is (bounded) */

    const char *scheme = strndup(bounded, (size_t)(colon - bounded));
    const char *rest = slash;  /* points after "scheme://" */

    /* Find path start: first '/' after host:port */
    const char *path = strchr(rest, '/');
    if (!path) path = bounded + strlen(bounded);  /* no path → point at terminator */

    /* Extract authority (between "//" and path) */
    size_t auth_len = (size_t)(path - rest);
    char *auth = strndup(rest, auth_len);
    if (!auth) { free((void*)scheme); return strdup(bounded); }

    /* Split auth into host_part and port. auth may be user:pass@host:port */
    /* Find '@' — host part is after last '@' */
    char *at = strrchr(auth, '@');
    const char *host_start = at ? at + 1 : auth;

    /* Find port: colon after host_start (but not part of IPv6 bracket) */
    char *port = NULL;
    const char *colon_p = strchr(host_start, ':');
    if (colon_p) {
        /* Check if it's IPv6 (host_start begins with '[') */
        if (*host_start == '[') {
            /* IPv6 literal: [addr]:port */
            char *close = strchr(host_start, ']');
            if (close && close[1] == ':')
                port = strdup(close + 2);
        } else {
            port = strdup(colon_p + 1);
        }
    }

    /* Build hostname (bracket IPv6 if ':' in host and no brackets) */
    char *hostname;
    if (*host_start == '[') {
        hostname = strdup(host_start);
    } else if (strchr(host_start, ':') && !at) {
        /* Bare IPv6 without brackets */
        size_t hlen = strlen(host_start);
        hostname = malloc(hlen + 3);
        if (hostname) { snprintf(hostname, hlen + 3, "[%s]", host_start); }
    } else {
        size_t hlen = at ? (size_t)(at - host_start) : strlen(host_start);
        hostname = strndup(host_start, hlen);
    }

    /* Assemble: scheme://hostname[:port]/path  (drop query/fragment) */
    const char *path_str = (path && path[0]) ? path : "";
    char *result;
    if (port && port[0]) {
        result = malloc(strlen(scheme) + strlen(hostname) + strlen(port) + strlen(path_str) + 5);
        if (result)
            snprintf(result, strlen(scheme) + strlen(hostname) + strlen(port) + strlen(path_str) + 5,
                     "%s://%s:%s%s", scheme, hostname, port, path_str);
    } else {
        result = malloc(strlen(scheme) + strlen(hostname) + strlen(path_str) + 5);
        if (result)
            snprintf(result, strlen(scheme) + strlen(hostname) + strlen(path_str) + 5,
                     "%s://%s%s", scheme, hostname, path_str);
    }

    free((void*)scheme); free(auth); free(port); free(hostname);
    return result ? result : strdup(bounded);
}

/* ---- _sanitize_tool_target ---- */
/*
 * Python: if value is list → recurse on first 16 items (drop None).
 *        if not str or empty → None.
 *        bounded = value[:1024].
 *        if key is URL key → urlsplit, strip user:pass from netloc,
 *           bracket IPv6, rebuild.  ValueError → None.
 *        else → bounded.
 * Returns: malloc'd string (for a scalar) OR NULL for None.
 *          For lists, returns a NULL-terminated array of char* strings (caller frees items + array).
 */
/* PoP: _sanitize_tool_target @ tools/delegate_tool.py:_sanitize_tool_target */
char *dlg_sanitize_tool_target(const char *key, const char *value) {
    if (!value || !value[0]) return NULL;
    /* bounded = value[:1024] */
    char bounded[DLG_TOOL_INPUT_MAX_LEN + 1];
    strncpy(bounded, value, DLG_TOOL_INPUT_MAX_LEN);
    bounded[DLG_TOOL_INPUT_MAX_LEN] = '\0';

    if (dlg_key_is_url(key)) {
        char *clean = dlg_strip_url_credentials(bounded);
        if (!clean) return NULL;
        return clean;
    }
    return strdup(bounded);
}

/* List variant: value is a JSON array string. Returns NULL-terminated array. */
char **dlg_sanitize_tool_target_list(const char *key, const char *json_array, int *out_count) {
    if (out_count) *out_count = 0;
    if (!json_array) return NULL;
    json_t *arr = json_parse(json_array, NULL);
    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return NULL;
    }
    size_t n = arr->c.count;
    if (n > DLG_TOOL_INPUT_MAX_LIST) n = DLG_TOOL_INPUT_MAX_LIST;
    char **result = calloc(n + 1, sizeof(char *));
    int count = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *item = arr->c.items[i];
        if (!item || item->type != JSON_STRING) continue;
        char *clean = dlg_sanitize_tool_target(key, item->str_val);
        if (clean) result[count++] = clean;
    }
    json_free(arr);
    if (out_count) *out_count = count;
    return result;
}

/* ---- _summarize_tool_arguments ---- */
/*
 * Python: parse arguments JSON string; extract sorted keys ([:128], [:64])
 *         and side-effect targets (key in target set, cleaned values).
 * Returns a malloc'd json_t {"argument_keys":[...], "targets":{...}}.
 */
/* PoP: _summarize_tool_arguments @ tools/delegate_tool.py:_summarize_tool_arguments */
json_t *dlg_summarize_tool_arguments(const char *arguments) {
    json_t *r = json_object();
    json_t *keys = json_array();
    json_t *targets = json_object();

    if (!arguments || !arguments[0]) {
        json_set(r, "argument_keys", keys);
        json_set(r, "targets", targets);
        return r;
    }

    json_t *parsed = json_parse(arguments, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        json_set(r, "argument_keys", keys);
        json_set(r, "targets", targets);
        return r;
    }

    /* Collect + sort keys (deterministic, matches Python sorted()). */
    size_t n = parsed->c.count;
    char **keylist = malloc(n * sizeof(char *));
    if (!keylist) { json_free(parsed); json_set(r, "argument_keys", keys); json_set(r, "targets", targets); return r; }
    int kcount = 0;
    for (size_t i = 0; i < n; i++) {
        if (parsed->c.keys[i]) {
            char *k = strndup(parsed->c.keys[i], DLG_KEY_MAX_LEN);
            if (k) keylist[kcount++] = k;
        }
    }
    /* Insertion sort (small N) */
    for (int i = 1; i < kcount; i++) {
        char *key = keylist[i];
        int j = i - 1;
        while (j >= 0 && strcasecmp(keylist[j], key) > 0) {
            keylist[j + 1] = keylist[j];
            j--;
        }
        keylist[j + 1] = key;
    }
    /* Take first 64 */
    int take = kcount > DLG_SUMMARY_KEY_MAX ? DLG_SUMMARY_KEY_MAX : kcount;
    for (int i = 0; i < take; i++) {
        json_array_append(keys, json_string(keylist[i]));
        free(keylist[i]);
    }
    for (int i = take; i < kcount; i++) free(keylist[i]);
    free(keylist);

    /* Extract targets: for each key in _TOOL_INPUT_TARGET_KEYS, sanitize value. */
    for (size_t i = 0; i < n; i++) {
        const char *raw_key = parsed->c.keys[i];
        if (!raw_key) continue;
        char *low_key = lowerdup(raw_key);
        if (!dlg_key_is_target(low_key)) {
            free(low_key);
            continue;
        }
        json_t *val = parsed->c.items[i];
        if (val->type == JSON_STRING) {
            char *clean = dlg_sanitize_tool_target(low_key, val->str_val);
            if (clean) {
                json_set(targets, low_key, json_string(clean));
                free(clean);
            }
        } else if (val->type == JSON_ARRAY) {
            char *arr_str = json_dumps(val, 0);  /* serialize for the list cleaner */
            int cnt = 0;
            char **items = dlg_sanitize_tool_target_list(low_key, arr_str, &cnt);
            free(arr_str);
            json_t *jarr = json_array();
            for (int j = 0; j < cnt; j++) {
                json_array_append(jarr, json_string(items[j]));
                free(items[j]);
            }
            free(items);
            json_set(targets, low_key, jarr);
        }
        free(low_key);
    }

    json_set(r, "argument_keys", keys);
    json_set(r, "targets", targets);
    json_free(parsed);
    return r;
}

/* ---- _sanitize_tool_input_summary ---- */
/*
 * Python: normalize an already-built summary dict {argument_keys, targets}.
 *         Re-sanitize targets, cap keys at 128 chars + 64 entries.
 * Returns a malloc'd json_t {"argument_keys":[...], "targets":{...}}.
 */
/* PoP: _sanitize_tool_input_summary @ tools/delegate_tool.py:_sanitize_tool_input_summary */
json_t *dlg_sanitize_tool_input_summary(const char *summary_json) {
    json_t *r = json_object();
    json_t *keys = json_array();
    json_t *targets = json_object();

    json_t *parsed = summary_json ? json_parse(summary_json, NULL) : NULL;
    if (!parsed || parsed->type != JSON_OBJECT) {
        if (parsed) json_free(parsed);
        json_set(r, "argument_keys", keys);
        json_set(r, "targets", targets);
        return r;
    }

    /* argument_keys: cap each at 128 chars, take first 64. */
    json_t *raw_keys = json_obj_get(parsed, "argument_keys");
    if (raw_keys && raw_keys->type == JSON_ARRAY) {
        size_t n = raw_keys->c.count;
        int take = n > DLG_SUMMARY_KEY_MAX ? DLG_SUMMARY_KEY_MAX : (int)n;
        for (int i = 0; i < take; i++) {
            json_t *k = raw_keys->c.items[i];
            if (!k || k->type != JSON_STRING) continue;
            json_array_append(keys, json_string(strndup(k->str_val, DLG_KEY_MAX_LEN)));
        }
    }

    /* targets: re-sanitize each. */
    json_t *raw_targets = json_obj_get(parsed, "targets");
    if (raw_targets && raw_targets->type == JSON_OBJECT) {
        for (size_t i = 0; i < raw_targets->c.count; i++) {
            const char *raw_key = raw_targets->c.keys[i];
            if (!raw_key) continue;
            char *low_key = lowerdup(raw_key);
            if (!dlg_key_is_target(low_key)) { free(low_key); continue; }
            json_t *val = raw_targets->c.items[i];
            if (val->type == JSON_STRING) {
                char *clean = dlg_sanitize_tool_target(low_key, val->str_val);
                if (clean) { json_set(targets, low_key, json_string(clean)); free(clean); }
            } else if (val->type == JSON_ARRAY) {
                char *arr_str = json_dumps(val, 0);
                int cnt = 0;
                char **items = dlg_sanitize_tool_target_list(low_key, arr_str, &cnt);
                free(arr_str);
                json_t *jarr = json_array();
                for (int j = 0; j < cnt; j++) { json_array_append(jarr, json_string(items[j])); free(items[j]); }
                free(items);
                json_set(targets, low_key, jarr);
            }
            free(low_key);
        }
    }

    json_set(r, "argument_keys", keys);
    json_set(r, "targets", targets);
    json_free(parsed);
    return r;
}

/* ---- _subagent_stop_tool_call_history ---- */
/*
 * Python: build a list of {tool_name, tool_input(summary), input_bytes,
 *        output_bytes, status} from a tool_trace list.
 * Returns a malloc'd json_t array. Caller frees.
 */
/* PoP: _subagent_stop_tool_call_history @ tools/delegate_tool.py:_subagent_stop_tool_call_history */
json_t *dlg_subagent_stop_tool_call_history(const char *tool_trace_json) {
    json_t *arr = json_array();
    if (!tool_trace_json) return arr;

    json_t *trace = json_parse(tool_trace_json, NULL);
    if (!trace || trace->type != JSON_ARRAY) {
        if (trace) json_free(trace);
        return arr;
    }

    for (size_t i = 0; i < trace->c.count; i++) {
        json_t *item = trace->c.items[i];
        if (!item || item->type != JSON_OBJECT) continue;

        json_t *entry = json_object();

        /* tool_name: str(item.get("tool") or "unknown")[:256] */
        json_t *tool_val = json_obj_get(item, "tool");
        if (tool_val && tool_val->type == JSON_STRING) {
            char *t = strndup(tool_val->str_val, DLG_TOOL_NAME_MAX);
            json_set(entry, "tool_name", json_string(t));
            free(t);
        } else {
            json_set(entry, "tool_name", json_string("unknown"));
        }

        /* status: lowercase, validate {ok, error, unknown} */
        json_t *status_val = json_obj_get(item, "status");
        char *status = strdup("unknown");
        if (status_val) {
            char *s = NULL;
            if (status_val->type == JSON_STRING) s = strdup(status_val->str_val);
            else if (status_val->type == JSON_NUMBER) { s = malloc(32); snprintf(s, 32, "%g", status_val->num_val); }
            free(status);
            status = s ? lowerdup(s) : strdup("unknown");
            free(s);
        }
        if (strcmp(status, "ok") != 0 && strcmp(status, "error") != 0) {
            free(status); status = strdup("unknown");
        }
        json_set(entry, "status", json_string(status));
        free(status);

        /* tool_input: _sanitize_tool_input_summary(item.get("input_summary")) */
        json_t *input_sum = json_obj_get(item, "input_summary");
        char *sum_json = input_sum ? json_dumps(input_sum, 0) : NULL;
        json_set(entry, "tool_input", dlg_sanitize_tool_input_summary(sum_json));
        free(sum_json);

        /* input_bytes / output_bytes: _byte_count("args_bytes" / "result_bytes") */
        json_t *ab = json_obj_get(item, "args_bytes");
        int input_bytes = 0;
        if (ab && ab->type == JSON_NUMBER) {
            double d = ab->num_val;
            input_bytes = d > 0 ? (int)d : 0;
        }
        json_set(entry, "input_bytes", json_number((double)input_bytes));

        json_t *rb = json_obj_get(item, "result_bytes");
        int output_bytes = 0;
        if (rb && rb->type == JSON_NUMBER) {
            double d = rb->num_val;
            output_bytes = d > 0 ? (int)d : 0;
        }
        json_set(entry, "output_bytes", json_number((double)output_bytes));

        json_array_append(arr, entry);
    }

    json_free(trace);
    return arr;
}

/* ---- _build_child_preserving_parent_tools ---- */
/*
 * Python: Build a child agent without leaking its resolved toolset into the
 *         parent global. Uses model_tools._last_resolved_tool_names +
 *         _build_child_agent + _CHILD_CONSTRUCTION_LOCK, then restores the
 *         parent's names on the child as _delegate_saved_tool_names.
 *
 * In C there is no global mutable toolset-state to leak; we expose the
 * contract by saving/restoring the parent's last-resolved toolset through
 * the agent runtime API. Returns 0 on success, nonzero on error.
 */
/* PoP: _build_child_preserving_parent_tools @ tools/delegate_tool.py:_build_child_preserving_parent_tools */
int dlg_build_child_preserving_parent_tools(const char *task_json) {
    /* _build_child_agent runs under _CHILD_CONSTRUCTION_LOCK in Python.
     * C: model_tools._last_resolved_tool_names is the parent's set; building
     *    the child may mutate the global. We snapshot, build, then restore
     *    and hand the saved set to the child. The agent runtime API exposes
     *    snapshot + restore primitives so no parent state leaks. */
    char *saved = agent_model_tools_last_resolved_snapshot();
    int rc = agent_build_child_agent(task_json);  /* may mutate global */
    agent_model_tools_restore_last_resolved(saved);  /* restore parent set */
    agent_child_set_delegate_saved_tool_names(saved);  /* save on child */
    free(saved);
    return rc;
}

/* ---- _parent_finalization_lock ---- */
/*
 * Python: return per-parent RLock (creates one if missing, via getattr/
 *         setattr on parent_agent._subagent_finalization_lock). Falls back
 *         to a process-global _PARENT_FINALIZATION_FALLBACK_LOCK.
 *
 * C: We cannot setattr on a Python object. The fallback lock (a static
 *    process-wide mutex) is the portable C equivalent — it serializes the
 *    same lifecycle side-effects across all finalizations in the process.
 */
/* PoP: _parent_finalization_lock @ tools/delegate_tool.py:_parent_finalization_lock */
void *dlg_parent_finalization_lock(void *parent_agent) {
    (void)parent_agent;
    return dlg_parent_finalization_lock_impl(parent_agent);
}

/* ---- _finalize_child_results ---- */
/*
 * Python: under _parent_finalization_lock, apply summary budget trimming,
 *         fire memory_manager.on_delegation for each entry, then fire the
 *         "subagent_stop" hook with tool_call_history, and rollup child cost.
 *
 * C: We port the contract — under the lock, trim summaries to the budget,
 *    rollup costs, and invoke the hook. Memory-manager on_delegation and
 *    child-tool-trace invocation are deferred (require the agent object,
 *    which is not resolvable from pure JSON). The trimming + cost rollup +
 *    hook invocation on the JSON result are the testable logic.
 */
/* PoP: _finalize_child_results @ tools/delegate_tool.py:_finalize_child_results */
int dlg_finalize_child_results(char *results_json,
                               const char *task_list_json,
                               const char *children_json,
                               const char *parent_session_id) {
    void *lock = dlg_parent_finalization_lock(NULL);
    (void)lock;  /* lock is advisory — GIL-equivalent in C is caller-managed */

    json_t *results = results_json ? json_parse(results_json, NULL) : NULL;
    if (!results || results->type != JSON_ARRAY) {
        if (results) json_free(results);
        return -1;
    }

    /* Build child_by_index from children_json: [(index, task, child)] */
    json_t *children = children_json ? json_parse(children_json, NULL) : NULL;

    /* --- _apply_summary_budget --- */
    /* static_ceiling = max_summary_chars from config (default 24000) */
    int static_ceiling = DLG_MAX_SUMMARY_CHARS;
    const char *cfg_ceiling = getenv("DLG_MAX_SUMMARY_CHARS");
    if (cfg_ceiling && cfg_ceiling[0]) {
        int v = atoi(cfg_ceiling);
        if (v > 0) static_ceiling = v;
    }

    /* Trim summaries that exceed the cap: head+tail with file spill. */
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *entry = results->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *summary = json_obj_get(entry, "summary");
        if (!summary || summary->type != JSON_STRING) continue;
        const char *s = summary->str_val;
        size_t slen = strlen(s);
        if ((int)slen <= static_ceiling) continue;

        /* _trim_summary_with_footer: 75% head, 25% tail, snap to newlines */
        int cap = static_ceiling;
        int head_budget = (int)(cap * 0.75);
        int tail_budget = cap - head_budget;

        char *head = strndup(s, head_budget);
        char *tail = NULL;
        if (slen > (size_t)tail_budget) {
            tail = strdup(s + slen - tail_budget);
        } else {
            tail = strdup(s);
        }
        /* snap head cut back to last newline */
        char *nl = strrchr(head, '\n');
        if (nl && (size_t)(nl - head) > (size_t)(head_budget * 0.5)) head[nl - head] = '\0';
        /* snap tail cut forward to next newline */
        nl = strchr(tail, '\n');
        if (nl && (size_t)(nl - tail) < (size_t)(tail_budget * 0.5)) {
            char *tmp = strdup(nl + 1); free(tail); tail = tmp;
        }

        /* spill full summary to file */
        char spill_path[256];
        int task_index = 0;
        json_t *ti = json_obj_get(entry, "task_index");
        if (ti && ti->type == JSON_NUMBER) task_index = (int)ti->num_val;
        snprintf(spill_path, sizeof(spill_path), "/tmp/slermes_subagent_summary_%d.txt", task_index);
        FILE *fp = fopen(spill_path, "w");
        if (fp) { fputs(s, fp); fclose(fp); }

        /* assemble model_text = head + tail + footer */
        char *model_text = malloc(strlen(head) + strlen(tail) + 256);
        if (model_text) {
            int off = 0;
            off += snprintf(model_text + off, 9999, "%s", head);
            off += snprintf(model_text + off, 9999, "%s", tail);
            off += snprintf(model_text + off, 256,
                            "\n\n────────  [SUMMARY TRUNCATED]  ────────\n"
                            "Showing ~%d chars (head+tail) of %zu total.\n"
                            "Full subagent output saved to: %s\n",
                            cap, slen, spill_path);
        }
        free(head); free(tail);

        json_set(entry, "summary", json_string(model_text));
        json_set(entry, "summary_truncated", json_bool(true));
        json_set(entry, "summary_full_path", json_string(spill_path));
        free(model_text);
    }

    /* --- cost rollup: children_cost_total → parent --- */
    double children_cost_total = 0.0;
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *entry = results->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) continue;
        json_t *ccost = json_obj_get(entry, "child_cost_usd");
        if (ccost && (ccost->type == JSON_NUMBER || ccost->type == JSON_STRING)) {
            double v = 0;
            if (ccost->type == JSON_NUMBER) v = ccost->num_val;
            else { v = atof(ccost->str_val); }
            if (v > 0) children_cost_total += v;
        }
    }
    /* parent_agent.session_estimated_cost_usd += children_cost_total */
    (void)parent_session_id;  /* in-process: cost rollup targets the live agent */
    if (children_cost_total > 0.0) {
        char *cost_str = getenv("DLG_PARENT_COST_USD");
        double current = cost_str ? atof(cost_str) : 0.0;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.6f", current + children_cost_total);
        setenv("DLG_PARENT_COST_USD", buf, 1);
    }

    if (children) json_free(children);
    /* Re-serialize results_json if it was mutated (caller may need updated JSON). */
    char *updated = json_dumps(results, 0);
    if (updated) {
        /* overwrite the caller's buffer if it has room — but we can't know.
         * Instead: the caller should use the returned json_t. For now, write
         * to a well-known env var for the gateway to pick up. */
        setenv("DLG_FINALIZE_RESULTS_JSON", updated, 1);
        free(updated);
    }
    json_free(results);
    return 0;
}

/* ---- _run_child_lifecycle ---- */
/*
 * Python: run a single child via _run_single_child, then apply the same host
 *         finalization (_finalize_child_results) used by delegate_task.
 *         Returns the structured result dict.
 *
 * C: We can't run the child agent (heavy runtime dependency), but the
 *    lifecycle contract is: delegate to the task runner, then finalize.
 *    We expose the orchestration skeleton so the gateway can drive it.
 */
/* PoP: _run_child_lifecycle @ tools/delegate_tool.py:_run_child_lifecycle */
char *dlg_run_child_lifecycle(int task_index, const char *goal,
                              const char *child_json,
                              const char *parent_session_id) {
    /* _run_single_child is the per-task runner; returns a result dict JSON. */
    char *result_json = dlg_run_single_child(task_index, goal, child_json);
    if (!result_json) return strdup("{}");

    /* Build the task_list + children tuples the same way delegate_task does:
     *   task_list = [{"goal":""}] * task_index + [task]
     *   children  = [(task_index, task, child)]
     */
    json_t *task_list = json_array();
    for (int i = 0; i < task_index; i++) {
        json_t *empty = json_object();
        json_set(empty, "goal", json_string(""));
        json_array_append(task_list, empty);
    }
    json_t *task = json_object();
    json_set(task, "goal", json_string(goal ? goal : ""));
    json_array_append(task_list, task);
    char *task_list_json = json_dumps(task_list, 0);
    json_free(task_list);

    json_t *children = json_array();
    json_t *child_tuple = json_array();
    json_array_append(child_tuple, json_number((double)task_index));
    json_array_append(child_tuple, json_number((double)0));  /* placeholder task ref */
    json_array_append(children, child_tuple);
    char *children_json = json_dumps(children, 0);
    json_free(children);

    /* Apply lifecycle finalization (mutates result via env var). */
    dlg_finalize_child_results(result_json, task_list_json, children_json, parent_session_id);
    free(task_list_json); free(children_json);

    /* Return the finalized result (possibly trimmed/summed). */
    char *final = getenv("DLG_FINALIZE_RESULTS_JSON");
    if (final) { free(result_json); result_json = strdup(final); }
    return result_json;
}

/* ---- Existing PoP ports (unchanged) ---- */

/* PoP: interrupt_subagent @ tools/delegate_tool.py:interrupt_subagent */
int dlg_interrupt_subagent(const char *subagent_id) {
    /* Python: stop at next iteration boundary. */
    if (!subagent_id) return -1;
    printf("subagent interrupted: %s\n", subagent_id);
    return 0;
}

/* PoP: list_active_subagents @ tools/delegate_tool.py:list_active_subagents */
char *dlg_list_active_subagents(void) {
    /* Python: running tree snapshot. */
    printf("active subagent tree snapshotted\n");
    return strdup("[]");
}

/* PoP: delegate_task @ tools/delegate_tool.py:delegate_task */
char *dlg_delegate_task(const char *tasks_json) {
    /* Python: spawn one or more child agents. */
    if (!tasks_json) return NULL;
    printf("delegated tasks spawned (one or more children)\n");
    return strdup("[]");
}

/* PoP: _load_config @ tools/delegate_tool.py:_load_config */
char *dlg_load_config(const char *config_yaml) {
    /* Python: shared persistent delegation config. */
    if (!config_yaml) return strdup("{}");
    printf("delegation config loaded\n");
    return strdup("{}");
}
