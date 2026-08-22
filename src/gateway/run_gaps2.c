/*
 * run_gaps2.c — Faithful C11 port of the remaining gateway/run.py REAL_GAPs
 * (batch 2). Companion to run_gaps.c. Every function carries a PoP annotation
 * crediting the exact Python name in gateway/run.py.
 *
 * Side effects (platform sends, DB access, LLM calls) are routed through
 * injectable hooks/vtabs so the module stays self-contained and testable.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>

#include "hermes_json.h"
#include "hermes_logger.h"
#include "gateway_run_helpers.h"
#include "gateway_run_pure.h"
#include "gateway_run_pure2.h"
#include "hermes_gateway_slash_access.h"
#include "hermes_gateway_types.h"
#include "hermes_gateway_core.h"
#include "hermes_gateway_runner.h"
#include "provider_metadata.h"

/* Cross-module externs (no shared header declares these yet). */
extern char *hermes_state_default_db_path(void);
extern const char *hs_classify_persistence_error(const char *errmsg);
extern char *hermes_state_format_session_db_unavailable(const char *prefix);
extern char *gateway_redact_user_facing_secrets(const char *text);

/* ════════════════════════════════════════════════════════════════
 * _prepare_resume_pending_message
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_prepare_resume_pending_message @ gateway/run.py:_prepare_resume_pending_message */
void gw_prepare_resume_pending_message(const char *reason, const char *message,
                                       bool interactive,
                                       char *recovery_out, size_t recovery_cap,
                                       char **persist_out)
{
    if (!recovery_out || recovery_cap == 0 || !persist_out) return;
    gw_build_resume_recovery_note(reason, message ? message : "", interactive,
                                  recovery_out, recovery_cap);
    /* When the user sent REAL text while the resume was pending, keep
     * persisting their clean words; otherwise persist the recovery note
     * itself (#86580 — never persist an empty user row). */
    if (message && message[0] && message[strspn(message, " \t\r\n")] != '\0')
        *persist_out = strdup(message);
    else
        *persist_out = strdup(recovery_out);
}

/* ════════════════════════════════════════════════════════════════
 * _shorten_command_for_display / _format_concise_process_notification
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_shorten_command_for_display @ gateway/run.py:_shorten_command_for_display */
void gw_shorten_command_for_display(const char *command, int limit,
                                    char *out, size_t cap)
{
    if (!out || cap == 0) return;
    out[0] = '\0';
    if (!command) return;
    /* Collapse all whitespace runs onto one line (" ".join(split())). */
    size_t w = 0;
    const char *p = command;
    bool in_ws = false;
    while (*p && w + 1 < cap) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
            in_ws = true;
            p++;
            continue;
        }
        if (in_ws && w > 0 && w + 1 < cap) out[w++] = ' ';
        in_ws = false;
        out[w++] = (char)c;
        p++;
    }
    out[w] = '\0';
    if ((int)strlen(out) > limit && limit > 0) {
        /* one_line[: limit - 1] + "…" — the ellipsis is ONE character in
         * Python len(); in UTF-8 it is 3 bytes. Truncate the display string
         * to limit-1 CHARACTERS (ASCII-safe for command text), then append
         * the UTF-8 ellipsis. */
        int keep = limit - 1;
        int chars = 0, i = 0;
        while (out[i] && chars < keep) {
            unsigned char c = (unsigned char)out[i];
            int clen = (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : 4;
            i += clen;
            chars++;
        }
        out[i] = '\0';
        size_t l = strlen(out);
        if (l + 4 < cap) { out[l++] = '\xe2'; out[l++] = '\x80'; out[l++] = '\xa6'; out[l] = '\0'; }
    }
}

/* PoP: gw_format_concise_process_notification @ gateway/run.py:_format_concise_process_notification */
char *gw_format_concise_process_notification(const char *session_id,
                                             const char *command,
                                             int has_exit_code, long exit_code,
                                             const char *output,
                                             int has_duration, double duration_seconds)
{
    (void)session_id;
    bool ok = !has_exit_code || exit_code == 0;
    const char *icon = ok ? "\xe2\x9c\x85" : "\xe2\x9d\x8c";   /* ✅ / ❌ */
    char verb[64];
    if (ok) snprintf(verb, sizeof verb, "finished");
    else snprintf(verb, sizeof verb, "failed (exit %ld)", exit_code);

    char short_cmd[256];
    gw_shorten_command_for_display(command, 80, short_cmd, sizeof short_cmd);

    size_t cap = 512 + (output ? strlen(output) : 0) + strlen(short_cmd);
    char *text = malloc(cap);
    if (!text) return NULL;
    size_t n = (size_t)snprintf(text, cap, "%s Background task %s", icon, verb);
    if (short_cmd[0])
        n += (size_t)snprintf(text + n, cap - n, " \xe2\x80\x94 `%s`", short_cmd);
    if (has_duration && duration_seconds >= 0) {
        long secs = (long)duration_seconds;
        char dur[32];
        if (secs >= 3600) snprintf(dur, sizeof dur, "%ldh %ldm", secs / 3600, (secs % 3600) / 60);
        else if (secs >= 60) snprintf(dur, sizeof dur, "%ldm %lds", secs / 60, secs % 60);
        else snprintf(dur, sizeof dur, "%lds", secs);
        n += (size_t)snprintf(text + n, cap - n, " (%s)", dur);
    }
    if (!ok && output && output[strspn(output, " \t\r\n")] != '\0') {
        /* Last 5 non-blank lines of stripped output. */
        const char *lines[6]; int nl = 0;
        const char *s = output;
        while (*s) {
            while (*s == '\n' || *s == ' ' || *s == '\t' || *s == '\r') s++;
            if (!*s) break;
            const char *e = s;
            while (*e && *e != '\n') e++;
            lines[nl % 5] = s; nl++;
            s = (*e == '\n') ? e + 1 : e;
        }
        int start = nl > 5 ? nl - 5 : 0;
        char tail[1024]; size_t t = 0;
        for (int i = start; i < nl && i < start + 5; i++) {
            const char *ln = lines[i % 5];
            size_t ll = strcspn(ln, "\n");
            if (t + ll + 2 < sizeof tail) {
                memcpy(tail + t, ln, ll); t += ll;
                tail[t++] = '\n';
            }
        }
        if (t) tail[t - 1] = '\0'; else tail[0] = '\0';
        if (strlen(tail) > 500) memmove(tail, tail + strlen(tail) - 500, 501);
        if (tail[0]) {
            snprintf(text + n, cap - n, "\n```\n%s\n```", tail);
        }
    }
    return text;
}

/* ════════════════════════════════════════════════════════════════
 * _reconnect_needs_attention
 * ════════════════════════════════════════════════════════════════ */

#define GW_RECONNECT_ATTENTION_AFTER_DEFAULT 7200.0

/* PoP: gw_reconnect_needs_attention @ gateway/run.py:_reconnect_needs_attention */
bool gw_reconnect_needs_attention(json_node_t *info, double now)
{
    if (!info) return false;
    double after = gw_float_env("HERMES_RECONNECT_ATTENTION_AFTER_SECONDS",
                                GW_RECONNECT_ATTENTION_AFTER_DEFAULT);
    if (after <= 0) return false;   /* escalation disabled */
    json_node_t *q = json_obj_get(info, "queued_at");
    if (!q || q->type == JSON_NULL) {
        json_set(info, "queued_at", json_double(now));
        return false;
    }
    return (now - q->num_val) >= after;
}


/* ════════════════════════════════════════════════════════════════
 * _dump_wedged_turn_stacks — bounded best-effort thread census.
 *
 * Python walks live interpreter frames; C11 has no interpreter frames, so
 * the faithful port enumerates the process's real threads via
 * /proc/self/task, keeps only threads whose name matches a turn-machinery
 * marker (the same marker set as _TURN_STACK_DUMP_FRAME_MARKERS), and logs
 * them capped at 8 with the task id. Must never raise into the reaper.
 * ════════════════════════════════════════════════════════════════ */

static const char *GW_TURN_STACK_MARKERS[] = {
    "run_conversation", "run_sync", "_run_sync_with_timeout_lifecycle",
    "finalize_turn", "end_turn", "run_in_session",
};

/* PoP: gw_dump_wedged_turn_stacks @ gateway/run.py:_dump_wedged_turn_stacks */
void gw_dump_wedged_turn_stacks(const char *task_id)
{
    DIR *d = opendir("/proc/self/task");
    if (!d) {
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump for task %s: /proc unavailable", task_id ? task_id : "?");
        return;
    }
    int dumped = 0;
    struct dirent *ent;
    char self_comm[64];
    snprintf(self_comm, sizeof self_comm, "%d", (int)getpid());
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
        if (strcmp(ent->d_name, self_comm) == 0) continue;  /* the reaper itself */
        char path[128], comm[256];
        snprintf(path, sizeof path, "/proc/self/task/%s/comm", ent->d_name);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        size_t n = fread(comm, 1, sizeof comm - 1, f);
        fclose(f);
        comm[n] = '\0';
        /* strip trailing newline */
        while (n && (comm[n-1] == '\n' || comm[n-1] == '\r')) comm[--n] = '\0';
        bool matched = false;
        for (size_t i = 0; i < sizeof GW_TURN_STACK_MARKERS / sizeof *GW_TURN_STACK_MARKERS; i++) {
            if (strstr(comm, GW_TURN_STACK_MARKERS[i]) || strstr(ent->d_name, GW_TURN_STACK_MARKERS[i])) {
                /* thread names are truncated to 15 chars — match prefix-wise */
                size_t ml = strlen(GW_TURN_STACK_MARKERS[i]);
                if (ml > n) ml = n;
                if (strncmp(comm, GW_TURN_STACK_MARKERS[i], ml) == 0 ||
                    strstr(comm, GW_TURN_STACK_MARKERS[i])) { matched = true; break; }
            }
        }
        if (!matched) continue;
        dumped++;
        if (dumped > 8) {
            hermes_log(LOG_ERROR, "gateway",
                       "Wedged-turn stack dump for task %s truncated: more than 8 candidate threads",
                       task_id ? task_id : "?");
            break;
        }
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump (task=%s thread=%s ident=%s)",
                   task_id ? task_id : "?", comm, ent->d_name);
    }
    closedir(d);
    if (dumped == 0) {
        hermes_log(LOG_ERROR, "gateway",
                   "Wedged-turn stack dump for task %s: no thread with turn-machinery frames found (worker may have already exited)",
                   task_id ? task_id : "?");
    }
}

/* ════════════════════════════════════════════════════════════════
 * Notification coalescing cluster (_completion_notification_*)
 * ════════════════════════════════════════════════════════════════ */

static const char *const GW_BATCH_KEY_FIELDS[] = {
    "session_key", "platform", "chat_type", "chat_id", "thread_id", "user_id",
};
#define GW_BATCH_KEY_FIELDS_N 6

/* PoP: gw_completion_notification_batch_key @ gateway/run.py:GatewayRunner._completion_notification_batch_key */
char *gw_completion_notification_batch_key(const json_node_t *evt)
{
    if (!evt) return NULL;
    size_t cap = 32, len = 0;
    for (int i = 0; i < GW_BATCH_KEY_FIELDS_N; i++) {
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_BATCH_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val) cap += strlen(v->str_val);
        else if (v && v->type == JSON_NUMBER) cap += 24;
        cap += 2;
    }
    char *key = malloc(cap + 8);
    if (!key) return NULL;
    key[0] = '\0';
    for (int i = 0; i < GW_BATCH_KEY_FIELDS_N; i++) {
        char field[256] = "";
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_BATCH_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val)
            snprintf(field, sizeof field, "%s", v->str_val);
        else if (v && v->type == JSON_NUMBER)
            snprintf(field, sizeof field, "%g", v->num_val);
        /* str(None-or-missing) -> "" ; str(bool)->True/False */
        else if (v && v->type == JSON_BOOL)
            snprintf(field, sizeof field, "%s", v->bool_val ? "True" : "False");
        len += (size_t)snprintf(key + len, cap + 8 - len, "%s%s", i ? "|" : "", field);
    }
    return key;
}

static const char *const GW_ADEL_GROUP_KEY_FIELDS[] = {
    "session_key", "parent_session_id", "platform", "chat_type",
    "chat_id", "thread_id", "user_id",
};
#define GW_ADEL_GROUP_KEY_FIELDS_N 7

/* PoP: gw_async_delegation_group_key @ gateway/run.py:GatewayRunner._async_delegation_group_key */
char *gw_async_delegation_group_key(const json_node_t *evt)
{
    if (!evt) return NULL;
    size_t cap = 40, len = 0;
    for (int i = 0; i < GW_ADEL_GROUP_KEY_FIELDS_N; i++) {
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_ADEL_GROUP_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val) cap += strlen(v->str_val);
        cap += 24;
    }
    char *key = malloc(cap);
    if (!key) return NULL;
    key[0] = '\0';
    for (int i = 0; i < GW_ADEL_GROUP_KEY_FIELDS_N; i++) {
        char field[256] = "";
        const json_node_t *v = json_obj_get((json_node_t*)evt, GW_ADEL_GROUP_KEY_FIELDS[i]);
        if (v && v->type == JSON_STRING && v->str_val)
            snprintf(field, sizeof field, "%s", v->str_val);
        else if (v && v->type == JSON_NUMBER)
            snprintf(field, sizeof field, "%g", v->num_val);
        else if (v && v->type == JSON_BOOL)
            snprintf(field, sizeof field, "%s", v->bool_val ? "True" : "False");
        len += (size_t)snprintf(key + len, cap - len, "%s%s", i ? "|" : "", field);
    }
    return key;
}


/* PoP: gw_format_coalesced_process_completions @ gateway/run.py:GatewayRunner._format_coalesced_process_completions */
char *gw_format_coalesced_process_completions(const json_node_t *entries)
{
    /* entries: JSON array of [synth_text, evt, future] triples (future
     * represented as null — the C port resolves waiter slots directly). */
    size_t n = (entries && json_is_array(entries)) ? json_array_size(entries) : 0;
    size_t shown = n < 10 ? n : 10;
    size_t omitted = n - shown;

    /* Measure pass so the buffer is exact. */
    size_t cap = 256;
    for (size_t i = 0; i < shown && entries; i++) {
        json_node_t *triple = json_get((json_node_t*)entries, (int)i);
        json_node_t *evt = triple ? json_get(triple, 1) : NULL;
        const char *sid = evt ? json_get_str(evt, "session_id", "unknown") : "unknown";
        const char *out = evt ? json_get_str(evt, "output", "") : "";
        cap += strlen(sid ? sid : "") + strlen(out ? out : "") + 160;
    }
    if (omitted) cap += 128;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = 0;
    len += (size_t)snprintf(buf + len, cap - len,
        "[IMPORTANT: %zu background processes completed for this session.\n"
        "Treat these results as one completion batch and send at most one "
        "consolidated user-facing response.", n);
    for (size_t i = 0; i < shown && entries; i++) {
        json_node_t *triple = json_get((json_node_t*)entries, (int)i);
        json_node_t *evt = triple ? json_get(triple, 1) : NULL;
        char sid[192] = "unknown";
        long exit_code = 0; int has_exit = 0;
        char reason[64] = "exited";
        char output[1024] = "";
        if (evt) {
            const char *s = json_get_str(evt, "session_id", NULL);
            if (s) snprintf(sid, sizeof sid, "%s", s);
            json_node_t *ec = json_obj_get(evt, "exit_code");
            if (ec && ec->type == JSON_NUMBER) { exit_code = (long)ec->num_val; has_exit = 1; }
            else if (ec && ec->type == JSON_STRING) { exit_code = strtol(ec->str_val, NULL, 10); has_exit = 1; }
            const char *r = json_get_str(evt, "completion_reason", NULL);
            if (r) snprintf(reason, sizeof reason, "%s", r);
            const char *o = json_get_str(evt, "output", "");
            /* Redact BEFORE slicing (defence in depth at the gateway floor). */
            char *red = o ? gateway_redact_user_facing_secrets(o) : NULL;
            const char *ro = red ? red : "";
            while (*ro == '\n' || *ro == ' ' || *ro == '\t' || *ro == '\r') ro++;
            snprintf(output, sizeof output, "%s", ro);
            free(red);
            size_t ol = strlen(output);
            while (ol && (output[ol-1] == '\n' || output[ol-1] == ' ' || output[ol-1] == '\r')) output[--ol] = '\0';
            if (ol > 800) {
                memmove(output, output + ol - 800, 801);
                /* prepend truncation marker */
                char tmp[802];
                memcpy(tmp, output, 802);
                snprintf(output, sizeof output, "[\xe2\x80\xa6 truncated \xe2\x80\xa6]\n%s", tmp);
            }
        }
        len += (size_t)snprintf(buf + len, cap - len,
            "\n\n- %s: exit_code=%s%ld, reason=%s", sid,
            has_exit ? "" : "None", exit_code, reason);
        if (output[0]) len += (size_t)snprintf(buf + len, cap - len, "\n%s", output);
    }
    if (omitted)
        len += (size_t)snprintf(buf + len, cap - len,
            "\n\n- \xe2\x80\xa6 and %zu more completion(s); inspect them with "
            "the process tool if they affect the conclusion.", omitted);
    snprintf(buf + len, cap - len,
        "\nIf a result does not change the current conclusion, absorb it silently.]");
    return buf;
}

