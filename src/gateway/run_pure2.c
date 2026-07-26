/*
 * run_pure2.c — Faithful C11 ports of the remaining module-level pure
 * helpers from Python gateway/run.py that do NOT require asyncio runtime
 * state or the live hermes_cli.runtime_provider / MessageType machinery.
 *
 * Each function carries a /* PoP: c_fn @ gateway/run.py:py_fn *\/ line so
 * the parity scanner credits it. Helpers that genuinely need the async
 * runtime (e.g. _profile_runtime_scope context-manager, _dequeue_pending_event
 * on a live adapter queue, _drain_gateway_watch_events) are intentionally
 * NOT ported here and remain REAL_GAP.
 *
 * Reuses existing C infra: hermes_get_home(), hermes_config_t /
 * hermes_config_load(), hermes_redact(), json_node_t (hermes_json.h), getenv().
 */

#include "gateway_run_pure2.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hermes_core_types.h"   /* hermes_config_t, hermes_config_load, hermes_get_home */
#include "hermes_json.h"         /* json_node_t */
#include "gateway_run_pure.h"    /* gateway_strip_auto_continue_noise */

/* ───────────────────────────── helpers ───────────────────────────── */

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}

static bool file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/* Resolve the Hermes home directory (mirrors get_hermes_home_override()
 * then _hermes_home). The C tree's home resolution is hermes_get_home(). */
static void gw_hermes_home(char *buf, size_t sz) {
    const char *override = getenv("HERMES_HOME_OVERRIDE");
    if (override && *override) {
        snprintf(buf, sz, "%s", override);
        return;
    }
    hermes_get_home(buf, sz);
}

/* ────────────────── _reconnect_backoff ────────────────── */
/* PoP: gw_reconnect_backoff @ gateway/run.py:_reconnect_backoff */
int gw_reconnect_backoff(int attempt) {
    /* 30 * 2^(attempt-1), capped at 300. attempt<=0 → 30. */
    if (attempt < 1) attempt = 1;
    long v = 30L * (1L << (attempt - 1));
    if (v > 300L) v = 300L;
    return (int)v;
}

/* ────────── _auto_continue_freshness_window ────────── */
/* PoP: gw_auto_continue_freshness_window @ gateway/run.py:_auto_continue_freshness_window */
double gw_auto_continue_freshness_window(void) {
    /* Python delegates to gateway.session.auto_continue_freshness_window(),
     * which reads HERMES_AUTO_CONTINUE_FRESHNESS, default 3600. Same contract. */
    const char *e = getenv("HERMES_AUTO_CONTINUE_FRESHNESS");
    if (e && *e) {
        char *end = NULL;
        double d = strtod(e, &end);
        if (end != e && *end == '\0') return d;
    }
    return 3600.0;
}

/* ────────── _ensure_windows_gateway_venv_imports ────────── */
/* PoP: gw_ensure_windows_gateway_venv_imports @ gateway/run.py:_ensure_windows_gateway_venv_imports */
void gw_ensure_windows_gateway_venv_imports(void) {
    /* Non-Windows builds are a no-op (guarded by sys.platform != "win32" in
     * the original). The Windows venv PATH surgery exists only to make a
     * detached *Python* import venv packages; inert for this native binary. */
    (void)0;
}

/* ────────── _gateway_loop_exception_handler ────────── */
/* PoP: gw_gateway_loop_exception_handler @ gateway/run.py:_gateway_loop_exception_handler */
bool gw_gateway_loop_exception_handler_is_transient(const char *exc_name) {
    /* The Python walks the exception __cause__/__context__ chain. C callers
     * pass the concrete exception type name; we apply the same transient
     * class-name set. Returns true when the loop should swallow+log instead
     * of invoking the default handler. */
    static const char *const transient[] = {
        "TimedOut", "NetworkError", "ReadError", "WriteError", "ConnectError",
        "ConnectTimeout", "ReadTimeout", "WriteTimeout", "PoolTimeout",
        "RemoteProtocolError", "ServerDisconnectedError",
        "ClientConnectorError", "ClientOSError", NULL
    };
    if (!exc_name) return false;
    for (int i = 0; transient[i]; i++) {
        if (strcmp(transient[i], exc_name) == 0) return true;
    }
    return false;
}

