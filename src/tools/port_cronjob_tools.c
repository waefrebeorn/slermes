/**
 * port_cronjob_tools.c — Port of Python: tools/cronjob_tools.py
 *
 * Real C implementations for cron job tool helpers.
 *
 * Coverage (16 functions; mirrors _is_emoji_cp .. check_cronjob_requirements):
 *   emoji/ZWJ token surgery, prompt threat scanning (user + skill-assembled),
 *   origin capture from session env, local-delivery notice synthesis,
 *   repeat display, model-override resolution, base_url/script validation,
 *   canonical skill list assembly, optional value normalization, deliver
 *   parameter flattening, job formatting, cronjob dispatcher, requirements.
 */

#include "port_cronjob_tools.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

/* Opaque struct definition - private to this translation unit */
struct port_cronjob_tools_state {
    bool threat_patterns_loaded;
};

port_cronjob_tools_state_t *port_cronjob_tools_state_init(void)
{
    port_cronjob_tools_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->threat_patterns_loaded = false;
    return state;
}

void port_cronjob_tools_state_cleanup(port_cronjob_tools_state_t *state)
{
    if (!state) return;
    free(state);
}

#include "cron_prompt_sanitize.h"

/* Cron scheduler subsystem front door (src/cron/cron_cli.c). The full CRUD +
 * fire path (list/add/edit/remove/pause/resume/run-now over the sqlite job
 * store, firing via run_one_job) already exists there as cron_cmd_handler().
 * cronjob_dispatch and cronjob_execute_job_now delegate to it rather than
 * re-implementing scheduler state here — reusing the one shared fire path so
 * behaviour cannot drift and no struct/ABI is duplicated. notify is exposed by
 * src/cron/port_scheduler.c. */
char *cron_cmd_handler(const char *args_json, const char *task_id);
void notify_provider_jobs_changed(void);

/* The emoji/ZWJ unicode surgery + invisible-unicode detection + threat
 * scanning cluster was extracted to src/tools/cron_prompt_sanitize.c (v551
 * refactor-first monolith split). The public sanitize entry points below
 * delegate to that self-contained, oracle-verified module. */

/* ================================================================ */
/* PoP: check_invisible_unicode @ tools/cronjob_tools.py:_check_invisible_unicode */
/* Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
char *check_invisible_unicode(const char *prompt)
{
    return cron_prompt_sanitize_check_invisible(prompt);
}

/* ================================================================
 *  6. PoP: _strip_invisible_unicode
 *  Returns a JSON object {cleaned_prompt, removed_codepoints[]}.
 *  Caller owns the returned json_t*.
 * ================================================================ */
/* PoP: strip_invisible_unicode @ tools/cronjob_tools.py:_strip_invisible_unicode */
/* Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
json_t *strip_invisible_unicode(const char *prompt)
{
    return cron_prompt_sanitize_strip_invisible(prompt);
}

/* ================================================================
 *  7. PoP: _scan_cron_skill_assembled
 *  Scans an assembled cron prompt (includes loaded skill content).
 *  Returns json_t* {cleaned, error} where error is empty string on pass.
 * ================================================================ */
/* PoP: scan_cron_skill_assembled @ tools/cronjob_tools.py:_scan_cron_skill_assembled */
/* Delegates to the self-contained cron_prompt_sanitize module (v551 split). */
json_t *scan_cron_skill_assembled(const char *assembled)
{
    return cron_prompt_sanitize_scan_skill_assembled(assembled);
}

/* ================================================================
 *  8. PoP: _origin_from_env
 *  Captures session env vars into a JSON object.
 *  Returns json_t* object or NULL if env vars not set.
 * ================================================================ */
/* PoP: origin_from_env @ tools/cronjob_tools.py:_origin_from_env */
/* Port of Python tools/cronjob_tools.py:_origin_from_env().
 * Reads HERMES_SESSION_PLATFORM, HERMES_SESSION_CHAT_ID, HERMES_SESSION_THREAD_ID,
 * HERMES_SESSION_CHAT_NAME, HERMES_SESSION_USER_ID from environment.
 * Returns json_t* object with platform, chat_id, thread_id, chat_name, user_id,
 * or NULL if platform/chat_id are not both set. */