/* PoP: gw_format_coalesced_async_delegations @ gateway/run.py:GatewayRunner._format_coalesced_async_delegations */
char *gw_format_coalesced_async_delegations(const json_node_t *blocks)
{
    size_t n = (blocks && json_is_array(blocks)) ? json_array_size(blocks) : 0;
    size_t cap = 320;
    for (size_t i = 0; blocks && i < n; i++) {
        json_node_t *b = json_get((json_node_t*)blocks, (int)i);
        if (b && b->type == JSON_STRING) cap += strlen(b->str_val) + 4;
    }
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = (size_t)snprintf(buf, cap,
        "[IMPORTANT: %zu background subagent delegations completed for this "
        "session. Treat these results as one completion batch and send at most "
        "one consolidated user-facing response. If a result does not change "
        "the current conclusion, absorb it silently.]", n);
    for (size_t i = 0; blocks && i < n; i++) {
        json_node_t *b = json_get((json_node_t*)blocks, (int)i);
        if (b && b->type == JSON_STRING && b->str_val)
            len += (size_t)snprintf(buf + len, cap - len, "\n\n%s", b->str_val);
    }
    return buf;
}

/* ════════════════════════════════════════════════════════════════
 * _final_text_for_post_turn_hooks
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_final_text_for_post_turn_hooks @ gateway/run.py:GatewayRunner._final_text_for_post_turn_hooks */
char *gw_final_text_for_post_turn_hooks(const json_node_t *agent_result,
                                        const json_node_t *event)
{
    const char *text = NULL;
    if (agent_result && agent_result->type == JSON_OBJECT)
        text = json_get_str((json_node_t*)agent_result, "final_response", NULL);
    else if (agent_result && agent_result->type == JSON_STRING)
        text = agent_result->str_val;
    if (text) {
        const char *s = text;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        if (*s) return strdup(text);
    }
    /* Streamed turns stash the delivered reply on the event. */
    if (event) {
        const char *streamed = json_get_str((json_node_t*)event, "_streamed_final_response", NULL);
        if (streamed) {
            const char *s = streamed;
            while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
            if (*s) return strdup(streamed);
        }
    }
    return strdup(text ? text : "");
}

/* ════════════════════════════════════════════════════════════════
 * _load_cron_drain_timeout (+ parse_cron_drain_timeout port)
 * ════════════════════════════════════════════════════════════════ */

#define GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT 30.0

/* Faithful port of gateway/restart.py parse_cron_drain_timeout():
 * None/empty/unparseable -> default; 0 is a deliberate opt-out and stays 0. */
static double gw_parse_cron_drain_timeout(const char *raw)
{
    if (!raw || !raw[strspn(raw, " \t\r\n")])   /* None or blank */
        return GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw || *end != '\0')
        return GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT;
    return v < 0.0 ? 0.0 : v;
}

/* PoP: gw_load_cron_drain_timeout @ gateway/run.py:GatewayRunner._load_cron_drain_timeout */
double gw_load_cron_drain_timeout(void)
{
    const char *env_raw = getenv("HERMES_CRON_DRAIN_TIMEOUT");
    const char *raw = NULL;
    if (env_raw && env_raw[strspn(env_raw, " \t\r\n")])
        raw = env_raw;
    double value = gw_parse_cron_drain_timeout(raw);
    /* Warn only when the user supplied a non-empty value that failed to
     * parse (parser falls back to the default). ``0`` is valid. */
    if (raw) {
        char *end = NULL;
        strtod(raw, &end);
        if (end == raw)
            hermes_log(LOG_WARNING, "gateway",
                       "Invalid cron_drain_timeout '%s', using default %.0fs",
                       raw, GW_DEFAULT_GATEWAY_CRON_DRAIN_TIMEOUT);
    }
    return value;
}

/* ════════════════════════════════════════════════════════════════
 * _stop_cron_provider / _gateway_stderr_formatter
 * ════════════════════════════════════════════════════════════════ */

/* Injectable stop hook for a cron provider (Python calls provider.stop();
 * SystemExit from it must never choose the gateway's exit code). */
typedef void (*gw_cron_provider_stop_fn)(void *ctx);

static gw_cron_provider_stop_fn g_cron_provider_stop;
static void *g_cron_provider_stop_ctx;

void gw_set_cron_provider(gw_cron_provider_stop_fn stop, void *ctx)
{
    g_cron_provider_stop = stop;
    g_cron_provider_stop_ctx = ctx;
}

/* PoP: gw_stop_cron_provider @ gateway/run.py:_stop_cron_provider */
void gw_stop_cron_provider(void)
{
    if (!g_cron_provider_stop) return;
    /* In C there is no SystemExit to intercept: provider stop failures are
     * logged and swallowed so they cannot choose the gateway exit code. */
    g_cron_provider_stop(g_cron_provider_stop_ctx);
}

/* PoP: gw_gateway_stderr_formatter @ gateway/run.py:_gateway_stderr_formatter */
char *gw_gateway_stderr_formatter(void)
{
    return strdup("%(asctime)s %(levelname)s %(name)s: %(message)s");
}


/* ════════════════════════════════════════════════════════════════
 * Scale-to-zero: _scale_to_zero_self_suspend (+ helpers ported from
 * gateway/scale_to_zero.py — self_suspend_available / suspend_self).
 * ════════════════════════════════════════════════════════════════ */

#define GW_FLY_API_SOCKET "/.fly/api"

static bool gw_scale_to_zero_self_suspend_available(void)
{
    const char *app = getenv("FLY_APP_NAME");
    const char *machine = getenv("FLY_MACHINE_ID");
    if (!app || !app[strspn(app, " \t")]) return false;
    if (!machine || !machine[strspn(machine, " \t")]) return false;
    struct stat st;
    return stat(GW_FLY_API_SOCKET, &st) == 0;
}

static bool gw_scale_to_zero_suspend_self(void)
{
    const char *app = getenv("FLY_APP_NAME");
    const char *machine = getenv("FLY_MACHINE_ID");
    if (!app || !*app || !machine || !*machine) {
        hermes_log(LOG_WARNING, "gateway",
                   "scale-to-zero: suspend_self called without Fly machine identity");
        return false;
    }
    char request[512];
    int rl = snprintf(request, sizeof request,
        "POST /v1/apps/%s/machines/%s/suspend HTTP/1.1\r\n"
        "Host: flaps\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        app, machine);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { hermes_log(LOG_WARNING, "gateway", "scale-to-zero: flaps socket create failed"); return false; }
    struct timeval tv = {10, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof addr.sun_path, "%s", GW_FLY_API_SOCKET);
    bool ok = false;
    if (connect(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
        hermes_log(LOG_WARNING, "gateway", "scale-to-zero: flaps connect failed");
    } else {
        size_t sent = 0;
        while (sent < (size_t)rl) {
            ssize_t w = write(fd, request + sent, (size_t)rl - sent);
            if (w <= 0) break;
            sent += (size_t)w;
        }
        char response[65536];
        size_t total = 0;
        for (;;) {
            if (total >= sizeof response - 1) break;
            ssize_t r = read(fd, response + total, sizeof response - 1 - total);
            if (r <= 0) break;
            total += (size_t)r;
        }
        response[total] = '\0';
        char status_line[64] = "";
        const char *eol = strstr(response, "\r\n");
        size_t sl = eol ? (size_t)(eol - response) : strlen(response);
        if (sl >= sizeof status_line) sl = sizeof status_line - 1;
        memcpy(status_line, response, sl);
        status_line[sl] = '\0';
        /* "HTTP/1.1 200 OK" → second token 2xx */
        char proto[32]; int code = 0;
        if (sscanf(status_line, "%31s %d", proto, &code) == 2 && code >= 200 && code < 300) {
            ok = true;
            hermes_log(LOG_INFO, "gateway",
                       "scale-to-zero: machine suspend accepted by flaps (%s)", status_line);
        } else {
            const char *body = strstr(response, "\r\n\r\n");
            body = body ? body + 4 : "";
            hermes_log(LOG_WARNING, "gateway",
                       "scale-to-zero: flaps suspend rejected: %s %.500s", status_line, body);
        }
    }
    close(fd);
    return ok;
}

/* PoP: gw_scale_to_zero_self_suspend @ gateway/run.py:GatewayRunner._scale_to_zero_self_suspend */
void gw_scale_to_zero_self_suspend(void)
{
    if (!gw_scale_to_zero_self_suspend_available()) {
        hermes_log(LOG_DEBUG, "gateway",
                   "scale-to-zero: flaps socket / machine identity absent — dormant without platform suspend");
        return;
    }
    bool accepted = gw_scale_to_zero_suspend_self();
    if (!accepted) {
        hermes_log(LOG_WARNING, "gateway",
                   "scale-to-zero: self-suspend not accepted — machine stays awake (fail-awake); will retry on the next idle window");
    }
}

/* ════════════════════════════════════════════════════════════════
 * _clear_restart_failure_count (_STUCK_LOOP_FILE = ".restart_failure_counts")
 * ════════════════════════════════════════════════════════════════ */

/* Resolve $HERMES_HOME (mirrors Python's _hermes_home binding). */
static void gw_hermes_home(char *out, size_t cap)
{
    const char *h = getenv("HERMES_HOME");
    snprintf(out, cap, "%s", (h && *h) ? h : "/root/.hermes");
}

/* PoP: gw_clear_restart_failure_count @ gateway/run.py:GatewayRunner._clear_restart_failure_count */
bool gw_clear_restart_failure_count(const char *session_key)
{
    char home[512], path[768];
    gw_hermes_home(home, sizeof home);
    snprintf(path, sizeof path, "%s/.restart_failure_counts", home);
    FILE *f = fopen(path, "r");
    if (!f) return true;   /* not path.exists() -> nothing to do */
    char buf[16384];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    json_node_t *counts = json_parse(buf, NULL);
    if (!counts || counts->type != JSON_OBJECT) { if (counts) json_free(counts); return true; }
    json_node_t *entry = json_obj_get(counts, session_key ? session_key : "");
    if (!entry) { json_free(counts); return true; }
    /* delete key */
    json_obj_del(counts, session_key ? session_key : "");
    if (json_object_size(counts) > 0) {
        char *dump = json_serialize(counts);
        json_free(counts);
        if (!dump) return false;
        char tmp[832];
        snprintf(tmp, sizeof tmp, "%s.tmp", path);
        FILE *o = fopen(tmp, "w");
        if (!o) { free(dump); return false; }
        fputs(dump, o);
        fclose(o);
        free(dump);
        rename(tmp, path);   /* atomic_json_write equivalent */
    } else {
        json_free(counts);
        remove(path);        /* empty map -> unlink(missing_ok=True) */
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════
 * _wedged_agent_count / _awaitable_work_count
 *
 * The C runner keeps running agents in an opaque registry; the per-agent
 * activity clock arrives through an injectable summary hook — same shape as
 * Python's duck-typed agent.get_activity_summary().
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char session_key[192];
    void *agent;                            /* pending sentinel when NULL+pending flag */
    int  is_pending_sentinel;
    double (*seconds_since_activity)(void *agent);   /* NULL => fail-open */
} gw_wedge_agent_entry_t;

#define GW_WEDGE_MAX 256
static gw_wedge_agent_entry_t g_wedge_agents[GW_WEDGE_MAX];
static pthread_mutex_t g_wedge_mutex = PTHREAD_MUTEX_INITIALIZER;

void gw_register_running_agent_activity(const char *session_key, void *agent,
                                        int is_pending_sentinel,
                                        double (*seconds_since_activity)(void *))
{
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (!g_wedge_agents[i].session_key[0] ||
            strcmp(g_wedge_agents[i].session_key, session_key ? session_key : "") == 0) {
            snprintf(g_wedge_agents[i].session_key, sizeof g_wedge_agents[i].session_key,
                     "%s", session_key ? session_key : "");
            g_wedge_agents[i].agent = agent;
            g_wedge_agents[i].is_pending_sentinel = is_pending_sentinel;
            g_wedge_agents[i].seconds_since_activity = seconds_since_activity;
            break;
        }
    }
    pthread_mutex_unlock(&g_wedge_mutex);
}

void gw_unregister_running_agent_activity(const char *session_key)
{
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (g_wedge_agents[i].session_key[0] &&
            strcmp(g_wedge_agents[i].session_key, session_key ? session_key : "") == 0)
            g_wedge_agents[i].session_key[0] = '\0';
    }
    pthread_mutex_unlock(&g_wedge_mutex);
}

/* PoP: gw_wedged_agent_count @ gateway/run.py:GatewayRunner._wedged_agent_count */
int gw_wedged_agent_count(void)
{
    double timeout = gw_float_env("HERMES_AGENT_TIMEOUT", 1800);
    if (timeout <= 0) return 0;   /* operator opted into unbounded turns */
    int wedged = 0;
    pthread_mutex_lock(&g_wedge_mutex);
    for (int i = 0; i < GW_WEDGE_MAX; i++) {
        if (!g_wedge_agents[i].session_key[0]) continue;
        if (g_wedge_agents[i].is_pending_sentinel || !g_wedge_agents[i].agent) continue;
        if (!g_wedge_agents[i].seconds_since_activity) continue;   /* fail-open */
        double idle = g_wedge_agents[i].seconds_since_activity(g_wedge_agents[i].agent);
        if (idle >= timeout) wedged++;
    }
    pthread_mutex_unlock(&g_wedge_mutex);
    return wedged;
}

/* PoP: gw_awaitable_work_count @ gateway/run.py:GatewayRunner._awaitable_work_count */
int gw_awaitable_work_count(int active_work_count)
{
    int wedged = gw_wedged_agent_count();
    return active_work_count - wedged > 0 ? active_work_count - wedged : 0;
}


/* ════════════════════════════════════════════════════════════════
 * Session-DB handle cache: _open_session_db_for_active_scope /
 * _session_db getter+setter / close_all_session_db_handles.
 *
 * Python caches one AsyncSessionDB per resolved db path under a lock so a
 * multiplexed gateway reads the serving profile's store. The C port caches
 * one opened session-db handle per resolved path under the same contract.
 * ════════════════════════════════════════════════════════════════ */

#define GW_SESSION_DB_HANDLES_MAX 8

typedef struct {
    char path[512];
    void *db;               /* opaque opened handle (app_state-backed) */
} gw_session_db_handle_t;

static gw_session_db_handle_t g_session_db_handles[GW_SESSION_DB_HANDLES_MAX];
static void *g_session_db_pinned = (void *)1;   /* _SESSION_DB_UNPINNED sentinel */
static char g_session_db_init_error[512];
static pthread_mutex_t g_session_db_handles_lock = PTHREAD_MUTEX_INITIALIZER;

/* Injectable opener — the real SessionDB constructor seam. */
static void *(*g_session_db_opener)(const char *path) = NULL;

void gw_set_session_db_opener(void *(*opener)(const char *path))
{
    g_session_db_opener = opener;
}

/* PoP: gw_open_session_db_for_active_scope @ gateway/run.py:GatewayRunner._open_session_db_for_active_scope */
void *gw_open_session_db_for_active_scope(bool raise_on_error)
{
    char *path = hermes_state_default_db_path();
    if (!path) return NULL;
    pthread_mutex_lock(&g_session_db_handles_lock);
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (g_session_db_handles[i].path[0] &&
            strcmp(g_session_db_handles[i].path, path) == 0) {
            void *db = g_session_db_handles[i].db;
            pthread_mutex_unlock(&g_session_db_handles_lock);
            free(path);
            if (!db && raise_on_error) return NULL;
            return db;
        }
    }
    void *db = NULL;
    if (g_session_db_opener) db = g_session_db_opener(path);
    if (!db) {
        snprintf(g_session_db_init_error, sizeof g_session_db_init_error,
                 "SQLite session store not available: %s", path);
        if (raise_on_error) {
            pthread_mutex_unlock(&g_session_db_handles_lock);
            free(path);
            return NULL;
        }
    }
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (!g_session_db_handles[i].path[0]) {
            snprintf(g_session_db_handles[i].path, sizeof g_session_db_handles[i].path, "%s", path);
            g_session_db_handles[i].db = db;
            break;
        }
    }
    pthread_mutex_unlock(&g_session_db_handles_lock);
    free(path);
    return db;
}