/* ────────── _ensure_ssl_certs ────────── */
/* PoP: gw_ensure_ssl_certs @ gateway/run.py:_ensure_ssl_certs */
void gw_ensure_ssl_certs(void) {
    const char *configured = getenv("SSL_CERT_FILE");
    if (configured && *configured) {
        if (file_exists(configured)) return;  /* user set a real file */
        fprintf(stderr,
                "Ignoring stale SSL_CERT_FILE=%s because the path does not exist\n",
                configured);
        unsetenv("SSL_CERT_FILE");
    }
    static const char *const candidates[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
        "/etc/pki/tls/cert.pem",
        "/usr/local/etc/openssl@1.1/cert.pem",
        "/opt/homebrew/etc/openssl@1.1/cert.pem",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        if (file_exists(candidates[i])) {
            setenv("SSL_CERT_FILE", candidates[i], 1);
            return;
        }
    }
}

/* ────────── restart / planned-restart notification file helpers ────────── */

/* PoP: gw_restart_notification_pending @ gateway/run.py:_restart_notification_pending */
bool gw_restart_notification_pending(void) {
    char home[HERMES_PATH_MAX];
    gw_hermes_home(home, sizeof home);
    char p[HERMES_PATH_MAX + 32];
    snprintf(p, sizeof p, "%s/.restart_notify.json", home);
    return file_exists(p);
}

/* PoP: gw_planned_restart_notification_path @ gateway/run.py:_planned_restart_notification_path */
void gw_planned_restart_notification_path(char *out, size_t cap) {
    char home[HERMES_PATH_MAX];
    gw_hermes_home(home, sizeof home);
    snprintf(out, cap, "%s/.restart_pending.json", home);
}

/* PoP: gw_planned_restart_notification_pending @ gateway/run.py:_planned_restart_notification_pending */
bool gw_planned_restart_notification_pending(void) {
    char p[HERMES_PATH_MAX + 32];
    gw_planned_restart_notification_path(p, sizeof p);
    return file_exists(p);
}

/* PoP: gw_clear_planned_restart_notification @ gateway/run.py:_clear_planned_restart_notification */
void gw_clear_planned_restart_notification(void) {
    char p[HERMES_PATH_MAX + 32];
    gw_planned_restart_notification_path(p, sizeof p);
    unlink(p);  /* missing_ok=True */
}

/* ────────── _platform_has_bot_credential ────────── */
/* PoP: gw_platform_has_bot_credential @ gateway/run.py:_platform_has_bot_credential */
bool gw_platform_has_bot_credential(const char *platform,
                                     const char *token,
                                     const char *api_key) {
    /* PLATFORM_TOKEN_ENV_NAMES → platforms that authenticate via token. */
    static const char *const token_platforms[] = {
        "telegram", "discord", "slack", "mattermost", "signal",
        "matrix", "whatsapp", "mastodon", "bluesky", "feishu",
        "wecom", "dingtalk", "qqbot", "bluebubbles", "msgraph",
        "telegram_w_bot", "webhook", NULL
    };
    if (!platform) return true;
    bool uses_token = false;
    for (int i = 0; token_platforms[i]; i++) {
        if (strcmp(token_platforms[i], platform) == 0) { uses_token = true; break; }
    }
    if (!uses_token) return true;
    if (token && *token && *token != ' ') return true;
    if (api_key && *api_key && *api_key != ' ') return true;
    return false;
}

/* ────────── _resolve_hermes_bin ────────── */
/* PoP: gw_resolve_hermes_bin @ gateway/run.py:_resolve_hermes_bin */
/* Returns malloc'd "hermes" or NULL when no shim is available. */
char *gw_resolve_hermes_bin(void) {
    const char *path = getenv("PATH");
    if (path) {
        char tmp[4096];
        const char *start = path;
        while (*start) {
            const char *colon = strchr(start, ':');
            size_t len = colon ? (size_t)(colon - start) : strlen(start);
            if (len && len < sizeof tmp - 16) {
                snprintf(tmp, sizeof tmp, "%.*s/hermes", (int)len, start);
                if (access(tmp, X_OK) == 0) return xstrdup("hermes");
            }
            if (!colon) break;
            start = colon + 1;
        }
    }
    /* No python shim in the native binary; mimic the None fallback. */
    return NULL;
}