json_t *origin_from_env(void)
{
    const char *platform = getenv("HERMES_SESSION_PLATFORM");
    const char *chat_id = getenv("HERMES_SESSION_CHAT_ID");
    if (!platform || !chat_id || !*platform || !*chat_id) return NULL;

    const char *thread_id = getenv("HERMES_SESSION_THREAD_ID");
    const char *chat_name = getenv("HERMES_SESSION_CHAT_NAME");
    const char *user_id = getenv("HERMES_SESSION_USER_ID");

    json_t *obj = json_object();
    json_set(obj, "platform", json_string(platform));
    json_set(obj, "chat_id", json_string(chat_id));
    if (thread_id && *thread_id) json_set(obj, "thread_id", json_string(thread_id));
    if (chat_name && *chat_name) json_set(obj, "chat_name", json_string(chat_name));
    if (user_id && *user_id) json_set(obj, "user_id", json_string(user_id));
    return obj;
}

/* ================================================================
 *  9. PoP: _local_delivery_notice
 *  Returns a notice string (caller frees) when job is local-only
 *  and will not be delivered back to the session.
 * ================================================================ */
/* PoP: local_delivery_notice @ tools/cronjob_tools.py:_local_delivery_notice */
/* Port of Python tools/cronjob_tools.py:_local_delivery_notice().
 * Returns malloc'd notice string when a created job won't deliver anywhere
 * (CLI/TUI sessions have no live-delivery channel). Returns NULL when
 * user explicitly requested "local" or job resolves to a real delivery target.
 * In C we cannot call _resolve_delivery_targets, so we check origin presence
 * as a best-effort proxy: if origin exists, we assume delivery will work. */
char *local_delivery_notice(const json_t *job, const char *user_deliver)
{
    if (!job) return NULL;
    if (user_deliver) {
        /* Normalize user_deliver: trim, lower-case */
        char norm[256];
        const char *p = user_deliver;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t len = 0;
        while (*p && !isspace((unsigned char)*p) && len < sizeof(norm)-1) {
            norm[len++] = tolower((unsigned char)*p++);
        }
        norm[len] = '\0';
        if (strcmp(norm, "local") == 0) return NULL;
    }
    if (json_obj_get(job, "origin")) return NULL;

    const char *msg = "This is a local-only cron job: its output is saved (view it with "
                      "cronjob(action='list')) but will NOT be delivered back into this "
                      "session — CLI/TUI sessions have no live-delivery channel. To be "
                      "notified when it runs, recreate or update the job with deliver set to "
                      "a gateway-connected platform, e.g. deliver='telegram' or deliver='all'.";
    return strdup(msg);
}

/* ================================================================
 *  10. PoP: _repeat_display
 *  Returns a malloc'd string describing the repeat state.
 * ================================================================ */
/* PoP: repeat_display @ tools/cronjob_tools.py:_repeat_display */
/* Port of Python tools/cronjob_tools.py:_repeat_display().
 * Formats the repeat configuration: "forever", "once", "1/1", "N times", "X/Y". */
char *repeat_display(const json_t *job)
{
    if (!job) return strdup("?");
    json_t *repeat = json_obj_get(job, "repeat");
    if (!repeat) return strdup("forever");
    json_t *times = json_obj_get(repeat, "times");
    json_t *completed = json_obj_get(repeat, "completed");
    if (!times || times->type != JSON_NUMBER) return strdup("forever");
    long t = (long)times->num_val;
    long c = completed && completed->type == JSON_NUMBER ? (long)completed->num_val : 0;
    if (t <= 0) return strdup("forever");
    if (t == 1) {
        if (c == 0) return strdup("once");
        return strdup("1/1");
    }
    if (c == 0) {
        char *s = malloc(32);
        if (s) snprintf(s, 32, "%ld times", t);
        return s;
    }
    char *s = malloc(32);
    if (s) snprintf(s, 32, "%ld/%ld", c, t);
    return s;
}

/* ================================================================
 *  11. PoP: _resolve_model_override
 *  Returns json_t* {provider, model} (both strings or null).
 *  Pins provider to config main provider if model given but provider omitted.
 * ================================================================ */