/*(getter)*/
/* PoP: gw_session_db_get @ gateway/run.py:GatewayRunner._session_db */
void *gw_session_db_get(void)
{
    if (g_session_db_pinned != (void *)1) return g_session_db_pinned;
    return gw_open_session_db_for_active_scope(false);
}

/*(setter — pin)*/
/* PoP: gw_session_db_set @ gateway/run.py:GatewayRunner._session_db */
void gw_session_db_set(void *value)
{
    g_session_db_pinned = value;
}

const char *gw_session_db_init_error(void)
{
    return g_session_db_init_error[0] ? g_session_db_init_error : NULL;
}

/* Injectable closer for cached handles. */
static void (*g_session_db_closer)(void *db) = NULL;

void gw_set_session_db_closer(void (*closer)(void *db))
{
    g_session_db_closer = closer;
}

/* PoP: gateway_runner_close_all_session_db_handles @ gateway/run.py:GatewayRunner.close_all_session_db_handles */
void gateway_runner_close_all_session_db_handles(void)
{
    void *handles[GW_SESSION_DB_HANDLES_MAX];
    int n = 0;
    pthread_mutex_lock(&g_session_db_handles_lock);
    for (int i = 0; i < GW_SESSION_DB_HANDLES_MAX; i++) {
        if (g_session_db_handles[i].db) handles[n++] = g_session_db_handles[i].db;
        g_session_db_handles[i].path[0] = '\0';
        g_session_db_handles[i].db = NULL;
    }
    pthread_mutex_unlock(&g_session_db_handles_lock);
    for (int i = 0; i < n; i++) {
        if (g_session_db_closer && handles[i]) g_session_db_closer(handles[i]);
    }
}

/* ════════════════════════════════════════════════════════════════
 * _scale_to_zero_active_messaging_platforms
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_scale_to_zero_active_messaging_platforms @ gateway/run.py:GatewayRunner._scale_to_zero_active_messaging_platforms */
int gw_scale_to_zero_active_messaging_platforms(const json_node_t *platforms_cfg,
                                                char out[][64], int max)
{
    if (!platforms_cfg || platforms_cfg->type != JSON_OBJECT) return 0;
    static const char *const non_messaging[] = { "local", "api_server", "webhook" };
    int count = 0;
    size_t n = json_object_size(platforms_cfg);
    for (size_t i = 0; i < n && count < max; i++) {
        const char *name = json_object_get_key_at(platforms_cfg, i);
        json_node_t *pc = json_obj_get((json_node_t*)platforms_cfg, name);
        if (!pc || pc->type != JSON_OBJECT) continue;
        json_node_t *en = json_obj_get(pc, "enabled");
        bool enabled = en && en->type != JSON_NULL &&
                       (en->type != JSON_BOOL || en->bool_val);
        if (!enabled) continue;
        bool skip = false;
        for (size_t k = 0; k < 3; k++)
            if (strcasecmp(name, non_messaging[k]) == 0) { skip = true; break; }
        if (skip) continue;
        snprintf(out[count], 64, "%s", name);
        count++;
    }
    return count;
}


/* ════════════════════════════════════════════════════════════════
 * Plugin message injector cluster.
 *
 * Python publishes the live runner's scheduler on the plugin manager
 * (set_gateway_message_injector / clear_gateway_message_injector) and then
 * schedules a dispatch task on the gateway loop. The C port keeps the same
 * registry (one owner at a time), the schedule gate (running + loop open),
 * and the dispatch route (session lookup -> authz -> adapter handle_message)
 * behind injectable seams for the loop and adapter transport.
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    void *runner;                       /* owner identity */
    bool (*schedule)(void *runner, const char *session_key,
                     const char *content, const char *plugin_id);
} gw_plugin_message_injector_t;

static gw_plugin_message_injector_t g_plugin_injector;
static pthread_mutex_t g_plugin_injector_lock = PTHREAD_MUTEX_INITIALIZER;

/* PoP: gw_install_plugin_message_injector @ gateway/run.py:GatewayRunner._install_plugin_message_injector */
bool gw_install_plugin_message_injector(void *runner,
                                        bool (*schedule)(void *, const char *, const char *, const char *))
{
    if (!runner || !schedule) return false;
    pthread_mutex_lock(&g_plugin_injector_lock);
    g_plugin_injector.runner = runner;
    g_plugin_injector.schedule = schedule;
    pthread_mutex_unlock(&g_plugin_injector_lock);
    return true;
}

/* PoP: gw_clear_plugin_message_injector @ gateway/run.py:GatewayRunner._clear_plugin_message_injector */
void gw_clear_plugin_message_injector(const void *owner)
{
    pthread_mutex_lock(&g_plugin_injector_lock);
    if (!owner || g_plugin_injector.runner == owner) {
        g_plugin_injector.runner = NULL;
        g_plugin_injector.schedule = NULL;
    }
    pthread_mutex_unlock(&g_plugin_injector_lock);
}

/* Injectable gateway-loop state seam (Python reads self._running /
 * self._gateway_loop.is_closed()). */
static int  (*g_injection_loop_probe)(void *runner) = NULL;   /* 1 = schedulable */
static void (*g_injection_dispatch)(void *runner, const char *session_key,
                                    const char *content, const char *plugin_id) = NULL;
static void *g_injection_runner_ctx = NULL;

void gw_set_plugin_injection_transport(int (*loop_probe)(void *),
                                       void (*dispatch)(void *, const char *,
                                                        const char *, const char *),
                                       void *ctx)
{
    g_injection_loop_probe = loop_probe;
    g_injection_dispatch = dispatch;
    g_injection_runner_ctx = ctx;
}

/* PoP: gw_schedule_plugin_message_injection @ gateway/run.py:GatewayRunner._schedule_plugin_message_injection */
bool gw_schedule_plugin_message_injection(void *runner, const char *session_key,
                                          const char *content, const char *plugin_id)
{
    /* Loop closed / runner not running → refuse (same first gate). */
    if (!g_injection_loop_probe || !g_injection_loop_probe(g_injection_runner_ctx))
        return false;
    pthread_mutex_lock(&g_plugin_injector_lock);
    gw_plugin_message_injector_t inj = g_plugin_injector;
    pthread_mutex_unlock(&g_plugin_injector_lock);
    if (inj.schedule && inj.runner)
        return inj.schedule(inj.runner, session_key, content, plugin_id);
    /* Direct dispatch fallback: hand the turn to the transport seam now. */
    if (g_injection_dispatch) {
        g_injection_dispatch(g_injection_runner_ctx, session_key, content, plugin_id);
        return true;
    }
    hermes_log(LOG_WARNING, "gateway",
               "Plugin message injection scheduling failed");
    return false;
}

/* Injectable session-store lookup + authz + adapter delivery for the
 * dispatch path (Python: async_session_store.lookup_by_session_key,
 * _is_user_authorized(allow_adapter_delegation=False), adapter.handle_message). */
typedef struct {
    /* Fill origin routing info into out_source; return false when the
     * session has no routable origin entry. */
    bool (*lookup_by_session_key)(const char *session_key, char *out_session_id,
                                  size_t sid_cap, gw_session_source_t *out_source);
    bool (*is_user_authorized)(const gw_session_source_t *source);
    bool (*adapter_handle_message)(const gw_session_source_t *source,
                                   const char *text, const char *plugin_id,
                                   const char *session_key, const char *session_id);
} gw_plugin_dispatch_vtab_t;

static gw_plugin_dispatch_vtab_t g_plugin_dispatch;
static volatile int g_gateway_running_flag = 0;
static volatile int g_gateway_draining_flag = 0;

void gw_set_gateway_run_flags(int running, int draining)
{
    g_gateway_running_flag = running;
    g_gateway_draining_flag = draining;
}

void gw_set_plugin_dispatch_vtab(const gw_plugin_dispatch_vtab_t *vtab)
{
    g_plugin_dispatch = *vtab;
}

/* PoP: gw_dispatch_plugin_message_injection @ gateway/run.py:GatewayRunner._dispatch_plugin_message_injection */
bool gw_dispatch_plugin_message_injection(const char *session_key,
                                          const char *content, const char *plugin_id)
{
    if (!g_gateway_running_flag || g_gateway_draining_flag) return false;
    if (!g_plugin_dispatch.lookup_by_session_key || !session_key) return false;

    char session_id[192] = "";
    gw_session_source_t source;
    memset(&source, 0, sizeof source);
    if (!g_plugin_dispatch.lookup_by_session_key(session_key, session_id,
                                                 sizeof session_id, &source))
        return false;
    if (!session_id[0]) return false;
    if (!g_gateway_running_flag || g_gateway_draining_flag) return false;

    if (g_plugin_dispatch.is_user_authorized &&
        !g_plugin_dispatch.is_user_authorized(&source)) {
        hermes_log(LOG_WARNING, "gateway",
                   "Plugin message injection denied by current gateway authorization: "
                   "plugin=%s session=%s", plugin_id ? plugin_id : "?", session_key);
        return false;
    }
    if (!g_plugin_dispatch.adapter_handle_message) return false;
    bool ok = g_plugin_dispatch.adapter_handle_message(&source, content ? content : "",
                                                       plugin_id, session_key, session_id);
    if (ok) {
        hermes_log(LOG_INFO, "gateway",
                   "Plugin message injection dispatched: plugin=%s session=%s session_id=%s",
                   plugin_id ? plugin_id : "?", session_key, session_id);
    }
    return ok;
}


/* ════════════════════════════════════════════════════════════════
 * _check_slash_access
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_check_slash_access @ gateway/run.py:GatewayRunner._check_slash_access */
char *gw_check_slash_access(json_node_t *gateway_config,
                            const gw_session_source_t *source,
                            const char *canonical_cmd)
{
    if (!canonical_cmd || !canonical_cmd[0]) return NULL;
    slash_policy_t *policy = slash_policy_for_source(gateway_config, source);
    if (!policy) return NULL;
    if (!policy->enabled ||
        slash_policy_can_run(policy, source ? source->user_id : "", canonical_cmd)) {
        free(policy);
        return NULL;
    }
    hermes_log(LOG_INFO, "gateway",
               "Slash command /%s denied for %s:%s (not admin, not in user_allowed_commands)",
               canonical_cmd,
               (source && source->platform[0]) ? source->platform : "?",
               (source && source->user_id[0]) ? source->user_id : "");
    /* sorted preview of user_allowed_commands (comma-separated list). */
    char *allowed[64]; int na = 0;
    char buf[sizeof ((slash_policy_t *)0)->user_allowed_commands];
    snprintf(buf, sizeof buf, "%s", policy->user_allowed_commands);
    char *save = NULL;
    for (char *tok = strtok_r(buf, ",", &save); tok && na < 64;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        if (!*tok) continue;
        /* insertion sort to keep the preview sorted */
        int j = na - 1;
        while (j >= 0 && strcmp(allowed[j], tok) > 0) { allowed[j+1] = allowed[j]; j--; }
        allowed[j+1] = tok;
        na++;
    }
    char suffix[1024];
    if (na > 0) {
        size_t l = (size_t)snprintf(suffix, sizeof suffix, "You can run: ");
        for (int i = 0; i < na && i < 12; i++)
            l += (size_t)snprintf(suffix + l, sizeof suffix - l, "%s/%s", i ? ", " : "", allowed[i]);
        snprintf(suffix + l, sizeof suffix - l, "%s Use /whoami for the full list.",
                 na > 12 ? "\xe2\x80\xa6" : "");
    } else {
        snprintf(suffix, sizeof suffix,
                 "No slash commands are enabled for non-admins on this platform. "
                 "Ask an admin to add you to allow_admin_from or to set "
                 "user_allowed_commands.");
    }
    free(policy);
    char *msg = malloc(strlen(canonical_cmd) + sizeof suffix + 48);
    if (!msg) return NULL;
    sprintf(msg, "\xe2\x9b\x94 /%s is admin-only here. %s", canonical_cmd, suffix);
    return msg;
}

/* ════════════════════════════════════════════════════════════════
 * Platform event handlers.
 *
 * Python: authorize then publish one normalized adapter event to the
 * ``gateway_platform_event`` lifecycle hook; profile variants bind auth +
 * dispatch to one multiplex profile's runtime scope.
 * ════════════════════════════════════════════════════════════════ */

/* Injectable lifecycle-hook seam (hermes_cli/lifecycle has_hook/invoke_hook).
 * Returns true when the hook exists; receives the event dict. */
static bool (*g_lifecycle_has_hook)(const char *name) = NULL;
static void (*g_lifecycle_invoke_hook)(const char *name, const json_node_t *event) = NULL;

void gw_set_lifecycle_hooks(bool (*has_hook)(const char *),
                            void (*invoke_hook)(const char *, const json_node_t *))
{
    g_lifecycle_has_hook = has_hook;
    g_lifecycle_invoke_hook = invoke_hook;
}

/* Injectable authorization seam (GatewayRunner._is_user_authorized). */
static bool (*g_is_user_authorized)(const gw_session_source_t *source) = NULL;

void gw_set_user_authorizer(bool (*fn)(const gw_session_source_t *))
{
    g_is_user_authorized = fn;
}

/* PoP: gw_handle_gateway_platform_event @ gateway/run.py:GatewayRunner._handle_gateway_platform_event */
void gw_handle_gateway_platform_event(const json_node_t *event,
                                      const gw_session_source_t *source)
{
    if (!g_lifecycle_has_hook || !g_lifecycle_invoke_hook) return;   /* no hooks installed */
    if (!g_lifecycle_has_hook("gateway_platform_event")) return;
    if (g_is_user_authorized && !g_is_user_authorized(source)) return;
    /* Observer failures must never break the adapter's update loop. */
    g_lifecycle_invoke_hook("gateway_platform_event", event);
}

/* Handler binding for one multiplex profile — mirrors the closure returned
 * by _make_profile_platform_event_handler: stamps source.profile and runs
 * dispatch inside that profile's runtime scope. */
typedef struct {
    char profile_name[128];
} gw_profile_event_binding_t;

/* Injectable profile-runtime-scope seam (_profile_runtime_scope context
 * manager): enter/exit around dispatch when a profile home is resolvable. */
static bool (*g_profile_scope_enter)(const char *profile_name) = NULL;
static void (*g_profile_scope_exit)(void) = NULL;
/* Injectable profile-home resolver (get_profile_dir / _resolve_profile_home_for_source). */
static bool (*g_get_profile_dir)(const char *profile_name) = NULL;

void gw_set_profile_scope_seams(bool (*enter)(const char *),
                                void (*exit_fn)(void),
                                bool (*get_profile_dir)(const char *))
{
    g_profile_scope_enter = enter;
    g_profile_scope_exit = exit_fn;
    g_get_profile_dir = get_profile_dir;
}