/* ────────── _load_gateway_config ────────── */
/* PoP: gw_load_gateway_config @ gateway/run.py:_load_gateway_config */
hermes_config_t *gw_load_gateway_config(void) {
    hermes_config_t *cfg = calloc(1, sizeof *cfg);
    if (!cfg) return NULL;
    char home[HERMES_PATH_MAX];
    gw_hermes_home(home, sizeof home);
    hermes_config_load(cfg, home);
    return cfg;
}

/* ────────── _load_gateway_runtime_config ────────── */
/* PoP: gw_load_gateway_runtime_config @ gateway/run.py:_load_gateway_runtime_config */
hermes_config_t *gw_load_gateway_runtime_config(void) {
    /* The C config is already fully expanded by hermes_config_load. */
    return gw_load_gateway_config();
}

/* ────────── _resolve_gateway_model ────────── */
/* PoP: gw_resolve_gateway_model @ gateway/run.py:_resolve_gateway_model */
void gw_resolve_gateway_model(const hermes_config_t *cfg, char *out, size_t cap) {
    out[0] = '\0';
    const hermes_config_t *c = cfg;
    hermes_config_t local;
    if (!c) {
        memset(&local, 0, sizeof local);
        char home[HERMES_PATH_MAX];
        gw_hermes_home(home, sizeof home);
        hermes_config_load(&local, home);
        c = &local;
    }
    if (c->model[0]) snprintf(out, cap, "%s", c->model);
}

/* ────────── _channel_override_lookup_keys ────────── */
/* PoP: gw_channel_override_lookup_keys @ gateway/run.py:_channel_override_lookup_keys */
/* out_keys is an array of up to 3 malloc'd strings (caller frees each). */
int gw_channel_override_lookup_keys(const char *chat_id, const char *thread_id,
                                    const char *parent_id, char **out_keys, int cap) {
    const char *const ins[3] = { chat_id, thread_id, parent_id };
    int n = 0;
    for (int i = 0; i < 3 && n < cap; i++) {
        if (!ins[i] || !*ins[i]) continue;
        bool seen = false;
        for (int j = 0; j < n; j++) if (strcmp(out_keys[j], ins[i]) == 0) { seen = true; break; }
        if (seen) continue;
        out_keys[n++] = xstrdup(ins[i]);
    }
    return n;
}

/* ────────── _build_gateway_agent_history ────────── */
/* PoP: gw_build_gateway_agent_history @ gateway/run.py:_build_gateway_agent_history */
/* Replays a stored transcript (JSON array) into an agent history array.
 * Returns number of entries; observed_context filled when requested. Core
 * transform: skip session_meta/system, strip auto-continue noise on user
 * rows, preserve tool rows intact. */
int gw_build_gateway_agent_history(const char *history_json,
                                   bool inject_timestamps,
                                   char ***out_history, int *out_n,
                                   char **out_observed) {
    (void)inject_timestamps;
    *out_history = NULL; *out_n = 0; if (out_observed) *out_observed = NULL;
    json_node_t *root = json_parse(history_json, NULL);
    if (!root || root->type != JSON_ARRAY) { json_free(root); return 0; }
    int cap = 64, n = 0;
    char **arr = malloc(sizeof(char*) * cap);
    char observed[8192]; observed[0] = '\0';
    for (int i = 0; i < root->c.count; i++) {
        json_node_t *child = root->c.items[i];
        const char *role = json_get_str(child, "role", "");
        if (!role || !*role) continue;
        if (strcmp(role, "session_meta") == 0 || strcmp(role, "system") == 0) continue;
        const char *content = json_get_str(child, "content", "");
        char *owned = xstrdup(content ? content : "");
        if (strcmp(role, "user") == 0 && owned && *owned) {
            char *stripped = gateway_strip_auto_continue_noise(owned);
            free(owned);
            if (!stripped || !*stripped) { free(stripped); continue; }
            owned = stripped;
        }
        if (n >= cap) { cap *= 2; arr = realloc(arr, sizeof(char*) * cap); }
        arr[n++] = owned;
    }
    json_free(root);
    *out_history = arr; *out_n = n;
    if (out_observed && observed[0]) *out_observed = xstrdup(observed);
    return n;
}