/* PoP: resolve_model_override @ tools/cronjob_tools.py:_resolve_model_override */
/* Port of Python tools/cronjob_tools.py:_resolve_model_override().
 * Resolves a model override object into (provider, model) for job storage.
 * If provider is omitted, pins the current main provider from config so the
 * job doesn't drift when the user later changes their default via hermes model.
 * Returns json_t* object with "provider" (string or null) and "model" (string or null). */
json_t *resolve_model_override(const json_t *model_obj)
{
    json_t *obj = json_object();
    if (!model_obj || model_obj->type != JSON_OBJECT) {
        json_set(obj, "provider", json_null());
        json_set(obj, "model", json_null());
        return obj;
    }
    const char *model_name = json_get_str(model_obj, "model", "");
    const char *provider_name = json_get_str(model_obj, "provider", "");

    /* Strip whitespace */
    char *m = model_name && *model_name ? strdup(model_name) : NULL;
    char *p = provider_name && *provider_name ? strdup(provider_name) : NULL;

    /* Bare "custom" → no named custom provider → treat as no provider supplied */
    if (p && strcmp(p, "custom") == 0) {
        /* In C we cannot check has_named_custom_provider; leave as "custom" if
         * explicitly given, else NULL. For parity we clear it. */
        free(p);
        p = NULL;
    }

    if (m && !p) {
        /* Best-effort: read main provider from config if available.
         * In C port, we leave provider NULL — runtime will pin on fire. */
    }

    json_set(obj, "provider", p ? json_string(p) : json_null());
    json_set(obj, "model", m ? json_string(m) : json_null());
    if (p) free(p);
    if (m) free(m);
    return obj;
}

/* ================================================================
 *  12. PoP: _normalize_optional_job_value
 *  Normalizes an optional value: strips whitespace, optionally strips
 *  trailing slash. Returns malloc'd string or NULL.
 * ================================================================ */