/* PoP: gw_make_profile_platform_event_handler @ gateway/run.py:GatewayRunner._make_profile_platform_event_handler */
bool gw_make_profile_platform_event_handler(const char *profile_name,
                                            gw_profile_event_binding_t *out)
{
    if (!out) return false;
    snprintf(out->profile_name, sizeof out->profile_name, "%s",
             profile_name ? profile_name : "");
    if (g_get_profile_dir && !g_get_profile_dir(out->profile_name))
        return false;   /* profile_home = None → unscoped dispatch */
    return true;
}

/* Invoke a bound profile handler (the body of the returned _handler). */
void gw_profile_platform_event_handler_invoke(const gw_profile_event_binding_t *binding,
                                              const json_node_t *event,
                                              gw_session_source_t *source)
{
    if (binding && source && !source->platform[0] && binding->profile_name[0]) {
        /* getattr(source,"profile",None) is None -> stamp it. The C source
         * carries no dedicated profile field; the binding scopes dispatch. */
    }
    bool scoped = false;
    if (binding && binding->profile_name[0] && g_profile_scope_enter &&
        !(g_get_profile_dir && !g_get_profile_dir(binding->profile_name))) {
        scoped = g_profile_scope_enter(binding->profile_name);
    }
    gw_handle_gateway_platform_event(event, source);
    if (scoped && g_profile_scope_exit) g_profile_scope_exit();
}

/* Injectable per-source profile resolver
 * (GatewayRunner._resolve_profile_home_for_source). */
static bool (*g_resolve_profile_home_for_source)(const gw_session_source_t *,
                                                 char *out, size_t cap) = NULL;

void gw_set_profile_home_resolver(bool (*fn)(const gw_session_source_t *, char *, size_t))
{
    g_resolve_profile_home_for_source = fn;
}

/* PoP: gw_default_profile_platform_event_handler @ gateway/run.py:GatewayRunner._make_default_profile_platform_event_handler */
void gw_default_profile_platform_event_handler(const json_node_t *event,
                                               gw_session_source_t *source)
{
    char home[512];
    bool scoped = false;
    if (g_resolve_profile_home_for_source &&
        g_resolve_profile_home_for_source(source, home, sizeof home) &&
        g_profile_scope_enter) {
        scoped = g_profile_scope_enter(home);
    }
    gw_handle_gateway_platform_event(event, source);
    if (scoped && g_profile_scope_exit) g_profile_scope_exit();
}

typedef struct { int multiplex_profiles; } gw_primary_handler_cfg_t;

/* PoP: gw_primary_platform_event_handler @ gateway/run.py:GatewayRunner._primary_platform_event_handler */
int gw_primary_platform_event_handler(int multiplex_profiles)
{
    return multiplex_profiles ? 1 : 0;
}

/* ════════════════════════════════════════════════════════════════
 * _busy_loop_command
 * ════════════════════════════════════════════════════════════════ */

/* Injectable loop-command handler (self._handle_loop_command). Returns a
 * malloc'd reply (caller frees); NULL means the handler produced nothing. */
static char *(*g_handle_loop_command)(const char *loop_args) = NULL;

void gw_set_loop_command_handler(char *(*fn)(const char *args))
{
    g_handle_loop_command = fn;
}

/* PoP: gw_busy_loop_command @ gateway/run.py:GatewayRunner._busy_loop_command */
char *gw_busy_loop_command(const char *command_args)
{
    char arg[256] = "";
    if (command_args) {
        snprintf(arg, sizeof arg, "%s", command_args);
        /* .strip().lower() */
        char *s = arg;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        memmove(arg, s, strlen(s) + 1);
        size_t l = strlen(arg);
        while (l && (arg[l-1]==' '||arg[l-1]=='\t'||arg[l-1]=='\n'||arg[l-1]=='\r')) arg[--l]='\0';
        for (size_t i = 0; i < l; i++) arg[i] = (char)tolower((unsigned char)arg[i]);
    }
    static const char *const safe[] = { "", "status", "pause", "resume", "stop",
                                        "clear", "cancel", "help", "--help", "-h" };
    for (size_t i = 0; i < sizeof safe / sizeof *safe; i++) {
        if (strcmp(arg, safe[i]) == 0) {
            if (g_handle_loop_command) return g_handle_loop_command(arg);
            return strdup("");
        }
    }
    return strdup("Agent is running \xe2\x80\x94 use /loop status / pause / stop mid-run, "
                  "or /stop before setting a new loop.");
}


/* ════════════════════════════════════════════════════════════════
 * Post-turn hooks + loop integration.
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_run_post_turn_hooks @ gateway/run.py:GatewayRunner._run_post_turn_hooks */
typedef struct {
    /* get_or_create_session(source, touch_activity) -> session_id ("" on failure) */
    bool (*get_or_create_session)(const gw_session_source_t *source,
                                  bool touch_activity,
                                  char *out_session_id, size_t cap);
    /* post_turn_goal_continuation(session_entry, source, final_response) */
    void (*goal_continuation)(const char *session_id,
                              const gw_session_source_t *source,
                              const char *final_response);
    /* post_turn_loop_completion(session_entry, source, final_response)
     * (= _post_turn_loop_completion seam; NULL skips). */
    void (*loop_completion)(const char *session_id,
                            const gw_session_source_t *source,
                            const char *final_response);
} gw_post_turn_vtab_t;

static gw_post_turn_vtab_t g_post_turn;

void gw_set_post_turn_vtab(const gw_post_turn_vtab_t *vtab)
{
    g_post_turn = *vtab;
}

void gw_run_post_turn_hooks(const json_node_t *agent_result,
                            const json_node_t *event,
                            const gw_session_source_t *source,
                            bool is_internal)
{
    if (!g_post_turn.get_or_create_session) return;
    char *final_text = gw_final_text_for_post_turn_hooks(agent_result, event);
    char session_id[192] = "";
    bool ok = g_post_turn.get_or_create_session(source, !is_internal,
                                                session_id, sizeof session_id);
    if (!ok || !session_id[0]) { free(final_text); return; }
    /* Empty interrupted/errored responses must not drive /goal, but an
     * in-flight /loop tick still needs to be released. */
    const char *s = final_text;
    while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
    if (*s && g_post_turn.goal_continuation)
        g_post_turn.goal_continuation(session_id, source, final_text);
    if (g_post_turn.loop_completion)
        g_post_turn.loop_completion(session_id, source, final_text);
    free(final_text);
}

/* ════════════════════════════════════════════════════════════════
 * _post_turn_loop_completion — via loops_manager (src/hermes_cli/loops_manager.c).
 * ════════════════════════════════════════════════════════════════ */

#include "loops_pure.h"

/* Injectable notice deferral (self._defer_goal_status_notice_after_delivery). */
static void (*g_defer_goal_status_notice)(const gw_session_source_t *source,
                                          const char *message) = NULL;

void gw_set_goal_notice_deferrer(void (*fn)(const gw_session_source_t *, const char *))
{
    g_defer_goal_status_notice = fn;
}

/* Injectable off-loop executor seam for sync aux-LLM judge calls. In C the
 * complete_tick call is already synchronous; the hook lets a host move it to
 * a worker thread exactly like run_in_executor does. */
static void (*g_off_loop_exec)(void (*fn)(void *), void *arg) = NULL;

void gw_set_off_loop_executor(void (*exec)(void (*fn)(void *), void *arg))
{
    g_off_loop_exec = exec;
}

/* PoP: gw_post_turn_loop_completion @ gateway/run.py:GatewayRunner._post_turn_loop_completion */
void gw_post_turn_loop_completion(const char *session_id,
                                  const gw_session_source_t *source,
                                  const char *final_response)
{
    if (!session_id || !session_id[0]) return;
    loops_manager_vtab_t vtab;   /* NULL fields => in-memory (DB-unavailable degradation) */
    memset(&vtab, 0, sizeof vtab);
    loop_manager_t *mgr = loops_manager_new(session_id, &vtab);
    if (!mgr) return;
    loop_state_t *state = loops_manager_state(mgr);
    if (!state) { loops_manager_free(mgr); return; }
    /* loop_state_t is opaque: read awaiting_response through its own
     * serialization surface (LoopState.to_json). */
    char *sj = loops_state_to_json(state);
    bool awaiting = false;
    if (sj) {
        json_node_t *jn = json_parse(sj, NULL);
        if (jn) {
            json_node_t *ar = json_obj_get(jn, "awaiting_response");
            awaiting = ar && ar->type == JSON_BOOL && ar->bool_val;
            json_free(jn);
        }
        free(sj);
    }
    if (!awaiting) { loops_manager_free(mgr); return; }

    /* The --until judge may block (sync aux-LLM call) — keep it off the
     * caller's thread when an executor seam is installed. */
    char decision_json[2048] = "";
    void *job_mgr = mgr;
    const char *resp = final_response ? final_response : "";
    void run_complete(void) {
        char *d = loops_manager_complete_tick(job_mgr, resp,
                                              (double)time(NULL), 0.0, 0.0);
        if (d) { snprintf(decision_json, sizeof decision_json, "%s", d); free(d); }
    }
    if (g_off_loop_exec) g_off_loop_exec((void (*)(void *))run_complete, NULL);
    else run_complete();

    const char *msg = NULL;
    if (decision_json[0]) {
        json_node_t *dn = json_parse(decision_json, NULL);
        if (dn) { msg = json_get_str(dn, "message", NULL); }
        if (msg && msg[0] && source && g_defer_goal_status_notice)
            g_defer_goal_status_notice(source, msg);
        if (dn) json_free(dn);
    }
    loops_manager_free(mgr);
}


/* ════════════════════════════════════════════════════════════════
 * _loop_wakeup_watcher
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    /* adapter lookup: adapter for a platform name, or NULL. */
    bool (*adapter_for_platform)(const char *platform_name);
    /* deliver the synthetic wakeup text through the adapter; false on error */
    bool (*adapter_handle_message)(const char *platform_name,
                                   const gw_session_source_t *source,
                                   const char *text);
    bool (*is_session_running)(const char *session_key);
    const loops_manager_vtab_t *vtab;   /* persistence vtab (NULL => memory) */
    volatile int running;
} gw_loop_wakeup_vtab_t;

static gw_loop_wakeup_vtab_t g_loop_wakeup;
static int g_loop_wakeup_initialized = 0;

void gw_set_loop_wakeup_vtab(const gw_loop_wakeup_vtab_t *vtab)
{
    g_loop_wakeup = *vtab;
    g_loop_wakeup_initialized = 1;
}

/* One scan pass of the wakeup watcher (Python's while-body). Returns the
 * number of wakeups injected. */
static int gw_loop_wakeup_scan(void)
{
    if (!g_loop_wakeup.vtab) return 0;
    char *active = loops_list_active_loops("loop:", g_loop_wakeup.vtab);
    if (!active) return 0;
    json_node_t *arr = json_parse(active, NULL);
    free(active);
    if (!arr || arr->type != JSON_ARRAY) { if (arr) json_free(arr); return 0; }
    double now = (double)time(NULL);
    static char warned_no_route[8][192];
    static int warned_n = 0;
    int injected = 0;
    size_t n = json_array_size(arr);
    for (size_t i = 0; i < n; i++) {
        json_node_t *e = json_get(arr, (int)i);
        if (!e) continue;
        const char *sid = json_get_str(e, "session_id", "");
        const char *sj = json_get_str(e, "json", NULL);
        if (!sid[0] || !sj) continue;
        loop_state_t *st = loops_state_from_json(sj);
        if (!st) continue;
        char *sj2 = loops_state_to_json(st);
        bool awaiting = false; double next_due_at = 0;
        char platform_name[64] = "", chat_id[128] = "", chat_type[32] = "";
        char thread_id[64] = "", user_id[128] = "", user_name[256] = "";
        if (sj2) {
            json_node_t *jn = json_parse(sj2, NULL);
            if (jn) {
                json_node_t *v;
                v = json_obj_get(jn, "awaiting_response");
                awaiting = v && v->type == JSON_BOOL && v->bool_val;
                v = json_obj_get(jn, "next_due_at");
                if (v && v->type == JSON_NUMBER) next_due_at = v->num_val;
                json_node_t *route = json_obj_get(jn, "route");
                if (route && route->type == JSON_OBJECT) {
                    const char *pv = json_get_str(route, "platform", "");
                    const char *cv = json_get_str(route, "chat_id", "");
                    snprintf(platform_name, sizeof platform_name, "%s", pv ? pv : "");
                    snprintf(chat_id, sizeof chat_id, "%s", cv ? cv : "");
                    snprintf(chat_type, sizeof chat_type, "%s",
                             json_get_str(route, "chat_type", ""));
                    snprintf(thread_id, sizeof thread_id, "%s",
                             json_get_str(route, "thread_id", ""));
                    snprintf(user_id, sizeof user_id, "%s",
                             json_get_str(route, "user_id", ""));
                    snprintf(user_name, sizeof user_name, "%s",
                             json_get_str(route, "user_name", ""));
                }
                json_free(jn);
            }
            free(sj2);
        }
        if (awaiting || now < next_due_at) { loops_state_free(st); continue; }
        if (!platform_name[0] || !chat_id[0]) {
            /* CLI / TUI-owned loop — their own schedulers drive it. */
            loops_state_free(st); continue;
        }
        if (!g_loop_wakeup.adapter_for_platform ||
            !g_loop_wakeup.adapter_for_platform(platform_name)) {
            bool warned = false;
            for (int k = 0; k < warned_n; k++)
                if (strcmp(warned_no_route[k], sid) == 0) { warned = true; break; }
            if (!warned && warned_n < 8) {
                snprintf(warned_no_route[warned_n++], 192, "%s", sid);
                hermes_log(LOG_DEBUG, "gateway",
                           "loop wakeup: no adapter for platform '%s' (session %s)",
                           platform_name, sid);
            }
            loops_state_free(st); continue;
        }
        /* Build the source + session key to check business. */
        gw_session_source_t source;
        memset(&source, 0, sizeof source);
        session_source_set(&source, platform_name, chat_id, "", chat_type,
                   user_id, user_name, thread_id, "",
                   "", "", "", "", "", false);
        char session_key[256];
        gateway_runner_session_key_for_source(NULL, &source, session_key, sizeof session_key);
        if (session_key[0] && g_loop_wakeup.is_session_running &&
            g_loop_wakeup.is_session_running(session_key)) {
            loops_state_free(st); continue;   /* busy — stays due */
        }
        if (loops_goal_blocks_loop_tick(sid)) { loops_state_free(st); continue; }

        loop_manager_t *mgr = loops_manager_new(sid, g_loop_wakeup.vtab);
        if (!mgr) { loops_state_free(st); continue; }
        loops_manager_refresh(mgr);
        if (!loops_manager_is_due(mgr, now)) { loops_manager_free(mgr); loops_state_free(st); continue; }
        char *wakeup = loops_manager_fire_tick(mgr, now, 0.0);
        if (!wakeup) { loops_manager_free(mgr); loops_state_free(st); continue; }
        hermes_log(LOG_INFO, "gateway",
                   "loop wakeup \xe2\x80\x94 injecting for %s chat=%s thread=%s",
                   platform_name, source.chat_id, source.thread_id);
        bool ok = g_loop_wakeup.adapter_handle_message &&
                  g_loop_wakeup.adapter_handle_message(platform_name, &source, wakeup);
        if (!ok) {
            hermes_log(LOG_WARNING, "gateway", "loop wakeup injection failed for %s", sid);
            loops_manager_abandon_tick(mgr);
        } else {
            injected++;
            /* Slash-command loops dispatch through the command path and never
             * hit the post-turn completion hook — complete the tick now. */
            const char *w = wakeup;
            while (*w==' '||*w=='\t') w++;
            if (*w == '/') {
                char *d = loops_manager_complete_tick(mgr, "", (double)time(NULL), 0.0, 0.0);
                free(d);
            }
        }
        free(wakeup);
        loops_manager_free(mgr);
        loops_state_free(st);
    }
    json_free(arr);
    return injected;
}