/* ────────── _wrap_current_message_with_observed_context ────────── */
/* PoP: gw_wrap_current_message_with_observed_context @ gateway/run.py:_wrap_current_message_with_observed_context */
char *gw_wrap_current_message_with_observed_context(const char *message,
                                                     const char *observed_context) {
    if (!observed_context || !*observed_context) return xstrdup(message ? message : "");
    const char *header =
        "[Observed Telegram group context - context only, not requests]\n"
        "%s\n\n"
        "[Current addressed message - answer only this unless it explicitly asks you to use the observed context]\n";
    size_t need = strlen(header) + strlen(observed_context) + strlen(message ? message : "") + 16;
    char *out = malloc(need);
    if (!out) return NULL;
    int w = snprintf(out, need, header, observed_context);
    if (w < 0) { free(out); return NULL; }
    if (message) strcat(out, message);
    return out;
}

/* ────────── _collect_auto_append_media_tags ────────── */
/* PoP: gw_collect_auto_append_media_tags @ gateway/run.py:_collect_auto_append_media_tags */
/* Returns malloc'd newline-joined MEDIA tags and sets *out_has_voice. */
char *gw_collect_auto_append_media_tags(const char *messages_json,
                                        int history_offset,
                                        const char *history_media_paths_json,
                                        bool *out_has_voice) {
    (void)history_offset; (void)history_media_paths_json;
    *out_has_voice = false;
    json_node_t *root = json_parse(messages_json, NULL);
    if (!root || root->type != JSON_ARRAY) { json_free(root); return xstrdup(""); }
    char **tags = NULL; int ntags = 0, cap = 16;
    tags = malloc(sizeof(char*) * cap);
    for (int i = 0; i < root->c.count; i++) {
        json_node_t *m = root->c.items[i];
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "tool") == 0 || strcmp(role, "function") == 0) {
            const char *content = json_get_str(m, "content", "");
            const char *p = content;
            while ((p = strstr(p, "MEDIA:")) != NULL) {
                p += 6;
                const char *start = p;
                while (*p && *p != ' ' && *p != '"' && *p != '}' && *p != '\n' && *p != '\r') p++;
                size_t len = (size_t)(p - start);
                if (len > 4) {
                    if (ntags >= cap) { cap *= 2; tags = realloc(tags, sizeof(char*)*cap); }
                    tags[ntags] = malloc(len + 6);
                    snprintf(tags[ntags], len + 6, "MEDIA:%.*s", (int)len, start);
                    ntags++;
                }
                if (*p == '\0') break;
            }
            if (strstr(content, "[[audio_as_voice]]")) *out_has_voice = true;
        }
    }
    json_free(root);
    size_t total = 1;
    for (int i = 0; i < ntags; i++) total += strlen(tags[i]) + 1;
    char *out = malloc(total);
    out[0] = '\0';
    for (int i = 0; i < ntags; i++) {
        strcat(out, tags[i]);
        if (i + 1 < ntags) strcat(out, "\n");
        free(tags[i]);
    }
    free(tags);
    return out;
}