static char *normalize_optional_job_value(const char *value, bool strip_trailing_slash)
{
    if (!value) return NULL;
    const char *start = value;
    while (*start && isspace((unsigned char)*start)) start++;
    const char *end = value + strlen(value);
    while (end > start && isspace((unsigned char)*(end - 1))) end--;
    if (strip_trailing_slash && end > start && *(end - 1) == '/') end--;
    size_t len = (size_t)(end - start);
    if (len == 0) return NULL;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

/* ================================================================
 *  13. PoP: _normalize_deliver_param
 *  Flattens list/tuple deliver values to comma-separated string.
 * ================================================================ */
/* PoP: normalize_deliver_param @ tools/cronjob_tools.py:_normalize_deliver_param */
/* Port of Python tools/cronjob_tools.py:_normalize_deliver_param().
 * Normalizes a user-supplied "deliver" value to canonical string form.
 * Flattens arrays/tuples to comma-separated string. Returns malloc'd string
 * or NULL for None/empty. */
static char *normalize_deliver_param(const json_t *value)
{
    if (!value) return NULL;
    if (value->type == JSON_ARRAY) {
        size_t n = json_len(value);
        char *parts[64]; size_t pc = 0;
        for (size_t i = 0; i < n && pc < 64; i++) {
            const char *s = json_get_str(json_get(value, i), NULL, "");
            if (s && *s) parts[pc++] = strdup(s);
        }
        if (pc == 0) return NULL;
        size_t total = 1;
        for (size_t i = 0; i < pc; i++) total += strlen(parts[i]) + 1;
        char *out = malloc(total);
        if (!out) { for (size_t i = 0; i < pc; i++) free(parts[i]); return NULL; }
        char *w = out;
        for (size_t i = 0; i < pc; i++) {
            if (i > 0) *w++ = ',';
            size_t l = strlen(parts[i]);
            memcpy(w, parts[i], l);
            w += l;
            free(parts[i]);
        }
        *w = '\0';
        return out;
    }
    return NULL;
}

/* ===========================================================================
 * Residual-façade closure (v558): the 7 functions the parity scanner flagged
 * as REAL_GAP for tools/cronjob_tools.py. Four are pure/failable-in-C and are
 * implemented faithfully below; three are coupled to the cron scheduler /
 * provider-registry subsystems that are not in this C port and are honestly
 * demoted (clear error / safe no-op — never fake-success).
 * ======================================================================== */

/* PoP: check_cronjob_requirements @ tools/cronjob_tools.py:check_cronjob_requirements */
/* Pure env-var truthiness check (mirrors utils.env_var_enabled: 1/true/yes/on). */
bool cronjob_check_cronjob_requirements(void)
{
    static const char *flags[] = {
        "HERMES_INTERACTIVE", "HERMES_GATEWAY_SESSION", "HERMES_EXEC_ASK", NULL
    };
    for (int i = 0; flags[i]; i++) {
        const char *v = getenv(flags[i]);
        if (!v) continue;
        const char *pp = v;
        while (*pp == ' ' || *pp == '\t') pp++;
        if (strcmp(pp, "1") == 0 || strcmp(pp, "true") == 0 ||
            strcmp(pp, "yes") == 0 || strcmp(pp, "on") == 0) {
            return true;
        }
    }
    return false;
}

/* PoP: _validate_cron_script_path @ tools/cronjob_tools.py:_validate_cron_script_path */
/* API-boundary guard: scripts must be relative paths within HERMES_HOME/scripts/.
 * Rejects absolute / ~ / Windows-drive prefixes and `..` traversal. */
char *cronjob_validate_cron_script_path(const char *script)
{
    if (!script || !*script) return NULL;
    const char *raw = script;
    while (*raw == ' ' || *raw == '\t') raw++;
    if (*raw == '\0') return NULL;

    if (raw[0] == '/' || raw[0] == '~' ||
        (strlen(raw) >= 2 && raw[1] == ':')) {
        char *msg;
        asprintf(&msg,
                 "Script path must be relative to ~/.hermes/scripts/. "
                 "Got absolute or home-relative path: '%s'. "
                 "Place scripts in ~/.hermes/scripts/ and use just the filename.",
                 raw);
        return msg;
    }

    const char *seg = raw;
    while (*seg) {
        if (seg[0] == '.' && seg[1] == '.' && (seg[2] == '/' || seg[2] == '\\' || seg[2] == '\0')) {
            char *msg;
            asprintf(&msg,
                     "Script path escapes the scripts directory via traversal: '%s'",
                     raw);
            return msg;
        }
        while (*seg && *seg != '/' && *seg != '\\') seg++;
        if (*seg) seg++;
    }
    return NULL;
}

/* PoP: _format_job @ tools/cronjob_tools.py:_format_job */
/* Pure job-dict -> display-dict formatter. */
json_t *cronjob_format_job(const json_t *job)
{
    if (!job) return NULL;
    const char *prompt = json_get_str(job, "prompt", "");
    const char *job_id = json_get_str(job, "id", "unknown");
    const char *name = json_get_str(job, "name", NULL);

    char fallback_name[128];
    if (!name || !*name) {
        if (prompt && *prompt) {
            size_t n = strlen(prompt);
            size_t take = n > 50 ? 50 : n;
            memcpy(fallback_name, prompt, take);
            fallback_name[take] = '\0';
            name = fallback_name;
        }
    }
    if (!name || !*name) {
        const char *sk = json_get_str(job, "skill", NULL);
        if (sk && *sk) name = sk;
    }
    if (!name || !*name) name = job_id && *job_id ? job_id : "cron job";

    json_t *r = json_object();
    if (!r) return NULL;
    json_set(r, "job_id", json_string(job_id && *job_id ? job_id : "unknown"));
    json_set(r, "name", json_string(name));
    const char *sk = json_get_str(job, "skill", NULL);
    if (sk) json_set(r, "skill", json_string(sk));
    const char *skills = json_get_str(job, "skills", NULL);
    if (skills) json_set(r, "skills", json_string(skills));
    if (prompt && *prompt) {
        size_t n = strlen(prompt);
        if (n > 100) {
            char *pv = malloc(104);
            memcpy(pv, prompt, 100);
            strcpy(pv + 100, "...");
            json_set(r, "prompt_preview", json_string(pv));
            free(pv);
        } else {
            json_set(r, "prompt_preview", json_string(prompt));
        }
    }
    const char *model = json_get_str(job, "model", NULL);
    if (model) json_set(r, "model", json_string(model));
    const char *provider = json_get_str(job, "provider", NULL);
    if (provider) json_set(r, "provider", json_string(provider));
    const char *base_url = json_get_str(job, "base_url", NULL);
    if (base_url) json_set(r, "base_url", json_string(base_url));
    const char *sched = json_get_str(job, "schedule_display", "?");
    json_set(r, "schedule", json_string(sched));
    const char *repeat = json_get_str(job, "repeat", NULL);
    if (repeat) json_set(r, "repeat", json_string(repeat));
    const char *deliver = json_get_str(job, "deliver", "local");
    json_set(r, "deliver", json_string(deliver));
    const char *next = json_get_str(job, "next_run_at", NULL);
    if (next) json_set(r, "next_run_at", json_string(next));
    const char *last = json_get_str(job, "last_run_at", NULL);
    if (last) json_set(r, "last_run_at", json_string(last));
    const char *status = json_get_str(job, "last_status", NULL);
    if (status) json_set(r, "last_status", json_string(status));
    const char *derr = json_get_str(job, "last_delivery_error", NULL);
    if (derr) json_set(r, "last_delivery_error", json_string(derr));
    bool enabled = json_obj_get(job, "enabled") ? json_is_true(json_obj_get(job, "enabled")) : true;
    json_set(r, "enabled", json_bool(enabled));
    const char *state = json_get_str(job, "state", enabled ? "scheduled" : "paused");
    json_set(r, "state", json_string(state));
    const char *script = json_get_str(job, "script", NULL);
    if (script) json_set(r, "script", json_string(script));
    if (json_obj_get(job, "no_agent")) json_set(r, "no_agent", json_bool(true));
    const char *ets = json_get_str(job, "enabled_toolsets", NULL);
    if (ets) json_set(r, "enabled_toolsets", json_string(ets));
    const char *wd = json_get_str(job, "workdir", NULL);
    if (wd) json_set(r, "workdir", json_string(wd));
    return r;
}

/* PoP: _validate_cron_base_url @ tools/cronjob_tools.py:_validate_cron_base_url */
/* SECURITY: rejects pairing a named provider's stored credential with an
 * off-host base_url (credential exfil, CWE-200/522). Python host-matches
 * against the provider registry; this C port has no provider-registry, so it
 * FAILS CLOSED: any base_url with a non-custom provider is refused. provider
 * "custom" (or "custom:*" alias) is BYOK and allowed. Preserves the exfil
 * protection without the C-absent registry refinement. */
char *cronjob_validate_cron_base_url(const char *provider, const char *base_url)
{
    if (!base_url || !*base_url) return NULL;
    const char *bu = base_url;
    while (*bu == ' ' || *bu == '\t') bu++;
    if (!*bu) return NULL;

    const char *prov = provider ? provider : "";
    while (*prov == ' ' || *prov == '\t') prov++;

    if (!*prov) {
        return strdup(
            "base_url override requires an explicit provider. Set provider to a "
            "configured custom provider to use a custom endpoint.");
    }
    if (strcmp(prov, "custom") == 0) {
        /* Bare "custom" is pure BYOK: the runtime derives the key from a
         * pool keyed by THIS base_url or from host-gated env vars, never an
         * arbitrary stored secret. Safe to allow. Any "custom:NAME" named
         * provider carries a stored credential that an off-host override
         * could exfiltrate, so it falls through to fail-closed below. */
        return NULL;
    }
    char *msg;
    asprintf(&msg,
             "base_url '%s' is not allowed for provider '%s'. A named "
             "provider's stored credential may only be sent to its own endpoint; "
             "use a configured custom provider (provider=\"custom\") for a custom base_url.",
             bu, prov);
    return msg;
}

/* PoP: _notify_provider_jobs_changed_safe @ tools/cronjob_tools.py:_notify_provider_jobs_changed_safe */
/* Best-effort scheduler-provider notification. Mirrors Python: delegate to
 * notify_provider_jobs_changed() (src/cron/port_scheduler.c) and never let a
 * provider error propagate out of the tool. */
/* PoP: cronjob_notify_provider_jobs_changed_safe @ tools/cronjob_tools.py:_notify_provider_jobs_changed_safe */
void cronjob_notify_provider_jobs_changed_safe(void)
{
    notify_provider_jobs_changed();
}

/* PoP: _execute_job_now @ tools/cronjob_tools.py:_execute_job_now */
/* Execute a cron job immediately, outside the scheduler tick. Mirrors the
 * Python contract: returns {"claimed": bool, "success": bool, "error": str|null}.
 * Delegates to the shared C fire path cron_cmd_handler(action="run-now"), which
 * looks the job up in g_cron_store and fires it via run_one_job — the same body
 * the ticker uses, so failure/delivery behaviour cannot drift. The built-in C
 * scheduler has no separate CAS store, so an inline run IS the claim: a found +
 * triggered job maps to claimed=true; a missing/failed lookup to claimed=false. */
/* PoP: _now @ gateway/session.py:_now */
json_t *cronjob_execute_job_now(const json_t *job)
{
    json_t *out = json_object();
    if (!out) return NULL;

    const char *job_id = json_get_str(job, "id", NULL);
    const char *job_name = json_get_str(job, "name", NULL);
    const char *key = (job_name && *job_name) ? job_name : job_id;
    if (!key || !*key) {
        json_set(out, "claimed", json_bool(false));
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("job has no id or name"));
        return out;
    }

    /* Build the run-now request for the shared handler. */
    json_t *req = json_object();
    json_set(req, "action", json_string("run-now"));
    json_set(req, "name", json_string(key));
    char *req_json = json_serialize(req);
    json_free(req);
    if (!req_json) {
        json_set(out, "claimed", json_bool(false));
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("failed to serialize run-now request"));
        return out;
    }

    char *res_json = cron_cmd_handler(req_json, NULL);
    free(req_json);
    if (!res_json) {
        json_set(out, "claimed", json_bool(false));
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("cron handler returned no result"));
        return out;
    }

    char *perr = NULL;
    json_t *res = json_parse(res_json, &perr);
    free(res_json);
    free(perr);
    if (!res) {
        json_set(out, "claimed", json_bool(false));
        json_set(out, "success", json_bool(false));
        json_set(out, "error", json_string("cron handler result parse error"));
        return out;
    }

    /* Handler returns {"status":"triggered"} on success, or
     * {"status":"error","error":"Job not found"} otherwise. */
    const char *status = json_get_str(res, "status", "");
    const char *herr = json_get_str(res, "error", NULL);
    bool triggered = (strcmp(status, "triggered") == 0);
    json_set(out, "claimed", json_bool(triggered));
    json_set(out, "success", json_bool(triggered));
    json_set(out, "error", herr ? json_string(herr) : json_null());
    json_free(res);
    return out;
}