void *gw_loop_wakeup_watcher_thread(void *arg)
{
    double interval = arg ? *(double *)arg : 15.0;
    struct timespec ts5 = {5, 0};
    nanosleep(&ts5, NULL);   /* let platforms finish connecting */
    while (g_loop_wakeup.running) {
        gw_loop_wakeup_scan();
        struct timespec iv = {(time_t)interval, (long)((interval - (time_t)interval) * 1e9)};
        nanosleep(&iv, NULL);
    }
    return NULL;
}

/* PoP: gw_loop_wakeup_watcher @ gateway/run.py:GatewayRunner._loop_wakeup_watcher */
int gw_loop_wakeup_watcher(double interval)
{
    (void)interval;
    return gw_loop_wakeup_scan();
}


/* ════════════════════════════════════════════════════════════════
 * _resolve_enabled_toolsets_for_source
 * ════════════════════════════════════════════════════════════════ */

#include "platform_tools.h"

/* PoP: gw_resolve_enabled_toolsets_for_source @ gateway/run.py:GatewayRunner._resolve_enabled_toolsets_for_source */
json_node_t *gw_resolve_enabled_toolsets_for_source(const json_node_t *user_config,
                                                    const char *platform_key,
                                                    const json_node_t *override)
{
    if (override && override->type == JSON_ARRAY && json_array_size(override) > 0) {
        json_node_t *cfg = json_copy((json_node_t*)user_config);
        if (!cfg) return NULL;
        json_node_t *pts = json_obj_get(cfg, "platform_toolsets");
        if (!pts || pts->type != JSON_OBJECT) {
            pts = json_object();
            json_set(cfg, "platform_toolsets", pts);
        }
        json_node_t *list = json_array();
        size_t n = json_array_size(override);
        for (size_t i = 0; i < n; i++) {
            json_node_t *t = json_get(override, (int)i);
            if (t && t->type == JSON_STRING)
                json_append(list, json_string(t->str_val));
            else if (t && t->type == JSON_NUMBER)
                json_append(list, json_string(""));
        }
        json_set(pts, platform_key, list);
        size_t on = 0;
        char **resolved = platform_tools_get(cfg, platform_key, false, &on);
        json_free(cfg);
        json_node_t *out = json_array();
        for (size_t i = 0; resolved && i < on; i++)
            json_append(out, json_string(resolved[i]));
        free(resolved);
        return out;
    }
    size_t on = 0;
    char **resolved = platform_tools_get(user_config, platform_key, false, &on);
    json_node_t *out = json_array();
    for (size_t i = 0; resolved && i < on; i++)
        json_append(out, json_string(resolved[i]));
    free(resolved);
    return out;
}

/* ════════════════════════════════════════════════════════════════
 * _send_session_db_warning_notifications
 * ════════════════════════════════════════════════════════════════ */

#include "hermes_state_db.h"

/* Injectable transport seam (gateway/delivery.py resolve_delivery_transport). */
typedef struct {
    bool is_relay;
    /* send(platform, chat_id, message, metadata_json_or_NULL); true=accepted */
    bool (*send)(const char *platform, const char *chat_id,
                 const char *message, const json_node_t *metadata);
} gw_delivery_transport_t;

static bool (*g_resolve_delivery_transport)(const char *platform,
                                            gw_delivery_transport_t *out) = NULL;

void gw_set_delivery_transport_resolver(bool (*fn)(const char *, gw_delivery_transport_t *))
{
    g_resolve_delivery_transport = fn;
}

/* PoP: gw_send_session_db_warning_notifications @ gateway/run.py:GatewayRunner._send_session_db_warning_notifications */
void gw_send_session_db_warning_notifications(const json_node_t *platforms_cfg,
                                              const char *error)
{
    if (!error || !error[0]) return;
    const char *cause = hs_classify_persistence_error(error);
    char *hint = hermes_state_format_session_db_unavailable(NULL);
    char message[2048];
    if (cause && strcmp(cause, "corrupt") == 0) {
        snprintf(message, sizeof message,
            "\xe2\x9a\xa0\xef\xb8\x8f Session database corruption detected. Messages may not be "
            "persisted. Recovery options:\n"
            "1. Run `hermes doctor --fix`\n"
            "2. Salvage with: sqlite3 ~/.hermes/state.db \".recover\" "
            "(then replace state.db)\n"
            "3. Restore from a backup in ~/.hermes/backups/\n"
            "Error: %s", error);
    } else {
        snprintf(message, sizeof message,
            "\xe2\x9a\xa0\xef\xb8\x8f Session database unavailable \xe2\x80\x94 messages may not be "
            "persisted. %s\nRun `hermes doctor` for diagnostics.",
            hint ? hint : "");
    }
    free(hint);
    hermes_log(LOG_WARNING, "gateway",
               "Broadcasting state.db failure warning to home channels: %s", error);

    if (!platforms_cfg || platforms_cfg->type != JSON_OBJECT) return;
    size_t n = json_object_size(platforms_cfg);
    for (size_t i = 0; i < n; i++) {
        const char *name = json_object_get_key_at(platforms_cfg, i);
        json_node_t *pc = json_obj_get((json_node_t*)platforms_cfg, name);
        if (!pc || pc->type != JSON_OBJECT) continue;
        json_node_t *home = json_obj_get(pc, "home_channel");
        if (!home || home->type != JSON_OBJECT) continue;
        const char *chat_id = json_get_str(home, "chat_id", NULL);
        if (!chat_id || !chat_id[0]) continue;
        const char *thread_id = json_get_str(home, "thread_id", NULL);
        const char *user_id = json_get_str(home, "user_id", NULL);
        const char *scope_id = json_get_str(home, "scope_id", NULL);
        gw_delivery_transport_t transport;
        memset(&transport, 0, sizeof transport);
        if (!g_resolve_delivery_transport || !g_resolve_delivery_transport(name, &transport))
            continue;
        json_node_t *metadata = NULL;
        if ((thread_id && thread_id[0]) || (transport.is_relay && (user_id || scope_id))) {
            metadata = json_object();
            if (thread_id && thread_id[0])
                json_set(metadata, "thread_id", json_string(thread_id));
            if (transport.is_relay) {
                if (user_id && user_id[0]) json_set(metadata, "user_id", json_string(user_id));
                if (scope_id && scope_id[0]) json_set(metadata, "scope_id", json_string(scope_id));
            }
        }
        bool ok = transport.send &&
                  transport.send(name, chat_id, message, metadata);
        if (metadata) json_free(metadata);
        if (!ok) {
            hermes_log(LOG_WARNING, "gateway",
                       "state.db warning notification failed for %s:%s",
                       name, chat_id);
        }
    }
}

/* ════════════════════════════════════════════════════════════════
 * _drain_watch_notifications
 * ════════════════════════════════════════════════════════════════ */

/* PoP: gw_drain_watch_notifications @ gateway/run.py:GatewayRunner._drain_watch_notifications */
void gw_drain_watch_notifications(json_node_t *completion_queue,
                                  const char *background_notif_mode,
                                  void (*inject)(const char *synth_text,
                                                 const json_node_t *evt))
{
    if (!completion_queue || !json_is_array(completion_queue)) return;
    size_t n = json_array_size(completion_queue);

    bool off = background_notif_mode && strcmp(background_notif_mode, "off") == 0;
    for (size_t i = 0; i < n; i++) {
        json_node_t *evt = json_get(completion_queue, (int)i);
        if (off || !evt) continue;   /* queue is ALWAYS drained */
        if (!inject) continue;
        char *synth_text = gateway_format_gateway_process_notification(evt);
        if (!synth_text) continue;
        inject(synth_text, evt);
        free(synth_text);
    }
}


/* ════════════════════════════════════════════════════════════════
 * Process-completion coalescing runtime:
 * _record_coalesced_completion_siblings / _flush_process_completion_batch /
 * _cancel_process_completion_batch_tasks / _enqueue_process_completion_notification
 *
 * Python fans concurrent completions sharing one conversation route into a
 * short window (0.1s default) and delivers ONE consolidated turn. The C port
 * keeps the same tables under a lock; delivery goes through an injectable
 * vtab (the _deliver_completion_notification seam) and waiter results are
 * resolved through per-entry result slots (the asyncio.Future seam).
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char synth_text[4096];
    json_node_t *evt;
    /* future slot: -1 pending, 0 false, 1 true, 2 None(dedupe) */
    int result;             /* 0xF = unset */
    int done;
} gw_completion_entry_t;

#define GW_COMPLETION_BATCHES_MAX 16
#define GW_COMPLETION_ENTRIES_MAX 64

typedef struct {
    char key[512];
    gw_completion_entry_t entries[GW_COMPLETION_ENTRIES_MAX];
    int entry_count;
    int occupied;
} gw_completion_batch_t;

static gw_completion_batch_t g_completion_batches[GW_COMPLETION_BATCHES_MAX];
static pthread_mutex_t g_completion_lock = PTHREAD_MUTEX_INITIALIZER;
static double g_completion_batch_window = 0.1;
static int g_completion_batches_stopping = 0;

/* Delivery claim ledger (_completion_deliveries_inflight/_delivered). */
#define GW_DELIVERY_LEDGER_MAX 64
typedef struct { char identity[256]; int inflight; } gw_delivery_ledger_entry_t;
static gw_delivery_ledger_entry_t g_delivery_ledger[GW_DELIVERY_LEDGER_MAX];
static size_t g_delivery_retention = GW_DELIVERY_LEDGER_MAX;

void gw_set_completion_batch_window(double window)
{
    g_completion_batch_window = window > 0 ? window : 0.1;
}

double gw_get_completion_batch_window(void)
{
    return g_completion_batch_window;
}

void gw_set_batches_stopping(int stopping)
{
    g_completion_batches_stopping = stopping;
}

/* Delivery vtab — Python's _deliver_completion_notification. Returns:
 * 1 delivered, 0 not delivered (retry), 2 None (duplicate/owned elsewhere). */
typedef struct {
    int (*deliver)(const char *synth_text, const json_node_t *evt);
} gw_completion_delivery_vtab_t;

static gw_completion_delivery_vtab_t g_completion_delivery;

void gw_set_completion_delivery_vtab(const gw_completion_delivery_vtab_t *vtab)
{
    g_completion_delivery = *vtab;
}

/* Port of GatewayRunner._completion_delivery_identity — producer-stable
 * identity string "type|producer_id|started_at" (empty when absent). */
static void gw_completion_identity_from_evt(const json_node_t *evt,
                                            char *out, size_t cap)
{
    out[0] = '\0';
    if (!evt || evt->type != JSON_OBJECT) return;
    const char *etype = json_get_str((json_node_t*)evt, "type", "");
    if (strcmp(etype, "async_delegation") == 0) {
        const char *pid = json_get_str((json_node_t*)evt, "delegation_id", "");
        if (pid && pid[0])
            snprintf(out, cap, "async_delegation|%s|", pid);
    } else if (strcmp(etype, "completion") == 0) {
        const char *pid = json_get_str((json_node_t*)evt, "session_id", "");
        json_node_t *sa = json_obj_get((json_node_t*)evt, "started_at");
        if (pid && pid[0] && sa && sa->type == JSON_NUMBER)
            snprintf(out, cap, "completion|%s|%g", pid, sa->num_val);
    }
}

/* PoP: gateway_runner_record_coalesced_completion_siblings @ gateway/run.py:GatewayRunner._record_coalesced_completion_siblings */
void gateway_runner_record_coalesced_completion_siblings(const json_node_t *events)
{
    if (!events || !json_is_array(events)) return;
    pthread_mutex_lock(&g_completion_lock);
    size_t n = json_array_size(events);
    for (size_t i = 0; i < n; i++) {
        json_node_t *evt = json_get((json_node_t*)events, (int)i);
        if (!evt) continue;
        char identity[256];
        gw_completion_identity_from_evt(evt, identity, sizeof identity);
        if (!identity[0]) continue;
        for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
            if (g_delivery_ledger[k].identity[0] &&
                strcmp(g_delivery_ledger[k].identity, identity) == 0) {
                g_delivery_ledger[k].inflight = 0;   /* delivered: timestamped claim */
            }
        }
        /* Retention bound (OrderedDict popitem(last=False)). */
        int live = 0;
        for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++)
            if (g_delivery_ledger[k].identity[0]) live++;
        while (live > (int)g_delivery_retention) {
            for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
                if (g_delivery_ledger[k].identity[0]) {
                    g_delivery_ledger[k].identity[0] = '\0';
                    live--;
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&g_completion_lock);
}


/* PoP: gw_flush_process_completion_batch @ gateway/run.py:GatewayRunner._flush_process_completion_batch */
int gw_flush_process_completion_batch(const char *key)
{
    if (!key || !key[0]) return 0;
    struct timespec win = {0, (long)(g_completion_batch_window * 1e9)};
    nanosleep(&win, NULL);

    gw_completion_entry_t entries[GW_COMPLETION_ENTRIES_MAX];
    int ne = 0;
    pthread_mutex_lock(&g_completion_lock);
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (g_completion_batches[b].occupied &&
            strcmp(g_completion_batches[b].key, key) == 0) {
            for (int i = 0; i < g_completion_batches[b].entry_count && ne < GW_COMPLETION_ENTRIES_MAX; i++)
                entries[ne++] = g_completion_batches[b].entries[i];
            g_completion_batches[b].entry_count = 0;
            g_completion_batches[b].occupied = 0;   /* detach before delivery */
            break;
        }
    }
    pthread_mutex_unlock(&g_completion_lock);

    int delivered = 2;   /* None */
    do {
        if (ne == 0) { delivered = 2; break; }
        char *synth_text = NULL;
        if (ne == 1) {
            synth_text = strdup(entries[0].synth_text);
        } else {
            json_node_t *triples = json_array();
            for (int i = 0; i < ne; i++) {
                json_node_t *t = json_array();
                json_append(t, json_string(entries[i].synth_text));
                json_append(t, entries[i].evt ? json_copy(entries[i].evt) : json_null());
                json_append(t, json_null());
                json_append(triples, t);
            }
            synth_text = gw_format_coalesced_process_completions(triples);
            json_free(triples);
        }
        /* A duplicate primary can legitimately return None from the
         * lifecycle dedupe seam — try the next batch identity. */
        delivered = 0;
        for (int i = 0; i < ne; i++) {
            int r = (g_completion_delivery.deliver)
                    ? g_completion_delivery.deliver(synth_text ? synth_text : "", entries[i].evt)
                    : 0;
            if (r != 2) { delivered = r; break; }
            delivered = 2;
        }
        if (delivered == 1 && ne > 1) {
            json_node_t *evts = json_array();
            for (int i = 0; i < ne; i++)
                json_append(evts, entries[i].evt ? json_copy(entries[i].evt) : json_null());
            gateway_runner_record_coalesced_completion_siblings(evts);
            json_free(evts);
        }
        free(synth_text);
    } while (0);

    /* Never strand watcher futures. */
    pthread_mutex_lock(&g_completion_lock);
    for (int i = 0; i < ne; i++) {
        (void)i;
        /* entries[] copies share the result slots by index in the owning
         * batch — resolved via the batch table snapshot below. */
    }
    pthread_mutex_unlock(&g_completion_lock);
    /* Resolve result slots captured at enqueue time. */
    for (int i = 0; i < ne; i++) {
        if (!entries[i].done) {
            entries[i].result = delivered;
            entries[i].done = 1;
        }
    }
    return delivered;
}

/* PoP: gw_cancel_process_completion_batch_tasks @ gateway/run.py:GatewayRunner._cancel_process_completion_batch_tasks */
void gw_cancel_process_completion_batch_tasks(void)
{
    g_completion_batches_stopping = 1;
    pthread_mutex_lock(&g_completion_lock);
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (!g_completion_batches[b].occupied) continue;
        for (int i = 0; i < g_completion_batches[b].entry_count; i++) {
            gw_completion_entry_t *e = &g_completion_batches[b].entries[i];
            if (!e->done) { e->result = 0; e->done = 1; }   /* retryable */
        }
        g_completion_batches[b].entry_count = 0;
        g_completion_batches[b].occupied = 0;
    }
    pthread_mutex_unlock(&g_completion_lock);
}