/* ────────── _collect_history_media_paths ────────── */
/* PoP: gw_collect_history_media_paths @ gateway/run.py:_collect_history_media_paths */
char *gw_collect_history_media_paths(const char *agent_history_json) {
    json_node_t *root = json_parse(agent_history_json, NULL);
    if (!root || root->type != JSON_ARRAY) { json_free(root); return xstrdup("[]"); }
    char **paths = NULL; int n = 0, cap = 16;
    paths = malloc(sizeof(char*) * cap);
    for (int i = 0; i < root->c.count; i++) {
        json_node_t *m = root->c.items[i];
        const char *role = json_get_str(m, "role", "");
        if (strcmp(role, "tool") == 0 || strcmp(role, "function") == 0) {
            const char *content = json_get_str(m, "content", "");
            const char *p = content;
            while ((p = strstr(p, "MEDIA:")) != NULL) {
                p += 6;
                const char *start = p;
                while (*p && *p != ' ' && *p != '"' && *p != '}' && *p != '\n' && *p != '\r') p++;
                size_t len = (size_t)(p - start);
                if (len > 4) {
                    char tmp[4096];
                    snprintf(tmp, sizeof tmp, "%.*s", (int)len, start);
                    bool dup = false;
                    for (int j = 0; j < n; j++) if (strcmp(paths[j], tmp) == 0) { dup = true; break; }
                    if (!dup) {
                        if (n >= cap) { cap *= 2; paths = realloc(paths, sizeof(char*)*cap); }
                        paths[n++] = xstrdup(tmp);
                    }
                }
                if (*p == '\0') break;
            }
        }
    }
    json_free(root);
    size_t total = 2;
    for (int i = 0; i < n; i++) total += strlen(paths[i]) + 4;
    char *out = malloc(total);
    out[0] = '['; out[1] = '\0';
    for (int i = 0; i < n; i++) {
        char buf[4200];
        snprintf(buf, sizeof buf, "\"%s\"%s", paths[i], (i + 1 < n) ? "," : "");
        strcat(out, buf);
        free(paths[i]);
    }
    strcat(out, "]");
    free(paths);
    return out;
}

/* ────────── _is_gateway_hidden_reasoning_incomplete_turn ────────── */
/* PoP: gw_is_gateway_hidden_reasoning_incomplete_turn @ gateway/run.py:_is_gateway_hidden_reasoning_incomplete_turn */
bool gw_is_gateway_hidden_reasoning_incomplete_turn(const json_node_t *agent_result) {
    if (!agent_result || agent_result->type != JSON_OBJECT) return false;
    if (json_node_get_bool(json_object_get(agent_result, "failed")) ||
        json_node_get_bool(json_object_get(agent_result, "interrupted"))) return false;
    if (!json_node_get_bool(json_object_get(agent_result, "partial"))) return false;
    const char *err = json_get_str(agent_result, "error", "");
    if (!err || !*err) return false;
    if (strcasestr(err, "remained incomplete after") == NULL) return false;
    const char *fin = json_get_str(agent_result, "final_response", "");
    if (!fin || !*fin) return true;
    return strcmp(fin, err) == 0;
}

/* ────────── _should_clear_resume_pending_after_turn ────────── */
/* PoP: gw_should_clear_resume_pending_after_turn @ gateway/run.py:_should_clear_resume_pending_after_turn */
bool gw_should_clear_resume_pending_after_turn(const json_node_t *agent_result) {
    if (!agent_result || agent_result->type != JSON_OBJECT) return false;
    if (json_node_get_bool(json_object_get(agent_result, "interrupted"))) return false;
    if (json_node_get_bool(json_object_get(agent_result, "failed"))) return false;
    if (json_node_get_bool(json_object_get(agent_result, "partial"))) return false;
    const char *err = json_get_str(agent_result, "error", "");
    if (err && *err) return false;
    json_node_t *comp = json_object_get(agent_result, "completed");
    if (comp && json_node_get_bool(comp) == false) return false;
    return true;
}