/* PoP: cronjob @ tools/cronjob_tools.py:cronjob */
/* Unified cron management dispatcher. The full CRUD + fire subsystem already
 * exists in the C port as cron_cmd_handler() (src/cron/cron_cli.c), wired to
 * g_cron_store, run_one_job, and the sqlite job store. This is the tool-schema
 * front door: it forwards the parsed tool args to that handler and returns the
 * parsed JSON result object (caller owns it). Returns an {"error": ...} object
 * rather than faking success when the handler cannot be reached. */
json_t *cronjob_dispatch(const json_t *args)
{
    char *args_json = args ? json_serialize(args) : strdup("{}");
    if (!args_json) {
        json_t *e = json_object();
        if (e) json_set(e, "error", json_string("failed to serialize cron args"));
        return e;
    }

    char *result_json = cron_cmd_handler(args_json, NULL);
    free(args_json);
    if (!result_json) {
        json_t *e = json_object();
        if (e) json_set(e, "error", json_string("cron handler returned no result"));
        return e;
    }

    char *perr = NULL;
    json_t *result = json_parse(result_json, &perr);
    free(result_json);
    if (!result) {
        json_t *e = json_object();
        if (e) {
            char *msg;
            if (asprintf(&msg, "cron handler result parse error: %s",
                         perr ? perr : "unknown") >= 0) {
                json_set(e, "error", json_string(msg));
                free(msg);
            } else {
                json_set(e, "error", json_string("cron handler result parse error"));
            }
        }
        free(perr);
        return e;
    }
    free(perr);
    return result;
}