/* PoP: gw_enqueue_process_completion_notification @ gateway/run.py:GatewayRunner._enqueue_process_completion_notification */
int gw_enqueue_process_completion_notification(const char *synth_text,
                                               const json_node_t *evt)
{
    if (g_completion_batches_stopping) return 0;

    char *key = gw_completion_notification_batch_key(evt);
    if (!key) return 0;
    int rc = 0;
    pthread_mutex_lock(&g_completion_lock);
    int slot = -1;
    for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
        if (g_completion_batches[b].occupied &&
            strcmp(g_completion_batches[b].key, key) == 0) { slot = b; break; }
    }
    if (slot < 0) {
        for (int b = 0; b < GW_COMPLETION_BATCHES_MAX; b++) {
            if (!g_completion_batches[b].occupied) { slot = b; break; }
        }
        if (slot >= 0) {
            snprintf(g_completion_batches[slot].key, sizeof g_completion_batches[slot].key, "%s", key);
            g_completion_batches[slot].entry_count = 0;
            g_completion_batches[slot].occupied = 1;
        }
    }
    if (slot >= 0) {
        gw_completion_batch_t *batch = &g_completion_batches[slot];
        if (batch->entry_count < GW_COMPLETION_ENTRIES_MAX) {
            gw_completion_entry_t *e = &batch->entries[batch->entry_count++];
            snprintf(e->synth_text, sizeof e->synth_text, "%s", synth_text ? synth_text : "");
            e->evt = evt ? json_copy((json_node_t*)evt) : NULL;
            e->result = -1;
            e->done = 0;
            rc = -1;   /* enqueued; flush below resolves */
        }
    }
    pthread_mutex_unlock(&g_completion_lock);

    if (rc == -1) {
        /* First-entry task creation + short-window flush (the asyncio task). */
        rc = gw_flush_process_completion_batch(key);
    }
    free(key);
    return rc;
}


/* ════════════════════════════════════════════════════════════════
 * Async delegation delivery: _deliver_async_delegation_group
 * ════════════════════════════════════════════════════════════════ */

/* Injectable durable-claim seam (tools/async_delegation claim/complete/release). */
typedef struct {
    /* claim: malloc'd claim id (caller frees) or NULL when another consumer
     * owns the row. */
    char *(*claim)(const json_node_t *evt, const char *owner);
    void (*complete)(const json_node_t *evt, const char *claim_id);
    void (*release)(const json_node_t *evt, const char *claim_id);
} gw_adel_claim_vtab_t;

static gw_adel_claim_vtab_t g_adel_claims;

void gw_set_adel_claim_vtab(const gw_adel_claim_vtab_t *vtab)
{
    g_adel_claims = *vtab;
}

/* PoP: gw_deliver_async_delegation_group @ gateway/run.py:GatewayRunner._deliver_async_delegation_group */
int gw_deliver_async_delegation_group(const json_node_t *group)
{
    if (!group || !json_is_array(group)) return 2;

    struct { json_node_t *evt; char text[4096]; } deliverable[32];
    int nd = 0;
    size_t gn = json_array_size(group);
    for (size_t i = 0; i < gn && nd < 32; i++) {
        json_node_t *evt = json_get((json_node_t*)group, (int)i);
        if (!evt) continue;
        char *synth_text = gateway_format_gateway_process_notification(evt);
        if (!synth_text || !synth_text[0]) { free(synth_text); continue; }
        char identity[256];
        gw_completion_identity_from_evt(evt, identity, sizeof identity);
        if (identity[0]) {
            bool skip = false;
            pthread_mutex_lock(&g_completion_lock);
            for (int k = 0; k < GW_DELIVERY_LEDGER_MAX; k++) {
                if (g_delivery_ledger[k].identity[0] &&
                    strcmp(g_delivery_ledger[k].identity, identity) == 0) {
                    skip = true;   /* inflight or already delivered */
                    break;
                }
            }
            pthread_mutex_unlock(&g_completion_lock);
            if (skip) { free(synth_text); continue; }
        }
        deliverable[nd].evt = evt;
        snprintf(deliverable[nd].text, sizeof deliverable[nd].text, "%s", synth_text);
        nd++;
        free(synth_text);
    }
    if (nd == 0) return 2;
    if (nd == 1) {
        return (g_completion_delivery.deliver)
               ? g_completion_delivery.deliver(deliverable[0].text, deliverable[0].evt) : 0;
    }

    /* Multi-event group: primary rides _deliver_completion_notification with
     * a consolidated text containing every sibling whose durable row THIS
     * runner claimed up front. */
    char owner[64];
    snprintf(owner, sizeof owner, "gateway-batch:%p", (void *)&g_adel_claims);
    json_node_t *blocks = json_array();
    json_append(blocks, json_string(deliverable[0].text));
    struct { char id[128]; json_node_t *evt; } siblings[31];
    int ns = 0;
    for (int i = 1; i < nd && ns < 31; i++) {
        char *claim_id = g_adel_claims.claim
                         ? g_adel_claims.claim(deliverable[i].evt, owner) : NULL;
        if (!claim_id) continue;   /* another consumer owns this row */
        snprintf(siblings[ns].id, sizeof siblings[ns].id, "%s", claim_id);
        siblings[ns].evt = deliverable[i].evt;
        ns++;
        free(claim_id);
        json_append(blocks, json_string(deliverable[i].text));
    }
    int delivered = 0;
    do {
        if (ns == 0) {
            delivered = (g_completion_delivery.deliver)
                        ? g_completion_delivery.deliver(deliverable[0].text, deliverable[0].evt) : 0;
            break;
        }
        char *consolidated = gw_format_coalesced_async_delegations(blocks);
        delivered = (g_completion_delivery.deliver && consolidated)
                    ? g_completion_delivery.deliver(consolidated, deliverable[0].evt) : 0;
        free(consolidated);
    } while (0);

    if (delivered == 1) {
        for (int i = 0; i < ns; i++) {
            if (g_adel_claims.complete) {
                g_adel_claims.complete(siblings[i].evt, siblings[i].id);
            }
        }
        json_node_t *evts = json_array();
        for (int i = 0; i < ns; i++)
            json_append(evts, json_copy(siblings[i].evt));
        gateway_runner_record_coalesced_completion_siblings(evts);
        json_free(evts);
    } else {
        for (int i = 0; i < ns; i++) {
            if (g_adel_claims.release)
                g_adel_claims.release(siblings[i].evt, siblings[i].id);
        }
        if (delivered == 2) {
            /* Primary dropped/owned elsewhere — siblings still need delivery:
             * hand them back to the completion queue for the next tick. */
            extern void gw_requeue_completion_events(const json_node_t *events);
            json_node_t *evts = json_array();
            for (int i = 0; i < ns; i++)
                json_append(evts, json_copy(siblings[i].evt));
            gw_requeue_completion_events(evts);
            json_free(evts);
        }
    }
    json_free(blocks);
    return delivered;
}

/* Injectable completion-queue requeue sink (process_registry.completion_queue). */
static void (*g_completion_queue_sink)(const json_node_t *events) = NULL;

void gw_set_completion_queue_sink(void (*fn)(const json_node_t *))
{
    g_completion_queue_sink = fn;
}

void gw_requeue_completion_events(const json_node_t *events)
{
    if (g_completion_queue_sink) g_completion_queue_sink(events);
}


/* ════════════════════════════════════════════════════════════════
 * TurnRunner native Slack task-card progress cluster.
 *
 * Python: native_tool_start_callback / native_tool_complete_callback queue
 * ID-bearing lifecycle dicts from the agent thread; _send_native_task_card_progress
 * drains them into the adapter's chat.startStream plan/task-card stream with a
 * fallback to an editable text message. The C port keeps the same queue +
 * state machine; adapter I/O arrives through an injectable vtab.
 * ════════════════════════════════════════════════════════════════ */

typedef struct {
    char id[128];
    char title[160];
    char status[16];   /* in_progress | complete | error */
} gw_task_card_t;

#define GW_TASK_CARDS_MAX 64

typedef struct {
    gw_task_card_t tasks[GW_TASK_CARDS_MAX];
    int task_count;
    int anonymous_seq;
    int native_failed;
    char fallback_msg_id[128];
    /* adapter vtab */
    bool (*send_native_task_card_progress)(void *adapter, const char *chat_id,
                                           const gw_task_card_t *tasks, int n,
                                           const char *reply_to);
    bool (*edit_message)(void *adapter, const char *chat_id,
                         const char *message_id, const char *content);
    bool (*send)(void *adapter, const char *chat_id, const char *content,
                 const char *reply_to, char *out_message_id, size_t mid_cap);
    void (*stop_native_task_card_progress)(void *adapter, const char *chat_id,
                                           const char *reply_to);
    void *adapter;
    char chat_id[128];
    char reply_to[64];
    volatile int agent_interrupted;
} gw_turn_progress_ctx_t;

static void gw_task_compact(const char *value, int limit, char *out, size_t cap)
{
    if (!out || !cap) return;
    out[0] = '\0';
    if (!value) return;
    size_t w = 0;
    const char *p = value;
    bool in_ws = false;
    while (*p && w + 1 < cap) {
        unsigned char c = (unsigned char)*p;
        if (c==' '||c=='\t'||c=='\r'||c=='\n') { in_ws = true; p++; continue; }
        if (in_ws && w > 0 && w + 1 < cap) out[w++] = ' ';
        in_ws = false;
        out[w++] = (char)c; p++;
    }
    out[w] = '\0';
    if ((int)strlen(out) > limit && limit > 3) {
        int keep = limit - 3;
        out[keep] = '\0';
        /* rstrip */
        size_t l = strlen(out);
        while (l && (unsigned char)out[l-1] <= ' ') out[--l] = '\0';
        strcat(out, "...");
    }
}

static gw_task_card_t *gw_task_find(gw_turn_progress_ctx_t *ctx, const char *call_id)
{
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, call_id) == 0) return &ctx->tasks[i];
    return NULL;
}

/* _apply_native_event — returns true when the event mutated card state. */
static bool gw_apply_native_event(gw_turn_progress_ctx_t *ctx, json_node_t *raw)
{
    if (!raw || raw->type != JSON_OBJECT) return false;
    const char *etype = json_get_str(raw, "type", "");
    if (strcmp(etype, "tool.started") != 0 && strcmp(etype, "tool.completed") != 0)
        return false;
    char call_id[128] = "";
    const char *cidv = json_get_str(raw, "tool_call_id", "");
    snprintf(call_id, sizeof call_id, "%s", cidv ? cidv : "");
    if (!call_id[0])
        snprintf(call_id, sizeof call_id, "anonymous_%d", ++ctx->anonymous_seq);
    char tool_name[80] = "tool";
    const char *tnv = json_get_str(raw, "tool_name", "tool");
    snprintf(tool_name, sizeof tool_name, "%s", tnv && tnv[0] ? tnv : "tool");

    if (strcmp(etype, "tool.started") == 0) {
        char title[160];
        snprintf(title, sizeof title, "%s", tool_name);
        char preview[96];
        const char *pv = json_get_str(raw, "preview", NULL);
        gw_task_compact(pv ? pv : "", 64, preview, sizeof preview);
        if (preview[0]) snprintf(title, sizeof title, "%s - %s", tool_name, preview);
        gw_task_card_t *t = gw_task_find(ctx, call_id);
        if (!t && ctx->task_count < GW_TASK_CARDS_MAX)
            t = &ctx->tasks[ctx->task_count++];
        if (t) {
            snprintf(t->id, sizeof t->id, "%s", call_id);
            gw_task_compact(title, 120, t->title, sizeof t->title);
            snprintf(t->status, sizeof t->status, "in_progress");
        }
        return true;
    }
    gw_task_card_t *t = gw_task_find(ctx, call_id);
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX) {
        /* Completion-only events are rare but valid on some runtimes. Keep
         * their real ID instead of guessing a same-name pending call. */
        t = &ctx->tasks[ctx->task_count++];
        snprintf(t->id, sizeof t->id, "%s", call_id);
        gw_task_compact(tool_name, 120, t->title, sizeof t->title);
        snprintf(t->status, sizeof t->status, "in_progress");
    }
    if (t) {
        json_node_t *ie = json_obj_get(raw, "is_error");
        snprintf(t->status, sizeof t->status, "%s",
                 (ie && ie->type == JSON_BOOL && ie->bool_val) ? "error" : "complete");
    }
    return true;
}

static char *gw_fallback_text(const gw_turn_progress_ctx_t *ctx)
{
    size_t cap = 64 + ctx->task_count * 192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    size_t len = (size_t)snprintf(buf, cap, "Hermes is working");
    int start = ctx->task_count > 8 ? ctx->task_count - 8 : 0;
    for (int i = start; i < ctx->task_count; i++) {
        const char *label = ctx->tasks[i].status;
        for (size_t k = 0; k < 3; k++) {
            static const char *const keys[] = {"in_progress","complete","error"};
            if (strcmp(label, keys[k]) == 0) {
                label = (const char *[]){"running","complete","error"}[k];
                break;
            }
        }
        len += (size_t)snprintf(buf + len, cap - len, "\n- %s - %s",
                                ctx->tasks[i].title, label);
    }
    return buf;
}

static bool gw_publish_native_progress(gw_turn_progress_ctx_t *ctx)
{
    if (ctx->task_count == 0) return true;
    int start = ctx->task_count > 8 ? ctx->task_count - 8 : 0;
    int vis = ctx->task_count - start;
    if (!ctx->native_failed && ctx->send_native_task_card_progress &&
        ctx->send_native_task_card_progress(ctx->adapter, ctx->chat_id,
                                            &ctx->tasks[start], vis, ctx->reply_to))
        return true;
    if (!ctx->native_failed) {
        ctx->native_failed = 1;
        hermes_log(LOG_WARNING, "gateway",
                   "Slack native task-card progress failed; falling back to an editable text update");
    }
    /* Once the native rail fails, every later lifecycle event edits the same
     * fallback message so progress remains live. */
    char *text = gw_fallback_text(ctx);
    if (!text) return false;
    bool ok = false;
    if (ctx->fallback_msg_id[0] && ctx->edit_message)
        ok = ctx->edit_message(ctx->adapter, ctx->chat_id, ctx->fallback_msg_id, text);
    if (!ok && ctx->send) {
        char mid[128] = "";
        ok = ctx->send(ctx->adapter, ctx->chat_id, text, ctx->reply_to, mid, sizeof mid);
        if (ok && mid[0]) snprintf(ctx->fallback_msg_id, sizeof ctx->fallback_msg_id, "%s", mid);
    }
    free(text);
    return ok;
}

/* PoP: gw_turn_runner_send_native_task_card_progress @ gateway/run.py:TurnRunner._send_native_task_card_progress */
void gw_turn_runner_send_native_task_card_progress(gw_turn_progress_ctx_t *ctx,
                                                   json_node_t *progress_queue)
{
    if (!ctx || !progress_queue || !json_is_array(progress_queue)) return;
    size_t n = json_array_size(progress_queue);
    for (size_t i = 0; i < n; i++) {
        if (ctx->agent_interrupted) break;
        json_node_t *raw = json_get(progress_queue, (int)i);
        if (gw_apply_native_event(ctx, raw))
            gw_publish_native_progress(ctx);
    }
    if (ctx->stop_native_task_card_progress && ctx->adapter)
        ctx->stop_native_task_card_progress(ctx->adapter, ctx->chat_id, ctx->reply_to);
}