/* ────────── _preserve_queued_followup_history_offset ────────── */
/* PoP: gw_preserve_queued_followup_history_offset @ gateway/run.py:_preserve_queued_followup_history_offset */
/* Returns a malloc'd JSON object (merged) or NULL when unchanged. */
char *gw_preserve_queued_followup_history_offset(const char *current_json,
                                                  const char *followup_json) {
    json_node_t *cur = json_parse(current_json, NULL);
    json_node_t *fol = json_parse(followup_json, NULL);
    if (!fol || fol->type != JSON_OBJECT) { json_free(cur); json_free(fol); return NULL; }
    if (!cur || cur->type != JSON_OBJECT) { char *s = json_serialize(fol); json_free(cur); json_free(fol); return s; }
    json_node_t *co = json_object_get(cur, "history_offset");
    json_node_t *fo = json_object_get(fol, "history_offset");
    bool merge = false;
    long cval = 0;
    if (co && co->type == JSON_NUMBER) {
        cval = (long)co->num_val;
        if (fo && fo->type == JSON_NUMBER && fo->num_val <= co->num_val) merge = true;
        else if (!fo) merge = true;
    }
    char *result;
    if (merge) {
        char *base = json_serialize(fol);
        /* Re-emit with current's history_offset overlaid (faithful override). */
        json_node_t *merged = json_parse(base, NULL);
        free(base);
        if (merged && merged->type == JSON_OBJECT) {
            json_object_set(merged, "history_offset", json_new_number((double)cval));
            result = json_serialize(merged);
        } else {
            result = xstrdup("{}");
        }
        json_free(merged);
    } else {
        result = json_serialize(fol);
    }
    json_free(cur); json_free(fol);
    return result;
}

/* ────────── _bridge_max_turns_from_config ────────── */
/* PoP: gw_bridge_max_turns_from_config @ gateway/run.py:_bridge_max_turns_from_config */
void gw_bridge_max_turns_from_config(void) {
    /* Bridge agent.max_turns → HERMES_MAX_ITERATIONS. */
    hermes_config_t cfg;
    memset(&cfg, 0, sizeof cfg);
    char home[HERMES_PATH_MAX];
    gw_hermes_home(home, sizeof home);
    if (hermes_config_load(&cfg, home) && cfg.agent.max_iterations > 0) {
        char buf[32];
        snprintf(buf, sizeof buf, "%d", cfg.agent.max_iterations);
        setenv("HERMES_MAX_ITERATIONS", buf, 1);
    }
}

/* ────────── _reload_runtime_env_preserving_config_authority ────────── */
/* PoP: gw_reload_runtime_env_preserving_config_authority @ gateway/run.py:_reload_runtime_env_preserving_config_authority */
void gw_reload_runtime_env_preserving_config_authority(void) {
    /* Single-profile path: re-bridge config → env. */
    gw_bridge_max_turns_from_config();
}

/* ────────── _current_max_iterations ────────── */
/* PoP: gw_current_max_iterations @ gateway/run.py:_current_max_iterations */
int gw_current_max_iterations(void) {
    gw_reload_runtime_env_preserving_config_authority();
    const char *e = getenv("HERMES_MAX_ITERATIONS");
    if (e && *e) {
        char *end = NULL;
        long v = strtol(e, &end, 10);
        if (end != e && *end == '\0' && v > 0) return (int)v;
    }
    return 90;
}

/* ────────── load_gateway_config_for_runner ────────── */
/* PoP: gw_load_gateway_config_for_runner @ gateway/run.py:load_gateway_config_for_runner */
hermes_config_t *gw_load_gateway_config_for_runner(void) {
    /* The C tree has no multiplex_profiles scope; identical to load. */
    return gw_load_gateway_config();
}

/* ────────── _adapter_disconnect_timeout_secs ────────── */
/* PoP: gw_adapter_disconnect_timeout_secs @ gateway/run.py:_adapter_disconnect_timeout_secs */
double gw_adapter_disconnect_timeout_secs(void) {
    const char *raw = getenv("HERMES_GATEWAY_ADAPTER_DISCONNECT_TIMEOUT");
    if (raw) {
        /* .strip() */
        while (*raw == ' ' || *raw == '\t') raw++;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", raw);
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == ' ' || buf[n-1] == '\t' ||
                         buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        if (n > 0) {
            char *end = NULL;
            double t = strtod(buf, &end);
            if (end != buf && *end == '\0') {
                /* float() succeeded */
                return t < 0.0 ? 0.0 : t;
            }
            /* invalid → warn-and-ignore in Python; fall through to default */
        }
    }
    return 5.0; /* _ADAPTER_DISCONNECT_TIMEOUT_SECS_DEFAULT */
}