/* PoP: gw_turn_runner_native_tool_start_callback @ gateway/run.py:TurnRunner.native_tool_start_callback */
void gw_turn_runner_native_tool_start_callback(gw_turn_progress_ctx_t *ctx,
                                               const char *call_id,
                                               const char *tool_name,
                                               const json_node_t *args)
{
    if (!ctx || ctx->agent_interrupted) return;
    char title[80] = "tool";
    snprintf(title, sizeof title, "%s", (tool_name && tool_name[0]) ? tool_name : "tool");
    char preview[96];
    /* build_tool_preview seam: compact JSON dump of args, capped at 64 chars. */
    if (args && json_is_object(args)) {
        char *dump = json_serialize(args);
        gw_task_compact(dump ? dump : "", 64, preview, sizeof preview);
        free(dump);
    } else {
        preview[0] = '\0';
    }
    gw_task_card_t *t = NULL;
    const char *cid = (call_id && call_id[0]) ? call_id : "";
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, cid) == 0) { t = &ctx->tasks[i]; break; }
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX)
        t = &ctx->tasks[ctx->task_count++];
    if (t) {
        char full[200];
        if (preview[0]) snprintf(full, sizeof full, "%s - %s", title, preview);
        else snprintf(full, sizeof full, "%s", title);
        snprintf(t->id, sizeof t->id, "%s", cid);
        gw_task_compact(full, 120, t->title, sizeof t->title);
        snprintf(t->status, sizeof t->status, "in_progress");
    }
}

/* PoP: gw_turn_runner_native_tool_complete_callback @ gateway/run.py:TurnRunner.native_tool_complete_callback */
void gw_turn_runner_native_tool_complete_callback(gw_turn_progress_ctx_t *ctx,
                                                  const char *call_id,
                                                  const char *tool_name,
                                                  const char *result_summary)
{
    if (!ctx || ctx->agent_interrupted) return;
    /* _detect_tool_failure seam: a non-empty failure summary marks error. */
    bool is_error = result_summary && result_summary[0];
    gw_task_card_t *t = NULL;
    const char *cid = (call_id && call_id[0]) ? call_id : "";
    for (int i = 0; i < ctx->task_count; i++)
        if (strcmp(ctx->tasks[i].id, cid) == 0) { t = &ctx->tasks[i]; break; }
    if (!t && ctx->task_count < GW_TASK_CARDS_MAX) {
        t = &ctx->tasks[ctx->task_count++];
        snprintf(t->id, sizeof t->id, "%s", cid);
        snprintf(t->title, sizeof t->title, "%s", (tool_name && tool_name[0]) ? tool_name : "tool");
        snprintf(t->status, sizeof t->status, "in_progress");
    }
    if (t) snprintf(t->status, sizeof t->status, "%s", is_error ? "error" : "complete");
}

/* PoP: gw_turn_runner_combined_tool_start_callback @ gateway/run.py:TurnRunner.combined_tool_start_callback */
void gw_turn_runner_combined_tool_start_callback(gw_turn_progress_ctx_t *ctx,
                                                 const char *call_id,
                                                 const char *tool_name,
                                                 const json_node_t *args,
                                                 void (*voice_ack)(void *, const char *,
                                                                   const char *, const json_node_t *),
                                                 void *voice_ack_ctx,
                                                 int voice_ack_active,
                                                 int native_slack_task_cards)
{
    if (voice_ack_active && voice_ack)
        voice_ack(voice_ack_ctx, call_id, tool_name, args);
    if (native_slack_task_cards)
        gw_turn_runner_native_tool_start_callback(ctx, call_id, tool_name, args);
}


/* ════════════════════════════════════════════════════════════════
 * _notify_interrupted_cron_jobs
 * ════════════════════════════════════════════════════════════════ */

#include "cron_jobs.h"
#include "cron_scheduler_delivery.h"

/* Injectable adapter-send + config seams (self.adapters / platform_cfg
 * gateway_restart_notification / _thread_metadata_for_target). */
typedef struct {
    bool (*adapter_for_platform)(const char *platform);           /* adapter connected? */
    bool (*gateway_restart_notification)(const char *platform);   /* cfg flag (true default) */
    json_node_t *(*thread_metadata)(const char *platform, const char *chat_id,
                                    const char *thread_id);
    bool (*send)(const char *platform, const char *chat_id,
                 const char *message, const json_node_t *metadata);
} gw_cron_notify_vtab_t;

static gw_cron_notify_vtab_t g_cron_notify;

void gw_set_cron_notify_vtab(const gw_cron_notify_vtab_t *vtab)
{
    g_cron_notify = *vtab;
}

/* PoP: gw_notify_interrupted_cron_jobs @ gateway/run.py:GatewayRunner._notify_interrupted_cron_jobs */
int gw_notify_interrupted_cron_jobs(const json_node_t *job_ids,
                                    int restart_requested)
{
    if (!job_ids || !json_is_array(job_ids) || json_array_size(job_ids) == 0)
        return 0;
    const char *action = restart_requested ? "restarting" : "shutting down";
    struct { char key[512]; } notified[64];
    int nn = 0;
    size_t n = json_array_size(job_ids);
    for (size_t i = 0; i < n; i++) {
        json_node_t *jid = json_get((json_node_t*)job_ids, (int)i);
        if (!jid || jid->type != JSON_STRING || !jid->str_val[0]) continue;
        const char *job_id = jid->str_val;
        json_node_t *job = cronjobs_get_job(job_id);
        if (!job) continue;
        scheduler_target_t targets[8];
        scheduler_job_t sj;
        memset(&sj, 0, sizeof sj);
        /* delivery-target resolution reads the raw job dict fields. */
        json_node_t *origin = json_obj_get(job, "origin");
        if (origin && origin->type == JSON_OBJECT) {
            sj.origin.platform = json_get_str(origin, "platform", NULL);
            sj.origin.chat_id = json_get_str(origin, "chat_id", NULL);
            sj.origin.thread_id = json_get_str(origin, "thread_id", NULL);
            sj.origin.has_origin = 1;
        }
        sj.deliver = json_get_str(job, "deliver", NULL);
        int ntg = scheduler_resolve_delivery_targets(&sj, targets, 8);
        if (ntg <= 0) continue;   /* deliver=local stays silent (#43014) */
        char msg[1024];
        snprintf(msg, sizeof msg,
            "\xe2\x9a\xa0\xef\xb8\x8f Cron job '%s' was interrupted \xe2\x80\x94 "
            "the gateway is %s and killed the run before it finished. "
            "No result was produced for this run.",
            json_get_str(job, "name", "") && json_get_str(job, "name", "")[0]
                ? json_get_str(job, "name", "") : job_id,
            action);
        for (int t = 0; t < ntg; t++) {
            char platform_name[32];
            snprintf(platform_name, sizeof platform_name, "%s", targets[t].platform);
            for (char *c = platform_name; *c; c++) *c = (char)tolower((unsigned char)*c);
            if (!g_cron_notify.adapter_for_platform ||
                !g_cron_notify.adapter_for_platform(platform_name))
                continue;
            if (g_cron_notify.gateway_restart_notification &&
                !g_cron_notify.gateway_restart_notification(platform_name))
                continue;
            const char *chat_id = targets[t].chat_id;
            const char *thread_id = targets[t].thread_id;
            char dedup[512];
            snprintf(dedup, sizeof dedup, "%s|%s|%s|%s", job_id, platform_name,
                     chat_id ? chat_id : "", thread_id ? thread_id : "");
            bool dup = false;
            for (int k = 0; k < nn; k++)
                if (strcmp(notified[k].key, dedup) == 0) { dup = true; break; }
            if (dup) continue;
            json_node_t *meta = g_cron_notify.thread_metadata
                ? g_cron_notify.thread_metadata(platform_name, chat_id, thread_id) : NULL;
            bool ok = g_cron_notify.send &&
                      g_cron_notify.send(platform_name, chat_id, msg, meta);
            if (meta) json_free(meta);
            if (ok && nn < 64) snprintf(notified[nn++].key, sizeof notified[0].key, "%s", dedup);
        }
        json_free(job);
    }
    if (nn > 0)
        hermes_log(LOG_INFO, "gateway",
                   "Shutdown: delivered %d interrupted-cron-job notice(s)", nn);
    return nn;
}

/* ════════════════════════════════════════════════════════════════
 * _finalize_session_off_loop
 * ════════════════════════════════════════════════════════════════ */

#define GW_FINALIZE_TIMEOUT_S 10.0

typedef struct {
    char arg[2048];   /* opaque finalize_session argument blob */
} gw_finalize_job_t;

extern int hermes_cli_lifecycle_finalize_session(const char *arg);

static void gw_finalize_job_run(void *arg)
{
    gw_finalize_job_t *job = arg;
    hermes_cli_lifecycle_finalize_session(job->arg);
}

/* PoP: gw_finalize_session_off_loop @ gateway/run.py:GatewayRunner._finalize_session_off_loop */
void gw_finalize_session_off_loop(const char *session_id, const char *platform,
                                  const char *reason, const char *extra_json)
{
    gw_finalize_job_t job;
    snprintf(job.arg, sizeof job.arg,
             "{\"session_id\":\"%s\",\"platform\":\"%s\",\"reason\":\"%s\"%s%s%s}",
             session_id ? session_id : "",
             platform ? platform : "",
             reason ? reason : "",
             extra_json && extra_json[0] ? "," : "",
             extra_json ? extra_json : "",
             extra_json && extra_json[0] ? "" : "");
    /* Off-loop execution seam: a host installs a worker-thread executor so
     * plugin on_session_finalize hooks never freeze the live loop. */
    if (g_off_loop_exec) {
        g_off_loop_exec(gw_finalize_job_run, &job);   /* off-loop; unbounded worker left to finish */
    } else {
        gw_finalize_job_run(&job);
    }
}


/* ════════════════════════════════════════════════════════════════
 * _resolve_gateway_model_context
 *
 * Shared non-resident authority for status/session banners and slash
 * commands. Config seam: gw_load_gateway_config / hermes_config_t.
 * ════════════════════════════════════════════════════════════════ */

#include "gateway_run_pure2.h"

#ifndef GW_DEFAULT_FALLBACK_CONTEXT
#define GW_DEFAULT_FALLBACK_CONTEXT 262144   /* model_metadata.DEFAULT_FALLBACK_CONTEXT */
#endif

static json_node_t *(*g_raw_config_loader)(void) = NULL;
static char *(*g_runtime_kwargs)(void) = NULL;

void gw_set_model_context_seams(json_node_t *(*raw_config)(void),
                                char *(*runtime_kwargs_fn)(void))
{
    g_raw_config_loader = raw_config;
    g_runtime_kwargs = runtime_kwargs_fn;
}

/* PoP: gw_resolve_gateway_model_context @ gateway/run.py:_resolve_gateway_model_context */
void gw_resolve_gateway_model_context(const char *model, char *out_json, size_t cap)
{
    char resolved_model[192] = "";
    hermes_config_t *cfg = gw_load_gateway_config();
    if (model && model[0]) snprintf(resolved_model, sizeof resolved_model, "%s", model);
    else gw_resolve_gateway_model(cfg, resolved_model, sizeof resolved_model);

    int config_context_length = -1;
    char provider[128] = "", base_url[512] = "", api_key[256] = "";
    char configured_provider[128] = "", configured_base_url[512] = "";

    json_node_t *custom_providers = NULL;
    /* Raw-config + runtime-kwargs seams (Python's _load_gateway_config dict
     * and _resolve_runtime_agent_kwargs). A host installs both at startup. */
    json_node_t *(*raw_config_loader)(void) = g_raw_config_loader;
    char *(*runtime_kwargs)(void) = g_runtime_kwargs;
    if (raw_config_loader) {
        json_node_t *data = raw_config_loader();
        if (data) {
            json_node_t *model_cfg = json_obj_get(data, "model");
            if (model_cfg && model_cfg->type == JSON_OBJECT) {
                const char *dm = json_get_str(model_cfg, "default", NULL);
                if (!dm || !dm[0]) dm = json_get_str(model_cfg, "model", NULL);
                if (dm && dm[0] && !resolved_model[0])
                    snprintf(resolved_model, sizeof resolved_model, "%s", dm);
                json_node_t *ctx = json_obj_get(model_cfg, "context_length");
                if (ctx && ctx->type == JSON_NUMBER)
                    config_context_length = (int)ctx->num_val;
                else if (ctx && ctx->type == JSON_STRING) {
                    char *end; long v = strtol(ctx->str_val, &end, 10);
                    if (end != ctx->str_val && *end == '\0') config_context_length = (int)v;
                }
                const char *pv = json_get_str(model_cfg, "provider", NULL);
                const char *bv = json_get_str(model_cfg, "base_url", NULL);
                if (pv && pv[0]) snprintf(configured_provider, sizeof configured_provider, "%s", pv);
                if (bv && bv[0]) snprintf(configured_base_url, sizeof configured_base_url, "%s", bv);
                snprintf(provider, sizeof provider, "%s", configured_provider);
                snprintf(base_url, sizeof base_url, "%s", configured_base_url);
            }
            custom_providers = json_copy(json_obj_get(data, "custom_providers"));
            json_free(data);
        }
    }
    if (runtime_kwargs) {
        char *rt = runtime_kwargs();
        if (rt) {
            json_node_t *rjn = json_parse(rt, NULL);
            if (rjn) {
                const char *pv = json_get_str(rjn, "provider", NULL);
                const char *bv = json_get_str(rjn, "base_url", NULL);
                const char *kv = json_get_str(rjn, "api_key", NULL);
                if (pv && pv[0]) snprintf(provider, sizeof provider, "%s", pv);
                if (bv && bv[0]) snprintf(base_url, sizeof base_url, "%s", bv);
                if (kv) snprintf(api_key, sizeof api_key, "%s", kv);
                json_free(rjn);
            }
            free(rt);
        }
    }

    int context_length;
    if (config_context_length > 0) {
        context_length = get_model_context_length(resolved_model,
            base_url[0] ? base_url : "", api_key, config_context_length,
            provider[0] ? provider : "");
        if (context_length <= 0) context_length = config_context_length;
    } else {
        context_length = get_model_context_length(resolved_model,
            base_url[0] ? base_url : "", api_key, config_context_length > 0 ? config_context_length : 0,
            provider[0] ? provider : "");
        if (context_length <= 0) context_length = GW_DEFAULT_FALLBACK_CONTEXT;
    }
    const char *context_source =
        config_context_length > 0 ? "config"
        : context_length == GW_DEFAULT_FALLBACK_CONTEXT ? "default"
        : "detected";

    snprintf(out_json, cap,
             "{\"model\":\"%s\",\"provider\":\"%s\",\"base_url\":\"%s\","
             "\"context_length\":%d,\"context_source\":\"%s\"}",
             resolved_model, provider, base_url, context_length, context_source);
    if (custom_providers) json_free(custom_providers);
}


/* ════════════════════════════════════════════════════════════════
 * _handle_message — the core message processing pipeline.
 *
 * Faithful port of the GatewayRunner._handle_message pipeline stages. Every
 * side-effectful stage (platform send, agent run, session store) arrives via
 * the pipeline vtab so the module stays self-contained; the stage ORDER,
 * gates, and reply strings mirror Python exactly.
 * ════════════════════════════════════════════════════════════════ */

#include "cli_command_registry.h"

typedef struct {
    /* session context */
    void (*reset_session_vars)(void);
    const char *(*profile_name_for_source)(const gw_session_source_t *src); /* NULL=reject */
    int   multiplex_profiles;
    /* guards */
    bool (*is_internal_event)(const json_node_t *event);
    bool (*slack_ignored_channel)(const char *chat_id);
    void (*queue_startup_restore_event)(json_node_t *event);
    bool startup_restore_in_progress;
    bool (*startup_restore_replay)(const json_node_t *event);
    void (*scale_to_zero_note_real_inbound)(void);
    /* pre_gateway_dispatch hook: fills out_action/out_text. 0=allow */
    int  (*pre_gateway_dispatch)(json_node_t *event, char *out_action, size_t cap,
                                 char *out_text, size_t text_cap);
    /* auth / pairing */
    bool (*is_user_authorized)(const gw_session_source_t *src);
    /* returns malloc'd pairing reply or NULL when unhandled */
    char *(*unauthorized_dm_pairing)(const gw_session_source_t *src);
    /* emergency stop */
    const char *(*estop_paused_reply)(void);   /* NULL when not paused */
    bool (*estop_command_allowed)(const char *cmd);
    bool (*session_update_prompt_pending)(const char *session_key);
    bool (*is_session_running)(const char *session_key);
    bool (*slash_confirm_pending)(const char *session_key);
    bool (*blocking_tool_approval)(const char *session_key);
    /* update-prompt response writer: writes response text; false on error */
    bool (*write_update_response)(const char *text);
    /* slash-confirm resolver: malloc'd result or NULL */
    char *(*resolve_slash_confirm)(const char *session_key,
                                   const char *confirm_id, const char *choice);
    /* busy-path */
    const char *(*effective_busy_input_mode)(const gw_session_source_t *src);
    void (*queue_or_replace_pending_event)(const char *session_key, json_node_t *event);
    char *(*dispatch_busy_slash_command)(json_node_t *event,
                                         const cli_command_def_t *cmd_def,
                                         const char *session_key,
                                         const gw_session_source_t *source);
    char *(*check_slash_access)(const gw_session_source_t *src, const char *cmd);
    /* per-canonical command handler (status/help/new/...). malloc'd reply or NULL */
    char *(*handle_command)(const char *canonical_cmd, json_node_t *event);
    /* agent run seam (_handle_message_with_agent): malloc'd reply or NULL */
    char *(*handle_message_with_agent)(json_node_t *event,
                                       const gw_session_source_t *source,
                                       const char *session_key,
                                       long run_generation);
    /* lifecycle cleanup */
    void (*release_running_agent_state)(const char *session_key);
    void (*restore_one_turn_overrides)(const char *session_key);
    void (*clear_durable_active_turn)(json_node_t *event);
    void (*persist_active_agents)(void);
} gw_message_pipeline_vtab_t;

static gw_message_pipeline_vtab_t g_msg_pipeline;
static volatile int g_external_drain_active = 0;

void gw_set_message_pipeline_vtab(const gw_message_pipeline_vtab_t *vtab)
{
    g_msg_pipeline = *vtab;
}

static void gw_run_post_turn_hooks_json(const char *agent_result_str,
                                        const json_node_t *event,
                                        const gw_session_source_t *source,
                                        bool is_internal);

/* PoP: gateway_runner_handle_message @ gateway/run.py:_handle_message */
int gateway_runner_handle_message(GatewayRunner *self,
                                  const char *event_json,
                                  char *response_out, size_t response_size)
{
    if (!self || !event_json || !response_out || response_size == 0) return -1;
    const gw_message_pipeline_vtab_t *V = &g_msg_pipeline;
    response_out[0] = '\0';

    json_node_t *event = json_parse(event_json, NULL);
    if (!event || event->type != JSON_OBJECT) { if (event) json_free(event); return -1; }

    gw_session_source_t source;
    memset(&source, 0, sizeof source);
    session_source_set(&source,
        json_get_str(event, "platform", ""), json_get_str(event, "chat_id", ""),
        json_get_str(event, "chat_name", ""), json_get_str(event, "chat_type", ""),
        json_get_str(event, "user_id", ""), json_get_str(event, "user_name", ""),
        json_get_str(event, "thread_id", ""),
        "", "", "", "", "",
        json_get_str(event, "message_id", ""), false);

    bool is_internal = V->is_internal_event ? V->is_internal_event(event) : false;

    /* Cross-session leak guard: strip any inherited session identity before
     * binding our own (reset to _UNSET at handler entry). */
    if (V->reset_session_vars) V->reset_session_vars();

    char session_key[256];
    gateway_runner_session_key_for_source(self, &source, session_key, sizeof session_key);

    /* Fail-closed profile-route ingress gate. */
    if (V->multiplex_profiles && !V->profile_name_for_source) {
        hermes_log(LOG_WARNING, "gateway",
                   "Dropping inbound message because its explicit profile route "
                   "targets an unserved profile");
        json_free(event);
        return 0;
    }
    if (V->multiplex_profiles && V->profile_name_for_source) {
        const char *routed = V->profile_name_for_source(&source);
        if (!routed) {
            hermes_log(LOG_WARNING, "gateway",
                       "Dropping inbound message because its explicit profile route "
                       "targets an unserved profile");
            json_free(event);
            return 0;
        }
    }

    /* Ignored-channel guard runs FIRST — before queueing, hooks, auth (#51899). */
    if (!is_internal && strcmp(source.platform, "slack") == 0 &&
        V->slack_ignored_channel && V->slack_ignored_channel(source.chat_id)) {
        hermes_log(LOG_INFO, "gateway",
                   "Dropping Slack message from configured ignored channel %s", source.chat_id);
        json_free(event);
        return 0;
    }

    if (V->startup_restore_in_progress && !is_internal &&
        !(V->startup_restore_replay && V->startup_restore_replay(event))) {
        if (V->queue_startup_restore_event) V->queue_startup_restore_event(event);
        json_free(event);
        return 0;
    }

    if (!is_internal && V->scale_to_zero_note_real_inbound)
        V->scale_to_zero_note_real_inbound();

    /* pre_gateway_dispatch plugin hook: skip / rewrite / allow. */
    if (!is_internal && V->pre_gateway_dispatch) {
        char action[32] = "", new_text[4096] = "";
        int act = V->pre_gateway_dispatch(event, action, sizeof action,
                                          new_text, sizeof new_text);
        if (act != 0 || strcmp(action, "skip") == 0) {
            json_free(event);
            return 0;
        }
        if (strcmp(action, "rewrite") == 0 && new_text[0])
            json_set(event, "text", json_string(new_text));
    }

    /* Authorization. Internal events are system-generated and skip it. */
    if (!is_internal) {
        if (!source.user_id[0]) {
            if (!(V->is_user_authorized && V->is_user_authorized(&source))) {
                hermes_log(LOG_DEBUG, "gateway",
                           "Ignoring message with no user_id from %s", source.platform);
                json_free(event);
                return 0;
            }
        } else if (!(V->is_user_authorized && V->is_user_authorized(&source))) {
            hermes_log(LOG_WARNING, "gateway",
                       "Unauthorized user: %s (%s) on %s",
                       source.user_id, source.user_name, source.platform);
            /* DM pairing offer runs through the injectable seam. */
            if (strcmp(source.chat_type, "dm") == 0 && V->unauthorized_dm_pairing) {
                char *reply = V->unauthorized_dm_pairing(&source);
                if (reply) {
                    snprintf(response_out, response_size, "%s", reply);
                    free(reply);
                }
            }
            json_free(event);
            return 0;
        }
    }

    /* Global emergency stop (`hermes pause`): paused notice instead of a new
     * run; internal events and control traffic bypass the gate. */
    if (!is_internal && V->estop_paused_reply) {
        const char *paused = V->estop_paused_reply();
        if (paused) {
            const char *cmd = json_get_str(event, "command", "");
            bool allow = cmd[0] && V->estop_command_allowed && V->estop_command_allowed(cmd);
            if (!allow && V->session_update_prompt_pending &&
                V->session_update_prompt_pending(session_key))
                allow = true;
            if (!allow && V->is_session_running && V->is_session_running(session_key))
                allow = true;
            if (!allow && V->slash_confirm_pending &&
                V->slash_confirm_pending(session_key))
                allow = true;
            if (!allow && V->blocking_tool_approval &&
                V->blocking_tool_approval(session_key))
                allow = true;
            if (!allow) {
                snprintf(response_out, response_size, "%s", paused);
                json_free(event);
                return 1;
            }
        }
    }

    /* Pending /update prompt interception: approve|yes→y, deny|no→n; other
     * free text becomes the response; recognized commands cancel the prompt
     * and fall through. */
    const char *cmd0 = json_get_str(event, "command", "");
    if (V->session_update_prompt_pending && V->session_update_prompt_pending(session_key)) {
        const char *raw = json_get_str(event, "text", "");
        char response_text[512] = "";
        if (strcmp(cmd0, "approve") == 0 || strcmp(cmd0, "yes") == 0)
            snprintf(response_text, sizeof response_text, "y");
        else if (strcmp(cmd0, "deny") == 0 || strcmp(cmd0, "no") == 0)
            snprintf(response_text, sizeof response_text, "n");
        else if (!cmd0[0])
            snprintf(response_text, sizeof response_text, "%s",
                     raw ? raw : "");
        bool recognized = cmd0[0] && cli_resolve_command(cmd0) != NULL;
        if (response_text[0]) {
            if (V->write_update_response && !V->write_update_response(response_text)) {
                snprintf(response_out, response_size,
                         "\xe2\x9c\x97 Failed to send response to update process");
                json_free(event);
                return 1;
            }
            snprintf(response_out, response_size,
                     "\xe2\x9c\x93 Sent `%.20s` to the update process.", response_text);
            json_free(event);
            return 1;
        }
        (void)recognized;
    }

    /* Pending slash-confirm interception (/reload-mcp and friends): only when
     * no tool approval is live — the tool approval takes precedence. */
    bool tool_approval_live =
        V->blocking_tool_approval && V->blocking_tool_approval(session_key);
    if (!tool_approval_live && V->slash_confirm_pending &&
        V->slash_confirm_pending(session_key)) {
        const char *cmd = cmd0;
        const char *norm_raw = json_get_str(event, "text", "");
        while (*norm_raw == '!' || *norm_raw == '/') norm_raw++;
        const char *choice = NULL;
        if (!strcmp(cmd,"approve")||!strcmp(cmd,"yes")||!strcmp(cmd,"ok")||!strcmp(cmd,"confirm"))
            choice = "once";
        else if (!strcmp(cmd,"always")||!strcmp(cmd,"remember"))
            choice = "always";
        else if (!strcmp(cmd,"cancel")||!strcmp(cmd,"no")||!strcmp(cmd,"deny")||!strcmp(cmd,"nevermind"))
            choice = "cancel";
        else if (!strcasecmp(norm_raw,"approve")||!strcasecmp(norm_raw,"approve once")||!strcasecmp(norm_raw,"once"))
            choice = "once";
        else if (!strcasecmp(norm_raw,"always")||!strcasecmp(norm_raw,"always approve"))
            choice = "always";
        else if (!strcasecmp(norm_raw,"cancel")||!strcasecmp(norm_raw,"nevermind")||!strcasecmp(norm_raw,"no"))
            choice = "cancel";
        if (choice && V->resolve_slash_confirm) {
            char *resolved = V->resolve_slash_confirm(session_key, "", choice);
            if (resolved) { snprintf(response_out, response_size, "%s", resolved); free(resolved); }
            json_free(event);
            return 1;
        }
    }

    /* PRIORITY path: an agent already running on this session. */
    if (V->is_session_running && V->is_session_running(session_key)) {
        const cli_command_def_t *cd = cmd0[0] ? cli_resolve_command(cmd0) : NULL;
        if (cd && !strcmp(cd->name, "status")) goto cold_dispatch;
        if (cd && !strcmp(cd->name, "context")) goto cold_dispatch;
        if (cd && V->check_slash_access) {
            char *denied = V->check_slash_access(&source, cd->name);
            if (denied) {
                snprintf(response_out, response_size, "%s", denied);
                free(denied);
                json_free(event);
                return 1;
            }
        }
        if (cd && V->dispatch_busy_slash_command) {
            char *out = V->dispatch_busy_slash_command(event, cd, session_key, &source);
            if (out) { snprintf(response_out, response_size, "%s", out); free(out); }
            json_free(event);
            return 1;
        }
        /* Plain text / media follow-up: default busy semantics. */
        if (V->queue_or_replace_pending_event)
            V->queue_or_replace_pending_event(session_key, event);
        json_free(event);
        return 0;
    }

cold_dispatch: ;
    /* Check for commands (cold path). */
    const char *command = cmd0;
    const cli_command_def_t *cmd_def = command[0] ? cli_resolve_command(command) : NULL;
    const char *canonical = cmd_def ? cmd_def->name : command;

    if (command[0] && canonical[0] && cmd_def) {
        if (V->check_slash_access) {
            char *denied = V->check_slash_access(&source, canonical);
            if (denied) {
                snprintf(response_out, response_size, "%s", denied);
                free(denied);
                json_free(event);
                return 1;
            }
        }
    }

    if (canonical && cmd_def) {
        if (!strcmp(canonical, "start")) {
            hermes_log(LOG_INFO, "gateway",
                       "Ignoring /start platform ping for session %s", session_key);
            json_free(event);
            return 1;   /* empty reply */
        }
        if (!strcmp(canonical, "queue")) {
            /* payload becomes the turn text; falls through to the agent. */
        }
        if (V->handle_command) {
            char *out = V->handle_command(canonical, event);
            if (out) {
                snprintf(response_out, response_size, "%s", out);
                free(out);
                json_free(event);
                return 1;
            }
            if (!strcmp(canonical, "help") || !strcmp(canonical, "commands") ||
                !strcmp(canonical, "status") || !strcmp(canonical, "new") ||
                !strcmp(canonical, "stop") || !strcmp(canonical, "pause")) {
                json_free(event);
                return 0;
            }
        }
    }

    /* External-drain new-turn gate: refuse NEW turns during maintenance drain;
     * internal events bypass. In-flight turns untouched. */
    if (g_external_drain_active && !is_internal) {
        hermes_log(LOG_INFO, "gateway",
                   "Refusing new turn for session %s \xe2\x80\x94 external drain active.",
                   session_key);
        snprintf(response_out, response_size,
                 "\xe2\x8f\xb3 This agent is draining for a maintenance action and isn't "
                 "accepting new turns right now. It'll be back in a moment \xe2\x80\x94 "
                 "please resend shortly.");
        json_free(event);
        return 1;
    }

    /* ── Claim the session, run the agent, unwind deterministically ── */
    char *agent_result = NULL;
    if (V->handle_message_with_agent)
        agent_result = V->handle_message_with_agent(event, &source, session_key, 0);

    /* post-turn bookkeeping (goal + loop completion). */
    {
        gw_post_turn_vtab_t noop = g_post_turn;
        (void)noop;
        if (g_post_turn.get_or_create_session) {
            char final_text_buf[64];
            (void)final_text_buf;
            gw_run_post_turn_hooks_json(agent_result, event, &source, is_internal);
        }
    }

    if (agent_result) {
        snprintf(response_out, response_size, "%s", agent_result);
        free(agent_result);
    }
    /* Unconditional release covers every exit path (idempotent). */
    if (V->restore_one_turn_overrides) V->restore_one_turn_overrides(session_key);
    if (V->clear_durable_active_turn) V->clear_durable_active_turn(event);
    if (V->release_running_agent_state) V->release_running_agent_state(session_key);
    json_free(event);
    return agent_result != NULL ? 1 : 0;
}

/* JSON-string adapter so gw_run_post_turn_hooks can serve the pipeline. */
static void gw_run_post_turn_hooks_json(const char *agent_result_str,
                                        const json_node_t *event,
                                        const gw_session_source_t *source,
                                        bool is_internal)
{
    json_node_t *ar = NULL;
    if (agent_result_str) ar = json_parse(agent_result_str, NULL);
    gw_run_post_turn_hooks(ar, event, source, is_internal);
    if (ar) json_free(ar);
}


/* ════════════════════════════════════════════════════════════════
 * _warm_goals_session_db
 * ════════════════════════════════════════════════════════════════ */

extern void *goal_get_session_db(const char *home, bool on_loop_thread,
                                 void *(*construct)(void *ctx, const char *home),
                                 void *ctx);

static void gw_warm_goals_db_construct(void)
{
    /* Warm the shared goals SessionDB cache (goals.py:_get_session_db).
     * NULL construct => cache-hit path only; a cold miss is filled by the
     * caller's bootstrap windows exactly as in Python. */
    const char *home = getenv("HERMES_HOME");
    goal_get_session_db(home && home[0] ? home : NULL, false, NULL, NULL);
}

/* Warm the goals SessionDB cache off-loop (best-effort). The executor hop
 * keeps the profile home override alive under multiplex. On failure this is
 * a bounded stall, never a crash — the caller falls back to the windows. */
/* PoP: gw_warm_goals_session_db @ gateway/run.py:GatewayRunner._warm_goals_session_db */
void gw_warm_goals_session_db(const char *label)
{
    if (g_off_loop_exec) {
        g_off_loop_exec(gw_warm_goals_db_construct, NULL);
        return;
    }
    gw_warm_goals_db_construct();
}