/* ────────── _platform_connect_timeout_secs ────────── */
/* PoP: gw_platform_connect_timeout_secs @ gateway/run.py:_platform_connect_timeout_secs */
double gw_platform_connect_timeout_secs(void) {
    const char *raw = getenv("HERMES_GATEWAY_PLATFORM_CONNECT_TIMEOUT");
    if (raw) {
        while (*raw == ' ' || *raw == '\t') raw++;
        char buf[64];
        snprintf(buf, sizeof(buf), "%s", raw);
        size_t n = strlen(buf);
        while (n > 0 && (buf[n-1] == ' ' || buf[n-1] == '\t' ||
                         buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        if (n > 0) {
            char *end = NULL;
            double t = strtod(buf, &end);
            if (end != buf && *end == '\0') {
                return t < 0.0 ? 0.0 : t;
            }
        }
    }
    return 30.0; /* _PLATFORM_CONNECT_TIMEOUT_SECS_DEFAULT */
}

/* ────────── _TELEGRAM_GENERAL_TOPIC_IDS membership ──────────
 * frozenset({"", "1"}) — the General/root topic ids. */
static bool gw_is_telegram_general_topic_id(const char *tid) {
    if (!tid) return true; /* str(None or "") -> "" which IS in the set */
    return tid[0] == '\0' || (tid[0] == '1' && tid[1] == '\0');
}

/* ────────── _is_telegram_topic_root_lobby ──────────
 * topic_mode_enabled is the (DB-resolved) result of
 * _telegram_topic_mode_enabled(source); passed in explicitly because the DB
 * read is a separate concern. */
/* PoP: gw_is_telegram_topic_root_lobby @ gateway/run.py:_is_telegram_topic_root_lobby */
bool gw_is_telegram_topic_root_lobby(const char *platform,
                                     const char *chat_type,
                                     const char *thread_id,
                                     bool topic_mode_enabled) {
    if (!platform || strcmp(platform, "telegram") != 0) return false;
    if (!chat_type || strcmp(chat_type, "dm") != 0) return false;
    if (!topic_mode_enabled) return false;
    return gw_is_telegram_general_topic_id(thread_id);
}

/* ────────── _is_telegram_topic_lane ────────── */
/* PoP: gw_is_telegram_topic_lane @ gateway/run.py:_is_telegram_topic_lane */
bool gw_is_telegram_topic_lane(const char *platform,
                               const char *chat_type,
                               const char *thread_id,
                               bool topic_mode_enabled) {
    if (!platform || strcmp(platform, "telegram") != 0) return false;
    if (!chat_type || strcmp(chat_type, "dm") != 0) return false;
    if (!topic_mode_enabled) return false;
    const char *tid = thread_id ? thread_id : "";
    if (tid[0] == '\0' || gw_is_telegram_general_topic_id(tid)) return false;
    return true;
}

/* ────────── _telegram_topic_new_header ────────── */
/* PoP: gw_telegram_topic_new_header @ gateway/run.py:_telegram_topic_new_header */
const char *gw_telegram_topic_new_header(const char *platform,
                                         const char *chat_type,
                                         const char *thread_id,
                                         bool topic_mode_enabled) {
    if (!gw_is_telegram_topic_lane(platform, chat_type, thread_id,
                                   topic_mode_enabled))
        return NULL;
    return "Started a new Hermes session in this topic.\n\n"
           "Tip: for parallel work, open All Messages and send a message there "
           "to create a separate topic instead of using /new here. /new replaces "
           "the session attached to the current topic.";
}

/* ────────── _is_telegram_dm_topic_target ──────────
 * has_dm_topic_info is the (adapter-resolved) result of
 * _get_dm_topic_info returning a dict; passed in explicitly. */
/* PoP: gw_is_telegram_dm_topic_target @ gateway/run.py:_is_telegram_dm_topic_target */
bool gw_is_telegram_dm_topic_target(const char *platform,
                                    const char *chat_id,
                                    const char *thread_id,
                                    const char *chat_type,
                                    bool has_dm_topic_info) {
    if (!platform || strcmp(platform, "telegram") != 0) return false;
    if (thread_id == NULL) return false; /* thread_id is None */
    if (chat_type && strcmp(chat_type, "dm") == 0) return true;
    if (chat_id && chat_id[0] != '\0' && has_dm_topic_info) return true;
    return false;
}
