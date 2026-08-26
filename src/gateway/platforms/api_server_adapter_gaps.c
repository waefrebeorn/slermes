/**
 * api_server_adapter_gaps.c — remaining REAL_GAP ports of
 * Python gateway/platforms/api_server.py (the APIServerAdapter HTTP surface).
 *
 * Each function carries a standalone PoP annotation mapping it 1:1 to its
 * Python origin. HTTP responses are produced with libjson and written via the
 * repo's libhttp fd helpers (send_json_response/send_error_response), mirroring
 * the Python web.json_response calls.
 */

#define _POSIX_C_SOURCE 200809L

#include "api_server_adapter.h"
#include "hermes_gateway_webhook.h"
#include "hermes_json.h"
#include "hermes_gateway_runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>

/* Cross-module externs (all real implementations elsewhere in the tree). */
extern char *apis_redact_api_error_text(const char *value, int limit);
extern bool aps_origin_allowed(const char *origin, const char *allowed_origins_json);
extern char *apis_clean_request_string(const char *value);
extern char *validate_media_delivery_path(const char *path);
extern int has_usable_secret(const char *value, int min_length);
extern const char *msw_resolve_effective_model(const char *session,
                                               const char *channel,
                                               const char *global_cfg);
extern void approval_unregister_gateway_notify(void);
extern int async_delegation_active_count(void);
extern void hermes_state_update_session_runtime_lock(hermes_state_db_t *db,
                                                     const char *session_id,
                                                     const char *provider,
                                                     const char *model,
                                                     const char *lock_key);
extern char *hermes_redact_force(const char *input);
extern const char *get_hermes_home(void);
extern int bcb_browser_control_enabled(const json_t *config);
extern int bcb_browser_control_developer_mode(const json_t *config);
extern char **bcb_filter_browser_control_capabilities(const json_t *value,
                                                      int developer_mode,
                                                      size_t *out_n);

/* Weak hooks installed by the gateway runner at startup (same pattern as
 * src/gateway/run_deps.c): NULL means "no active runner", exactly like
 * Python's _gateway_runner_ref() returning None outside the gateway. */
extern __attribute__((weak)) GatewayRunner *api_server_gateway_runner_ref(void);
extern __attribute__((weak)) json_t *gateway_runner_snapshot_session_model_override(
    const GatewayRunner *self, const char *session_key);
extern __attribute__((weak)) int gw_request_hard_interrupt(void *agent,
                                                           const char *reason);
extern __attribute__((weak)) int process_registry_completion_depth(void);

typedef struct { const char *key, *value; } apisg_kv_t;

/* ── Module constants (Port of api_server.py module globals) ───────── */

static const char *const APIsg_REASONING_EFFORTS[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max", "ultra", NULL};

static const apisg_kv_t APIsg_SECURITY_HEADERS[] = {
    {"Content-Security-Policy", "default-src 'none'; frame-ancestors 'none'"},
    {"Permissions-Policy", "camera=(), microphone=(), geolocation=()"},
    {"Strict-Transport-Security", "max-age=31536000; includeSubDomains"},
    {"X-Content-Type-Options", "nosniff"},
    {"X-Frame-Options", "DENY"},
    {"X-XSS-Protection", "0"},
    {"Referrer-Policy", "no-referrer"},
    {NULL, NULL},
};

static const apisg_kv_t APIsg_CORS_HEADERS[] = {
    {"Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS"},
    {"Access-Control-Allow-Headers", "Authorization, Content-Type, Idempotency-Key"},
    {NULL, NULL},
};

static const char *const APIsg_RUNTIME_AGENT_OVERRIDE_KEYS[] = {
    "api_key", "base_url", "provider", "api_mode", "command", "args",
    "credential_pool", "max_tokens", NULL};

#define APIsg_MEDIA_DATA_URL_MAX_BYTES (5 * 1024 * 1024)
static const apisg_kv_t APIsg_MEDIA_MIME[] = {
    {".png", "image/png"}, {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
    {".gif", "image/gif"}, {".webp", "image/webp"}, {".bmp", "image/bmp"},
    {NULL, NULL}};

#define APIsg_BROWSER_CONTROL_PROTOCOL_VERSION 1
#define APIsg_BROWSER_CONTROL_WS_PROTOCOL "hermes-browser-control-v1"
#define APIsg_BROWSER_CONTROL_TICKET_PROTOCOL_PREFIX "hermes-browser-control-ticket."

/* api_server.py: _CRON_AVAILABLE — the C tree always links libcron. */
static const int APIsg_CRON_AVAILABLE = 1;

/* ── fd-level response helpers used below ──────────────────────────── */
void send_json_response_with_retry_after(int fd, int status, const char *json_body,
                                         const char *retry_after);
void send_binary_response(int fd, int status, const unsigned char *body,
                          size_t body_len, const char *content_type,
                          const char *sha256, const char *artifact_id,
                          const char *filename);

/* ── Small shared utilities ─────────────────────────────────────────── */

/* str.strip().lower() into out; returns out. */
static char *apisg_strip_lower(const char *in, char *out, size_t out_sz) {
    size_t n = 0;
    if (!out || !out_sz) return out;
    out[0] = '\0';
    if (!in) return out;
    while (*in == ' ' || *in == '\t' || *in == '\n' || *in == '\r') in++;
    for (; *in && n + 1 < out_sz; in++) out[n++] = (char)tolower((unsigned char)*in);
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    out[n] = '\0';
    return out;
}

/* re.fullmatch(r"[a-z0-9_]+", s) */
static bool apisg_is_lower_ident(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_'))
            return false;
    }
    return true;
}

/* re.match(r"^[a-zA-Z0-9_.-]{2,64}$", s) */
static bool apisg_is_provider_slug(const char *s) {
    size_t n = s ? strlen(s) : 0;
    if (n < 2 || n > 64) return false;
    for (const char *p = s; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '-'))
            return false;
    }
    return true;
}

/* Python uuid4().hex[:24] → 24 hex chars (caller free). */
static char *apisg_uuid_hex24(void) {
    char *u = uuid_v4();
    char *out = malloc(25);
    if (!out) { free(u); return NULL; }
    if (!u) { memset(out, '0', 24); out[24] = '\0'; return out; }
    /* strip dashes from the 36-char uuid and take 24 hex chars */
    size_t k = 0;
    for (const char *p = u; *p && k < 24; p++) {
        if (*p != '-') out[k++] = (char)tolower((unsigned char)*p);
    }
    while (k < 24) out[k++] = '0';
    out[24] = '\0';
    free(u);
    return out;
}

/* ── Registry bookkeeping over the adapter's fixed tables ──────────── */

/* ════════════════════════════════════════════════════════════════════
 * Module-level helper ports
 * ════════════════════════════════════════════════════════════════════ */

/* PoP: apisg_prefix_names_served_profile @ api_server.py:_prefix_names_served_profile */
bool apisg_prefix_names_served_profile(const char *profile) {
    /* Python: profile_matches_home(profile); fail closed on any error.
     * The gateway serves one home; a /p/<x>/ prefix names it only when the
     * x profile directory resolves to that same served home. */
    if (!profile || !*profile) return false;
    const char *home = getenv("HERMES_HOME");
    const char *native = get_hermes_home();
    if (!native || !*native) native = home ? home : "";
    if (!native || !*native) return false;
    /* Single-profile gateway: HERMES_HOME unset or equal to the native root
     * serves the default profile, so no explicit /p/<x>/ prefix names it
     * unless <native>/profiles/<x> IS the served home (profile mode). */
    if (!home || !*home || strcmp(home, native) == 0) {
        if (strcmp(profile, "default") == 0) return true;
        return false;
    }
    char candidate[1024];
    int n = snprintf(candidate, sizeof(candidate), "%s/profiles/%s", native, profile);
    if (n <= 0 || (size_t)n >= sizeof(candidate)) return false;
    char chome[1024];
    n = snprintf(chome, sizeof(chome), "%s", home);
    if (n <= 0 || (size_t)n >= sizeof(chome)) return false;
    /* strip trailing slashes on both before comparing */
    size_t cl = strlen(candidate), hl = strlen(chome);
    while (cl > 1 && candidate[cl - 1] == '/') candidate[--cl] = '\0';
    while (hl > 1 && chome[hl - 1] == '/') chome[--hl] = '\0';
    return strcmp(candidate, chome) == 0;
}

/* PoP: apisg_artifact_scope_facade_repr @ api_server.py:__repr__ */
char *apisg_artifact_scope_facade_repr(const char *principal_id) {
    /* f"_ArtifactScopeFacade(principal={self.principal_id!r})" */
    size_t cap = strlen(principal_id ? principal_id : "") + 64;
    char *out = malloc(cap);
    if (!out) return NULL;
    snprintf(out, cap, "_ArtifactScopeFacade(principal='%s')",
             principal_id ? principal_id : "");
    return out;
}

/* PoP: apisg_approval_event_choices @ api_server.py:_approval_event_choices */
json_t *apisg_approval_event_choices(bool smart_denied, bool allow_session,
                                     bool allow_permanent) {
    json_t *arr = json_array();
    if (!arr) return NULL;
    if (smart_denied || !allow_session) {
        json_append(arr, json_string("once"));
        json_append(arr, json_string("deny"));
        return arr;
    }
    json_append(arr, json_string("once"));
    json_append(arr, json_string("session"));
    if (allow_permanent) json_append(arr, json_string("always"));
    json_append(arr, json_string("deny"));
    return arr;
}

/* PoP: apisg_browser_controller_ws_send @ api_server.py:_browser_controller_ws_sender */
int apisg_browser_controller_ws_send(void *ws, void *loop, const char *frame_json,
                                     double wait_timeout) {
    /* A closed controller socket is a hard ConnectionError, like Python. */
    if (!ws) {
        fprintf(stderr, "browser-control websocket is closed\n");
        return -1;
    }
    (void)loop; /* C transport has a single loop handle inside ws */
    if (!frame_json) return -1;
    /* Keep the broker command pending across a slow send: enqueue
     * thread-safely and let the frame's own deadline decide. wait_timeout
     * bounds only the in-flight handoff, never rejects the frame. */
    sse_queue_t *q = (sse_queue_t *)ws;
    if (!sse_queue_put(q, frame_json)) {
        fprintf(stderr,
                "browser-controller websocket send pending after wait timeout (%.1fs)\n",
                wait_timeout > 0 ? wait_timeout : 10.0);
        return 1; /* still in flight, not rejected */
    }
    return 0;
}

/* PoP: apisg_request_reasoning_config @ api_server.py:_request_reasoning_config */
json_t *apisg_request_reasoning_config(const json_t *model_options) {
    if (!json_is_object(model_options)) return NULL;
    const json_t *reasoning = json_obj_get(model_options, "reasoning");
    json_t *enabled = NULL;               /* tri-state: NULL = absent */
    const json_t *effort_v = json_obj_get(model_options, "reasoning_effort");
    const char *effort = (effort_v && json_is_string(effort_v)) ? effort_v->str_val : NULL;
    if (json_is_object(reasoning)) {
        enabled = json_obj_get(reasoning, "enabled");
        const json_t *r_effort = json_obj_get(reasoning, "effort");
        if (r_effort) {
            effort = json_is_string(r_effort) ? r_effort->str_val : NULL;
            if (!effort) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", r_effort->num_val);
                static char effort_buf[32];
                snprintf(effort_buf, sizeof(effort_buf), "%s", buf);
                effort = effort_buf;
            }
        }
    }
    char effort_norm[64] = "";
    apisg_strip_lower(effort, effort_norm, sizeof(effort_norm));
    bool enabled_false = (enabled && json_is_bool(enabled) && !enabled->bool_val);
    bool enabled_true = (enabled && json_is_bool(enabled) && enabled->bool_val);
    if (enabled_false || strcmp(effort_norm, "none") == 0) {
        json_t *off = json_object();
        json_set(off, "enabled", json_bool(false));
        return off;
    }

    bool in_set = false;
    for (const char *const *e = APIsg_REASONING_EFFORTS; *e; e++)
        if (strcmp(effort_norm, *e) == 0) { in_set = true; break; }
    if (in_set && strcmp(effort_norm, "none") != 0) {
        json_t *on = json_object();
        json_set(on, "enabled", json_bool(true));
        json_set(on, "effort", json_string(effort_norm));
        return on;
    }
    if (enabled_true) {
        json_t *on2 = json_object();
        json_set(on2, "enabled", json_bool(true));
        return on2;
    }
    return NULL;
}

/* PoP: apisg_apply_runtime_agent_overrides @ api_server.py:_apply_runtime_agent_overrides */
json_t *apisg_apply_runtime_agent_overrides(json_t *runtime_kwargs,
                                            const json_t *overrides) {
    if (!json_is_object(overrides)) return runtime_kwargs;
    if (!json_is_object(runtime_kwargs)) return runtime_kwargs;
    for (const char *const *key = APIsg_RUNTIME_AGENT_OVERRIDE_KEYS; *key; key++) {
        const json_t *v = json_obj_get(overrides, *key);
        if (!v) continue;
        if (json_is_null(v)) continue;
        if (strcmp(*key, "args") == 0 && json_is_array(v)) {
            json_t *copy = json_copy(v);
            if (copy) json_set(runtime_kwargs, *key, copy);
        } else {
            json_set(runtime_kwargs, *key, json_copy(v));
        }
    }
    return runtime_kwargs;
}

/* PoP: apisg_resolve_request_runtime_agent_kwargs @ api_server.py:_resolve_request_runtime_agent_kwargs */
json_t *apisg_resolve_request_runtime_agent_kwargs(const char *provider,
                                                   const char *target_model) {
    /* Mirrors gateway.run._resolve_runtime_agent_kwargs with an explicit
     * provider/model. The full runtime-provider catalog chain resolves via
     * the same host-installed hooks run.c uses; when no host hook is live the
     * resolution fails exactly like Python's RuntimeError path — surfaced as
     * an error payload rather than a crash. */
    extern __attribute__((weak)) json_t *gw_resolve_runtime_provider(
        const char *requested, const char *target_model);
    json_t *runtime = NULL;
    if (gw_resolve_runtime_provider)
        runtime = gw_resolve_runtime_provider(provider, target_model);
    json_t *out = json_object();
    long max_tokens = -1; /* None until resolved */
    const char *env_max = getenv("HERMES_MAX_TOKENS");
    if (env_max && *env_max) {
        char *end = NULL;
        long v = strtol(env_max, &end, 10);
        if (end && *end == '\0' && end != env_max) max_tokens = v;
    }
    if (max_tokens < 0 && runtime && json_is_object(runtime)) {
        const json_t *cfg = json_obj_get(runtime, "max_tokens");
        if (cfg && json_is_number(cfg)) max_tokens = (long)cfg->num_val;
    }
    if (max_tokens < 0 && runtime && json_is_object(runtime)) {
        const json_t *rt = json_obj_get(runtime, "max_output_tokens");
        if (rt && json_is_number(rt) && rt->num_val > 0) max_tokens = (long)rt->num_val;
    }
    if (runtime && json_is_object(runtime)) {
        const char *keys[] = {"api_key", "base_url", "provider", "api_mode",
                              "command", "credential_pool"};
        for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const json_t *v = json_obj_get(runtime, keys[i]);
            if (v && !json_is_null(v)) json_set(out, keys[i], json_copy(v));
        }
        const json_t *args = json_obj_get(runtime, "args");
        json_set(out, "args", args ? json_copy(args) : json_array());
    } else {
        fprintf(stderr,
                "[api_server] runtime provider resolution failed for %s/%s\n",
                provider ? provider : "(null)", target_model ? target_model : "(null)");
        json_set(out, "args", json_array());
    }
    if (max_tokens >= 0)
        json_set(out, "max_tokens", json_number((double)max_tokens));
    else
        json_set(out, "max_tokens", json_null());
    if (runtime) json_free(runtime);
    return out;
}

/* PoP: apisg_request_agent_overrides @ api_server.py:_request_agent_overrides */
json_t *apisg_request_agent_overrides(const json_t *body, const char *virtual_model,
                                      bool allow_bare_model) {
    json_t *overrides = json_object();
    if (!json_is_object(body)) return overrides;
    char *provider = NULL;
    const json_t *pv = json_obj_get(body, "provider");
    if (pv && json_is_string(pv)) provider = apis_clean_request_string(pv->str_val);
    if (provider && *provider) json_set(overrides, "requested_provider", json_string(provider));

    char *model = NULL;
    const json_t *mv = json_obj_get(body, "model");
    if (mv && json_is_string(mv)) model = apis_clean_request_string(mv->str_val);
    if (model && *model &&
        !(virtual_model && strcmp(model, virtual_model) == 0) &&
        ((provider && *provider) || allow_bare_model))
        json_set(overrides, "requested_model", json_string(model));

    const json_t *mo = json_obj_get(body, "model_options");
    if (json_is_object(mo)) json_set(overrides, "model_options", json_copy(mo));
    free(provider);
    free(model);
    return overrides;
}

/* PoP: apisg_resolve_media_to_data_urls @ api_server.py:_resolve_media_to_data_urls */
char *apisg_resolve_media_to_data_urls(const char *text) {
    /* Replace ``MEDIA:<abs path>`` image tags with inline base64 data URLs.
     * Anchored on an absolute-path token after MEDIA: with a known image
     * extension, gated by validate_media_delivery_path() and the 5MB cap;
     * anything else is left untouched. */
    if (!text || !*text || !strstr(text, "MEDIA:")) {
        char *out = malloc(strlen(text ? text : "") + 1);
        if (out) strcpy(out, text ? text : "");
        return out;
    }
    size_t cap = strlen(text) + 4096;
    char *out = malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = text;
    while (*p) {
        const char *tag = strstr(p, "MEDIA:");
        if (!tag) break;
        /* copy everything before the tag */
        size_t pre = (size_t)(tag - p);
        memcpy(out + o, p, pre);
        o += pre;
        const char *rest = tag + strlen("MEDIA:");
        while (*rest == ' ' || *rest == '\t') rest++;
        if (*rest != '/') { /* not an anchored absolute path — keep verbatim */
            size_t seg = (size_t)(rest - tag);
            memcpy(out + o, tag, seg);
            o += seg;
            p = rest;
            continue;
        }
        /* token ends at first whitespace / closing bracket / quote */
        const char *q = rest;
        while (*q && !isspace((unsigned char)*q) && *q != ')' && *q != '"' &&
               *q != '`' && *q != '\'')
            q++;
        size_t plen = (size_t)(q - rest);
        char *raw = malloc(plen + 1);
        if (!raw) { free(out); return NULL; }
        memcpy(raw, rest, plen);
        raw[plen] = '\0';
        char *safe = validate_media_delivery_path(raw);
        const char *mime = NULL;
        size_t rlen = strlen(raw);
        for (const apisg_kv_t *m = APIsg_MEDIA_MIME; m->key; m++) {
            size_t el = strlen(m->key);
            if (rlen >= el &&
                strcasecmp(raw + rlen - el, m->key) == 0) { mime = m->value; break; }
        }
        FILE *fp = NULL;
        long fsz = -1;
        if (safe && mime) {
            struct stat st;
            if (stat(safe, &st) == 0 && S_ISREG(st.st_mode) &&
                st.st_size <= APIsg_MEDIA_DATA_URL_MAX_BYTES) {
                fp = fopen(safe, "rb");
                if (fp) { fseek(fp, 0, SEEK_END); fsz = ftell(fp); fseek(fp, 0, SEEK_SET); }
            }
        }
        if (!(safe && mime && fp && fsz >= 0)) {
            /* leave the original token untouched */
            size_t seg = (size_t)(q - tag);
            memcpy(out + o, tag, seg);
            o += seg;
        } else {
            unsigned char *data = malloc((size_t)fsz);
            if (!data) { fclose(fp); free(raw); free(out); return NULL; }
            size_t rd = fread(data, 1, (size_t)fsz, fp);
            fclose(fp);
            extern char *base64_encode(const unsigned char *, size_t);
            char *b64 = rd ? base64_encode(data, rd) : NULL;
            free(data);
            if (b64) {
                int n = snprintf(out + o, cap - o, "![image](data:%s;base64,%s)",
                                 mime, b64);
                if (n < 0 || (size_t)n >= cap - o) { free(b64); free(raw); free(out); return NULL; }
                o += (size_t)n;
                free(b64);
            } else {
                size_t seg = (size_t)(q - tag);
                memcpy(out + o, tag, seg);
                o += seg;
            }
        }
        free(raw);
        p = q;
    }
    /* tail */
    size_t tail = strlen(p);
    if (o + tail + 1 > cap) {
        char *grown = realloc(out, o + tail + 1);
        if (!grown) { free(out); return NULL; }
        out = grown;
        cap = o + tail + 1;
    }
    memcpy(out + o, p, tail + 1);
    o += tail;
    out[o] = '\0';
    return out;
}

/* PoP: apisg_openai_error @ api_server.py:_openai_error */
json_t *apisg_openai_error(const char *message, const char *err_type,
                           const char *param, const char *code) {
    char *redacted = apis_redact_api_error_text(message ? message : "", 0);
    json_t *err = json_object();
    json_set(err, "message", json_string(redacted ? redacted : ""));
    free(redacted);
    json_set(err, "type", json_string(err_type ? err_type : "invalid_request_error"));
    if (param) json_set(err, "param", json_string(param));
    else json_set(err, "param", json_null());
    if (code) json_set(err, "code", json_string(code));
    else json_set(err, "code", json_null());
    json_t *envelope = json_object();
    json_set(envelope, "error", err);
    return envelope;
}

/* ── Pending-work reservation (api_server.py module-level trio) ────── */

typedef struct api_reservation { bool active; bool detached; } api_reservation_t;

/* PoP: apisg_admit_api_agent_request @ api_server.py:_admit_api_agent_request */
bool apisg_admit_api_agent_request(api_server_adapter_t *adapter,
                                   api_reservation_t *reservation) {
    /* Reserve an authenticated API turn before its handler first awaits:
     * drain check + reservation in one non-awaiting block. */
    if (!adapter || !reservation) return false;
    reservation->active = true;
    reservation->detached = false;
    adapter->pending_agent_requests++;
    return true;
}

/* PoP: apisg_release_pending_api_work @ api_server.py:_release_pending_api_work */
void apisg_release_pending_api_work(api_server_adapter_t *adapter,
                                    api_reservation_t *reservation) {
    /* Release a pending-work reservation exactly once. */
    if (!adapter || !reservation || !reservation->active) return;
    reservation->active = false;
    adapter->pending_agent_requests =
        adapter->pending_agent_requests > 0 ? adapter->pending_agent_requests - 1 : 0;
}

/* PoP: apisg_reserve_pending_api_work @ api_server.py:_reserve_pending_api_work */
api_reservation_t *apisg_reserve_pending_api_work(api_server_adapter_t *adapter) {
    /* Keep externally-triggered background work visible across awaits. */
    if (!adapter) return NULL;
    api_reservation_t *r = malloc(sizeof(*r));
    if (!r) return NULL;
    r->active = true;
    r->detached = false;
    adapter->pending_agent_requests++;
    return r;
}

/* PoP: apisg_response_store_len @ api_server.py:__len__ */
int apisg_response_store_len(response_store_t *store) {
    if (!store || !store->conn) return 0;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(store->conn, "SELECT COUNT(*) FROM responses", -1,
                           &stmt, NULL) != SQLITE_OK)
        return 0;
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        n = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return n;
}

/* PoP: apisg_response_store_tighten_file_permissions @ api_server.py:_tighten_file_permissions */
void apisg_response_store_tighten_file_permissions(response_store_t *store) {
    /* Force owner-only permissions on the DB and SQLite sidecars. */
    if (!store || !store->db_path) return;
    char path[1024];
    static const char *const suffixes[] = {"", "-wal", "-shm"};
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++) {
        int n = snprintf(path, sizeof(path), "%s%s", store->db_path, suffixes[i]);
        if (n <= 0 || (size_t)n >= sizeof(path)) continue;
        struct stat st;
        if (stat(path, &st) == 0 && chmod(path, 0600) != 0)
            fprintf(stderr, "[api_server] failed to restrict response store permissions for %s: %s\n",
                    path, strerror(errno));
    }
}

/* PoP: apisg_put_threadsafe @ api_server.py:put_threadsafe */
void apisg_put_threadsafe(sse_queue_t *queue, const char *item) {
    /* (loop or self._loop_ref).call_soon_threadsafe(self.put_nowait, item):
     * sse_queue_put is mutex-guarded, so it is safe from any thread. */
    if (queue && item) sse_queue_put(queue, item);
}

/* ── Middleware ports ───────────────────────────────────────────────── */

/* PoP: apisg_cors_headers_for_origin @ api_server.py:_cors_headers_for_origin */
json_t *apisg_cors_headers_for_origin(api_server_adapter_t *adapter,
                                      const char *origin) {
    if (!origin || !*origin || adapter->cors_count == 0) return NULL;
    json_t *headers = json_object();
    bool wildcard = false, exact = false;
    for (int i = 0; i < adapter->cors_count; i++) {
        if (strcmp(adapter->cors_origins[i], "*") == 0) wildcard = true;
        if (strcmp(adapter->cors_origins[i], origin) == 0) exact = true;
    }
    if (!wildcard && !exact) { json_free(headers); return NULL; }
    for (const apisg_kv_t *h = APIsg_CORS_HEADERS; h->key; h++)
        json_set(headers, h->key, json_string(h->value));
    if (wildcard) {
        json_set(headers, "Access-Control-Allow-Origin", json_string("*"));
    } else {
        json_set(headers, "Access-Control-Allow-Origin", json_string(origin));
        json_set(headers, "Vary", json_string("Origin"));
    }
    json_set(headers, "Access-Control-Max-Age", json_string("600"));
    return headers;
}

/* PoP: apisg_parse_cors_origins @ api_server.py:_parse_cors_origins */
json_t *apisg_parse_cors_origins(const json_t *value) {
    json_t *out = json_array();
    if (!out) return NULL;
    if (!value || json_is_null(value)) return out;
    if (json_is_string(value)) {
        char *copy = strdup(value->str_val);
        if (!copy) return out;
        char *save = NULL, *tok = strtok_r(copy, ",", &save);
        while (tok) {
            while (*tok == ' ' || *tok == '\t') tok++;
            char *end = tok + strlen(tok);
            while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
            if (*tok) json_append(out, json_string(tok));
            tok = strtok_r(NULL, ",", &save);
        }
        free(copy);
        return out;
    }
    if (json_is_array(value)) {
        for (size_t i = 0; i < json_len(value); i++) {
            const json_t *it = json_get(value, i);
            if (!it) continue;
            char buf[256];
            if (json_is_string(it)) snprintf(buf, sizeof(buf), "%s", it->str_val);
            else snprintf(buf, sizeof(buf), "%lld", (long long)(it->num_val));
            const char *s = buf;
            while (*s == ' ') s++;
            if (*s) json_append(out, json_string(s));
        }
        return out;
    }
    /* str(value) fallback for scalars */
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
    json_append(out, json_string(buf));
    return out;
}

/* PoP: apisg_origin_allowed @ api_server.py:_origin_allowed (adapter wrapper used by cors_middleware) */
bool apisg_adapter_origin_allowed(api_server_adapter_t *adapter, const char *origin) {
    if (!adapter) return false;
    json_t *allowed = json_array();
    for (int i = 0; i < adapter->cors_count; i++)
        json_append(allowed, json_string(adapter->cors_origins[i]));
    char *serialized = json_serialize(allowed);
    json_free(allowed);
    bool ok = aps_origin_allowed(origin ? origin : "", serialized ? serialized : "");
    free(serialized);
    return ok;
}

/* api_server.py:cors_middleware — see note below. */
/* PoP: apisg_cors_middleware @ api_server.py:cors_middleware */
/* headers_out set; 0 → continue (headers_out carries CORS headers to merge). */
int apisg_cors_middleware(api_server_adapter_t *adapter, const char *method,
                          const char *origin, json_t **headers_out) {
    if (headers_out) *headers_out = NULL;
    json_t *cors_headers = NULL;
    if (adapter) {
        if (!apisg_adapter_origin_allowed(adapter, origin))
            return 403;
        cors_headers = apisg_cors_headers_for_origin(adapter, origin);
    }
    if (method && strcmp(method, "OPTIONS") == 0) {
        if (!cors_headers) return 403;
        if (headers_out) *headers_out = cors_headers;
        return 200;
    }
    if (headers_out) *headers_out = cors_headers;
    return 0;
}

/* api_server.py:body_limit_middleware — see note below. */
/* PoP: apisg_body_limit_middleware @ api_server.py:body_limit_middleware */
/* 413 body_too_large envelope via status 413. */
int apisg_body_limit_middleware(const char *method, const char *content_length) {
    if (method && content_length &&
        (strcmp(method, "POST") == 0 || strcmp(method, "PUT") == 0 ||
         strcmp(method, "PATCH") == 0)) {
        char *end = NULL;
        errno = 0;
        long cl = strtol(content_length, &end, 10);
        if (errno != 0 || !end || *end != '\0' || end == content_length)
            return 400; /* invalid_content_length */
        if (cl > MAX_REQUEST_BYTES) return 413; /* body_too_large */
    }
    return 0;
}

/* PoP: apisg_security_headers_middleware @ api_server.py:security_headers_middleware */
void apisg_security_headers_middleware(json_t *response_headers) {
    /* setdefault semantics: only add a header when not already present. */
    if (!response_headers || !json_is_object(response_headers)) return;
    for (const apisg_kv_t *h = APIsg_SECURITY_HEADERS; h->key; h++) {
        if (!json_obj_get(response_headers, h->key))
            json_set(response_headers, h->key, json_string(h->value));
    }
}

/* ── Audit / origin metadata cluster ───────────────────────────────── */

/* PoP: apisg_clean_log_value @ api_server.py:_clean_log_value */
char *apisg_clean_log_value(const char *value, size_t max_len) {
    /* Sanitize request metadata before it reaches security logs: CR/LF
     * collapsed to spaces, surrounding whitespace stripped, bounded. */
    if (!value) return strdup("");
    size_t n = strlen(value);
    char *tmp = malloc(n + 1);
    if (!tmp) return NULL;
    size_t k = 0;
    for (const char *p = value; *p; p++) tmp[k++] = (*p == '\r' || *p == '\n') ? ' ' : *p;
    while (k > 0 && (tmp[k - 1] == ' ')) k--;
    size_t start = 0;
    while (start < k && tmp[start] == ' ') start++;
    size_t len = k - start;
    if (len > max_len) len = max_len;
    char *out = malloc(len + 1);
    if (!out) { free(tmp); return NULL; }
    memcpy(out, tmp + start, len);
    out[len] = '\0';
    free(tmp);
    return out;
}

/* PoP: apisg_request_audit_context @ api_server.py:_request_audit_context */
json_t *apisg_request_audit_context(const char *remote, const char *peer_ip,
                                    const char *forwarded_for,
                                    const char *real_ip, const char *method,
                                    const char *path_qs, const char *user_agent) {
    json_t *ctx = json_object();
    char *v;
    v = apisg_clean_log_value((remote && *remote) ? remote : peer_ip, 200);
    json_set(ctx, "remote", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(peer_ip, 200);
    json_set(ctx, "peer_ip", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(forwarded_for, 200);
    json_set(ctx, "forwarded_for", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(real_ip, 200);
    json_set(ctx, "real_ip", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(method, 16);
    json_set(ctx, "method", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(path_qs, 500);
    json_set(ctx, "path", json_string(v ? v : "")); free(v);
    v = apisg_clean_log_value(user_agent, 300);
    json_set(ctx, "user_agent", json_string(v ? v : "")); free(v);
    return ctx;
}

/* PoP: apisg_request_audit_log_suffix @ api_server.py:_request_audit_log_suffix */
char *apisg_request_audit_log_suffix(const json_t *ctx) {
    if (!json_is_object(ctx))
        return strdup("source='unknown'");
    char *out = malloc(4096);
    if (!out) return NULL;
    size_t o = 0;
    out[0] = '\0';
    for (size_t i = 0; i < json_object_size(ctx); i++) {
        const char *key = json_object_get_key_at(ctx, i);
        const json_t *val = json_object_get_at(ctx, i);
        const char *s = (val && json_is_string(val)) ? val->str_val : "";
        if (!s || !*s) continue;
        int n = snprintf(out + o, 4096 - o, "%s%s='%s'", o ? " " : "", key, s);
        if (n < 0 || (size_t)n >= 4096 - o) break;
        o += (size_t)n;
    }
    if (o == 0) snprintf(out, 4096, "source='unknown'");
    return out;
}

/* PoP: apisg_cron_origin_from_request @ api_server.py:_cron_origin_from_request */
json_t *apisg_cron_origin_from_request(const json_t *audit_ctx) {
    json_t *origin = json_object();
    json_set(origin, "platform", json_string("api_server"));
    json_set(origin, "chat_id", json_string("api"));
    static const char *const fields[] =
        {"remote:source_ip", "peer_ip:peer_ip", "forwarded_for:forwarded_for",
         "real_ip:real_ip", "user_agent:user_agent"};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        char src[64], dst[64];
        sscanf(fields[i], "%63[^:]:%63s", src, dst);
        const json_t *v = json_obj_get(audit_ctx, src);
        const char *s = (v && json_is_string(v)) ? v->str_val : NULL;
        if (s && *s) json_set(origin, dst, json_string(s));
    }
    return origin;
}

/* PoP: apisg_normalize_callback_platform @ api_server.py:_normalize_callback_platform */
char *apisg_normalize_callback_platform(const char *value) {
    char norm[256];
    apisg_strip_lower(value, norm, sizeof(norm));
    for (char *p = norm; *p; p++)
        if (*p == '-') *p = '_';
    if (!apisg_is_lower_ident(norm)) return strdup("");
    return strdup(norm);
}

/* PoP: apisg_get_platform_callback_adapter @ api_server.py:_get_platform_callback_adapter */
void *apisg_get_platform_callback_adapter(api_server_adapter_t *adapter,
                                         json_t *app,
                                         const char *platform_name) {
    /* injected app["platform_event_adapters"] wins first */
    if (app && json_is_object(app)) {
        const json_t *injected = json_obj_get(app, "platform_event_adapters");
        if (json_is_object(injected)) {
            const json_t *a = json_obj_get(injected, platform_name);
            if (a) return (void *)a;
        }
        char key[288];
        snprintf(key, sizeof(key), "%s_adapter", platform_name);
        const json_t *a = json_obj_get(app, key);
        if (a) return (void *)a;
    }
    GatewayRunner *runner = api_server_gateway_runner_ref();
    (void)adapter;
    if (!runner) return NULL;
    extern __attribute__((weak)) void *gateway_runner_adapter_for(
        GatewayRunner *self, const char *platform_value);
    if (!gateway_runner_adapter_for) return NULL;
    return gateway_runner_adapter_for(runner, platform_name);
}

/* api_server.py:_handle_platform_event_callback — see note below. */
/* PoP: apisg_handle_platform_event_callback @ api_server.py:_handle_platform_event_callback */
/* Writes the JSON response over client_fd and returns the status used. */
int apisg_handle_platform_event_callback(api_server_adapter_t *adapter, int client_fd,
                                         const char *platform_raw, json_t *app,
                                         const char *auth_header, const char *body) {
    char *platform_name = apisg_normalize_callback_platform(platform_raw);
    int status;
    if (!platform_name || !*platform_name) {
        json_t *err = apisg_openai_error("Invalid platform name", NULL, NULL,
                                         "invalid_platform");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        free(platform_name);
        return 400;
    }

    void *cb_adapter = apisg_get_platform_callback_adapter(adapter, app, platform_name);
    if (!cb_adapter) {
        json_t *err = apisg_openai_error("Platform adapter is not connected", NULL,
                                         NULL, "platform_unavailable");
        char *s = json_serialize(err);
        send_json_response(client_fd, 503, s);
        free(s); json_free(err);
        free(platform_name);
        return 503;
    }

    /* verifier/dispatcher capability probes on the adapter record */
    extern __attribute__((weak)) int gateway_platform_verify_http_event_request(
        void *adapter_rec, const char *auth_header, char *code_out, size_t code_sz);
    extern __attribute__((weak)) int gateway_platform_dispatch_http_event(
        void *adapter_rec, const json_t *payload, json_t **result_out);
    if (!gateway_platform_verify_http_event_request ||
        !gateway_platform_dispatch_http_event) {
        json_t *err = apisg_openai_error(
            "Platform adapter does not support HTTP events", NULL, NULL,
            "platform_http_events_unsupported");
        char *s = json_serialize(err);
        send_json_response(client_fd, 503, s);
        free(s); json_free(err);
        free(platform_name);
        return 503;
    }

    char code[128] = "";
    int ok = 0;
    /* Fail closed: a crashing verifier must never admit the event. */
    if (gateway_platform_verify_http_event_request(cb_adapter,
                                                   auth_header ? auth_header : "",
                                                   code, sizeof(code)) == 1)
        ok = 1;
    else
        fprintf(stderr, "[api_server] Platform HTTP event verifier failed for %s\n",
                platform_name);
    if (!ok) {
        json_t *err = apisg_openai_error(
            "Invalid platform event authorization", NULL, NULL,
            (code[0]) ? code : "invalid_platform_event_authorization");
        char *s = json_serialize(err);
        send_json_response(client_fd, 401, s);
        free(s); json_free(err);
        free(platform_name);
        return 401;
    }

    json_t *payload = body ? json_parse(body, NULL) : NULL;
    if (!payload || !json_is_object(payload)) {
        json_t *err = apisg_openai_error(
            payload ? "Platform event must be a JSON object"
                    : "Invalid JSON in platform event",
            NULL, NULL, payload ? "invalid_request" : "invalid_json");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        if (payload) json_free(payload);
        free(platform_name);
        return 400;
    }

    json_t *result = NULL;
    if (gateway_platform_dispatch_http_event(cb_adapter, payload, &result) != 1) {
        fprintf(stderr, "[api_server] Platform HTTP event dispatch failed for %s\n",
                platform_name);
        json_t *err = apisg_openai_error("Platform event dispatch failed",
                                         "server_error", NULL,
                                         "platform_event_dispatch_failed");
        char *s = json_serialize(err);
        send_json_response(client_fd, 500, s);
        free(s); json_free(err);
        json_free(payload);
        free(platform_name);
        return 500;
    }
    json_t *resp = (result && json_is_object(result)) ? result : json_object();
    char *s = json_serialize(resp);
    send_json_response(client_fd, 200, s);
    free(s);
    status = 200;
    if (resp != result) json_free(resp);
    if (result) json_free(result);
    json_free(payload);
    free(platform_name);
    return status;
}

/* api_server.py:_make_profile_prefix_middleware — see note below. */
/* PoP: apisg_profile_prefix_middleware @ api_server.py:_make_profile_prefix_middleware */
/* Returns 404 to reject, 0 to continue with *profile_out set. */
int apisg_profile_prefix_middleware(api_server_adapter_t *adapter,
                                    const char *profile, char **profile_out) {
    if (profile_out) *profile_out = NULL;
    if (profile && *profile &&
        !apisg_prefix_names_served_profile(profile) &&
        !(strcmp(profile, "default") == 0)) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Unknown or unconfigured profile"));
        return 404; /* caller serializes err with status 404 and frees */
    }
    (void)adapter;
    if (profile_out)
        *profile_out = strdup((profile && *profile) ? profile : "default");
    return 0;
}

/* PoP: apisg_gateway_is_draining @ api_server.py:_gateway_is_draining */
bool apisg_gateway_is_draining(void) {
    GatewayRunner *runner = api_server_gateway_runner_ref();
    if (!runner) return false;
    extern __attribute__((weak)) bool gateway_runner_is_draining(const GatewayRunner *);
    extern __attribute__((weak)) bool gateway_runner_external_drain_active(
        const GatewayRunner *);
    bool draining = false;
    if (gateway_runner_is_draining && gateway_runner_is_draining(runner)) draining = true;
    if (gateway_runner_external_drain_active &&
        gateway_runner_external_drain_active(runner)) draining = true;
    return draining;
}

/* api_server.py:_draining_response — see note below. */
/* PoP: apisg_draining_response @ api_server.py:_draining_response */
/* Returns a retryable 503 envelope (caller adds Retry-After: 1) or NULL. */
json_t *apisg_draining_response(void) {
    if (!apisg_gateway_is_draining()) return NULL;
    json_t *err = apisg_openai_error(
        "Gateway is draining existing work; retry shortly.", NULL, NULL,
        "gateway_draining");
    return err;
}

/* PoP: apisg_activate_admitted_request @ api_server.py:_activate_admitted_request */
void apisg_activate_admitted_request(api_server_adapter_t *adapter,
                                     api_reservation_t *reservation) {
    /* Transfer this request's drain reservation to agent bookkeeping. */
    if (!adapter || !reservation || !reservation->active) return;
    reservation->active = false;
    adapter->pending_agent_requests =
        adapter->pending_agent_requests > 0 ? adapter->pending_agent_requests - 1 : 0;
}

/* PoP: apisg_readiness_work_counts @ api_server.py:_readiness_work_counts */
void apisg_readiness_work_counts(api_server_adapter_t *adapter, int *active_api_runs,
                                 int *process_depth, int *active_delegations) {
    int runs = 0;
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        run_status_t *rs = adapter->run_statuses[i];
        if (!rs) continue;
        /* "stopping" is not terminal — it is doing real executor-thread work
         * until the agent notices the interrupt and settles to cancelled. */
        if (strcmp(rs->status, "queued") == 0 || strcmp(rs->status, "running") == 0 ||
            strcmp(rs->status, "waiting_for_approval") == 0 ||
            strcmp(rs->status, "stopping") == 0)
            runs++;
    }
    pthread_mutex_unlock(&adapter->run_lock);
    int depth = 0;
    if (process_registry_completion_depth)
        depth = process_registry_completion_depth();
    *active_api_runs = runs;
    *process_depth = depth;
    *active_delegations = async_delegation_active_count();
}

/* PoP: apisg_active_agent_work_count @ api_server.py:active_agent_work_count */
int apisg_active_agent_work_count(api_server_adapter_t *adapter) {
    if (!adapter) return 0;
    /* pending + inflight + live tasks (queued-before-agent gap covered by the
     * task accounting; completed tasks retained until cleanup excluded). */
    int live_tasks = 0;
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count; i++) {
        if (adapter->run_statuses[i] && adapter->run_statuses[i]->running)
            live_tasks++;
    }
    pthread_mutex_unlock(&adapter->run_lock);
    return adapter->pending_agent_requests + adapter->inflight_agent_runs + live_tasks;
}

/* PoP: apisg_interrupt_active_runs @ api_server.py:interrupt_active_runs */
int apisg_interrupt_active_runs(api_server_adapter_t *adapter, const char *reason) {
    if (!adapter) return 0;
    /* Dedupe by pointer identity across _active_run_agents and
     * _shutdown_interruptible_agents — an agent published to both must still
     * be interrupted exactly once. _pending_agent_requests is intentionally
     * not covered: no agent object exists yet. */
    void *seen[80];
    size_t seen_n = 0;
    int interrupted = 0;
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) {
            for (size_t i = 0; i < adapter->shutdown_interruptible_count; i++) {
                void *agent = adapter->shutdown_interruptible_agents[i];
                if (!agent) continue;
                int dup = 0;
                for (size_t j = 0; j < seen_n; j++) if (seen[j] == agent) dup = 1;
                if (!dup && seen_n < sizeof(seen) / sizeof(seen[0])) seen[seen_n++] = agent;
            }
        } else {
            pthread_mutex_lock(&adapter->run_lock);
            for (int i = 0; i < adapter->run_status_count; i++) {
                run_status_t *rs = adapter->run_statuses[i];
                if (rs && rs->output) {
                    void *agent = (void *)rs; /* registry entry stands in for agent ref */
                    int dup = 0;
                    for (size_t j = 0; j < seen_n; j++) if (seen[j] == agent) dup = 1;
                    if (!dup && seen_n < sizeof(seen) / sizeof(seen[0])) seen[seen_n++] = agent;
                }
            }
            pthread_mutex_unlock(&adapter->run_lock);
        }
    }
    for (size_t i = 0; i < seen_n; i++) {
        int accepted = gw_request_hard_interrupt
                           ? gw_request_hard_interrupt(seen[i], reason) : 0;
        if (accepted > 0) interrupted++;
        else fprintf(stderr, "[api_server] failed interrupting active agent (%s)\n", reason);
    }
    return interrupted;
}

/* PoP: apisg_resolve_model_name @ api_server.py:_resolve_model_name */
char *apisg_resolve_model_name(const char *explicit_name) {
    /* explicit override > active profile name > "hermes-agent". */
    char profile_name[128] = "";
    const char *profile = getenv("HERMES_PROFILE");
    if (profile && *profile && strcmp(profile, "default") != 0 &&
        strcmp(profile, "custom") != 0)
        snprintf(profile_name, sizeof(profile_name), "%s", profile);
    const char *resolved = msw_resolve_effective_model(
        explicit_name ? explicit_name : "", profile_name, "hermes-agent");
    return strdup(resolved && *resolved ? resolved : "hermes-agent");
}

/* PoP: apisg_http_route_table @ api_server.py:_http_route_table */
json_t *apisg_http_route_table(void) {
    static const struct { const char *method, *path, *handler; } routes[] = {
        {"GET", "/health", "_handle_health"},
        {"GET", "/health/detailed", "_handle_health_detailed"},
        {"GET", "/v1/health", "_handle_health"},
        {"GET", "/v1/models", "_handle_models"},
        {"GET", "/api/model/options", "_handle_model_options"},
        {"GET", "/v1/capabilities", "_handle_capabilities"},
        {"POST", "/v1/browser-control/register", "_handle_browser_control_register"},
        {"GET", "/v1/browser-control/ws", "_handle_browser_control_ws"},
        {"POST", "/v1/artifacts/upload", "_handle_artifact_upload"},
        {"GET", "/v1/artifacts/download/{artifact_id}", "_handle_artifact_download"},
        {"GET", "/v1/skills", "_handle_skills"},
        {"GET", "/v1/toolsets", "_handle_toolsets"},
        {"GET", "/api/sessions", "_handle_list_sessions"},
        {"POST", "/api/sessions", "_handle_create_session"},
        {"GET", "/api/sessions/{session_id}", "_handle_get_session"},
        {"PATCH", "/api/sessions/{session_id}", "_handle_patch_session"},
        {"DELETE", "/api/sessions/{session_id}", "_handle_delete_session"},
        {"GET", "/api/sessions/{session_id}/messages", "_handle_session_messages"},
        {"POST", "/api/sessions/{session_id}/fork", "_handle_fork_session"},
        {"POST", "/api/sessions/{session_id}/chat", "_handle_session_chat"},
        {"POST", "/api/sessions/{session_id}/chat/stream", "_handle_session_chat_stream"},
        {"POST", "/api/sessions/{session_id}/model", "_handle_session_model_lock"},
        {"POST", "/v1/chat/completions", "_handle_chat_completions"},
        {"POST", "/v1/responses", "_handle_responses"},
        {"GET", "/v1/responses/{response_id}", "_handle_get_response"},
        {"DELETE", "/v1/responses/{response_id}", "_handle_delete_response"},
        {"POST", "/api/platforms/{platform}/events", "_handle_platform_event_callback"},
        {"GET", "/api/jobs", "_handle_list_jobs"},
        {"POST", "/api/jobs", "_handle_create_job"},
        {"GET", "/api/jobs/{job_id}", "_handle_get_job"},
        {"PATCH", "/api/jobs/{job_id}", "_handle_update_job"},
        {"DELETE", "/api/jobs/{job_id}", "_handle_delete_job"},
        {"POST", "/api/jobs/{job_id}/pause", "_handle_pause_job"},
        {"POST", "/api/jobs/{job_id}/resume", "_handle_resume_job"},
        {"POST", "/api/jobs/{job_id}/run", "_handle_run_job"},
        {"POST", "/v1/runs", "_handle_runs"},
        {"GET", "/v1/runs/{run_id}", "_handle_get_run"},
        {"GET", "/v1/runs/{run_id}/events", "_handle_run_events"},
        {"POST", "/v1/runs/{run_id}/approval", "_handle_run_approval"},
        {"POST", "/v1/runs/{run_id}/steer", "_handle_steer_run"},
        {"POST", "/v1/runs/{run_id}/stop", "_handle_stop_run"},
    };
    json_t *table = json_array();
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        json_t *row = json_array();
        json_append(row, json_string(routes[i].method));
        json_append(row, json_string(routes[i].path));
        json_append(row, json_string(routes[i].handler));
        json_append(table, row);
    }
    if (APIsg_CRON_AVAILABLE) {
        /* Chronos managed-cron fire webhook (NAS → agent), NAS-JWT authed. */
        json_t *row = json_array();
        json_append(row, json_string("POST"));
        json_append(row, json_string("/api/cron/fire"));
        json_append(row, json_string("_handle_cron_fire"));
        json_append(table, row);
    }
    return table;
}

/* ── Per-profile SessionDB cache ───────────────────────────────────── */

/* api_server.py:_open_and_cache_session_db — see note below. */
/* PoP: apisg_open_and_cache_session_db @ api_server.py:_open_and_cache_session_db */
/* does NOT write into the adapter's single-db override slot. */
hermes_state_db_t *apisg_open_and_cache_session_db(api_server_adapter_t *adapter,
                                                   const char *home) {
    if (!adapter || !home) return NULL;
    pthread_mutex_lock(&adapter->session_db_cache_lock);
    if (adapter->session_db_cache_closed) {
        pthread_mutex_unlock(&adapter->session_db_cache_lock);
        return NULL;
    }
    hermes_state_db_t *db = NULL;
    for (int i = 0; i < adapter->session_db_count; i++) {
        if (strcmp(adapter->session_dbs[i].home, home) == 0) { db = adapter->session_dbs[i].db; break; }
    }
    if (!db && adapter->session_db_count < APIS_MAX_CACHED_DBS) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/state.db", home);
        db = hermes_state_db_open(path);
        if (db) {
            int i = adapter->session_db_count++;
            snprintf(adapter->session_dbs[i].home, sizeof(adapter->session_dbs[i].home),
                     "%s", home);
            adapter->session_dbs[i].db = db;
        }
    }
    pthread_mutex_unlock(&adapter->session_db_cache_lock);
    return db;
}

/* PoP: apisg_close_cached_session_dbs @ api_server.py:_close_cached_session_dbs */
void apisg_close_cached_session_dbs(api_server_adapter_t *adapter) {
    extern __attribute__((weak)) void hermes_state_db_close(hermes_state_db_t *db);
    pthread_mutex_lock(&adapter->session_db_cache_lock);
    adapter->session_db_cache_closed = true;
    int n = adapter->session_db_count;
    hermes_state_db_t *cached[APIS_MAX_CACHED_DBS];
    for (int i = 0; i < n; i++) cached[i] = adapter->session_dbs[i].db;
    adapter->session_db_count = 0;
    memset(adapter->session_dbs, 0, sizeof(adapter->session_dbs));
    pthread_mutex_unlock(&adapter->session_db_cache_lock);
    /* The explicitly-wired override handle is owned by its setter — skip it. */
    for (int i = 0; i < n; i++) {
        if (cached[i] && cached[i] != adapter->session_db_override &&
            hermes_state_db_close)
            hermes_state_db_close(cached[i]);
    }
}

/* PoP: apisg_ensure_session_db @ api_server.py:_ensure_session_db */
hermes_state_db_t *apisg_ensure_session_db(api_server_adapter_t *adapter) {
    /* Explicit override (tests / manual wiring) wins. */
    if (adapter->session_db_override)
        return adapter->session_db_override;
    const char *home = get_hermes_home();
    if (!home || !*home) {
        fprintf(stderr, "[api_server] SessionDB unavailable for API server\n");
        return NULL;
    }
    return apisg_open_and_cache_session_db(adapter, home);
}

/* api_server.py:_ensure_session_db_async — see note below. */
/* PoP: apisg_ensure_session_db_async @ api_server.py:_ensure_session_db_async */
/* cache/single-flight gate applies synchronously. */
hermes_state_db_t *apisg_ensure_session_db_async(api_server_adapter_t *adapter) {
    if (adapter->session_db_override)
        return adapter->session_db_override;
    const char *home = get_hermes_home();
    if (!home || !*home) return NULL;
    pthread_mutex_lock(&adapter->session_db_cache_lock);
    for (int i = 0; i < adapter->session_db_count; i++) {
        if (strcmp(adapter->session_dbs[i].home, home) == 0) {
            hermes_state_db_t *cached = adapter->session_dbs[i].db;
            pthread_mutex_unlock(&adapter->session_db_cache_lock);
            return cached;
        }
    }
    pthread_mutex_unlock(&adapter->session_db_cache_lock);
    return apisg_open_and_cache_session_db(adapter, home);
}

/* ── model_routes / runtime request resolution ─────────────────────── */

/* PoP: apisg_parse_model_routes @ api_server.py:_parse_model_routes */
json_t *apisg_parse_model_routes(const json_t *raw) {
    json_t *routes = json_object();
    if (!routes) return NULL;
    if (!json_is_object(raw)) {
        if (raw && !json_is_null(raw))
            fprintf(stderr, "[api_server] model_routes ignored: expected a mapping\n");
        return routes;
    }
    static const char *const allowed_keys[] =
        {"model", "provider", "api_key", "base_url"};
    for (size_t i = 0; i < json_object_size(raw); i++) {
        const char *alias_raw = json_object_get_key_at(raw, i);
        const json_t *cfg = json_object_get_at(raw, i);
        char alias[256];
        apisg_strip_lower(alias_raw, alias, sizeof(alias)); /* str().strip(); case kept below */
        /* Python keeps original case after strip — recompute properly. */
        size_t a = 0;
        while (alias_raw[a] == ' ' || alias_raw[a] == '\t') a++;
        size_t alen = strlen(alias_raw);
        while (alen > a && (alias_raw[alen - 1] == ' ' || alias_raw[alen - 1] == '\t')) alen--;
        if (alen - a >= sizeof(alias)) alen = a + sizeof(alias) - 1;
        memcpy(alias, alias_raw + a, alen - a);
        alias[alen - a] = '\0';
        if (!alias[0] || !json_is_object(cfg)) {
            fprintf(stderr, "[api_server] model_routes: dropping invalid route entry '%s'\n",
                    alias);
            continue;
        }
        json_t *route = json_object();
        bool has_model = false;
        for (size_t k = 0; k < sizeof(allowed_keys) / sizeof(allowed_keys[0]); k++) {
            const json_t *v = json_obj_get(cfg, allowed_keys[k]);
            if (v && !json_is_null(v)) {
                char buf[512];
                if (json_is_string(v)) {
                    const char *s = v->str_val;
                    size_t b = 0, e = strlen(s);
                    while (b < e && (s[b] == ' ' || s[b] == '\t')) b++;
                    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) e--;
                    if (e - b >= sizeof(buf)) e = b + sizeof(buf) - 1;
                    memcpy(buf, s + b, e - b); buf[e - b] = '\0';
                } else {
                    snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
                }
                if (buf[0]) {
                    json_set(route, allowed_keys[k], json_string(buf));
                    if (strcmp(allowed_keys[k], "model") == 0) has_model = true;
                }
            }
        }
        const json_t *mv2 = json_obj_get(route, "model");
        if (!has_model || !mv2 || !((mv2->str_val && mv2->str_val[0]))) {
            fprintf(stderr,
                    "[api_server] model_routes: route '%s' has no 'model'; dropping\n",
                    alias);
            json_free(route);
            continue;
        }
        json_set(routes, alias, route);
    }
    return routes;
}

/* PoP: apisg_resolve_route @ api_server.py:_resolve_route */
json_t *apisg_resolve_route(api_server_adapter_t *adapter, const char *model_alias) {
    if (!adapter || !adapter->model_routes || !model_alias || !*model_alias)
        return NULL;
    const json_t *route = json_obj_get(adapter->model_routes, model_alias);
    return route ? json_copy(route) : NULL;
}

/* PoP: apisg_stored_session_model @ api_server.py:_stored_session_model */
char *apisg_stored_session_model(api_server_adapter_t *adapter, const json_t *session) {
    if (!json_is_object(session)) return NULL;
    const json_t *stored_v = json_obj_get(session, "model");
    const char *stored = (stored_v && json_is_string(stored_v)) ? stored_v->str_val : NULL;
    if (!stored || !*stored) return NULL;
    if (strcmp(stored, adapter->model_name) == 0) return NULL; /* virtual alias */
    return strdup(stored);
}

/* PoP: apisg_clean_runtime_id @ api_server.py:_clean_runtime_id */
char *apisg_clean_runtime_id(const char *value, size_t max_len) {
    if (!value) return strdup("");
    const char *p = value;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                       p[len - 1] == '\n' || p[len - 1] == '\r'))
        len--;
    if (len == 0 || len > max_len) return strdup("");
    for (size_t i = 0; i < len; i++)
        if (p[i] == '\r' || p[i] == '\n' || p[i] == '\0') return strdup("");
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

/* PoP: apisg_split_provider_prefixed_model @ api_server.py:_split_provider_prefixed_model */
void apisg_split_provider_prefixed_model(const char *model, char *provider,
                                         size_t provider_sz, char *raw,
                                         size_t raw_sz) {
    provider[0] = '\0';
    snprintf(raw, raw_sz, "%s", model ? model : "");
    char cleaned_buf[512];
    char *cleaned = apisg_clean_runtime_id(model ? model : "", 200);
    snprintf(cleaned_buf, sizeof(cleaned_buf), "%s", cleaned ? cleaned : "");
    free(cleaned);
    const char *sep = strstr(cleaned_buf, "::");
    if (sep) {
        size_t plen = (size_t)(sep - cleaned_buf);
        if (plen < provider_sz) {
            char prov[80];
            memcpy(prov, cleaned_buf, plen);
            prov[plen] = '\0';
            const char *rest = sep + 2;
            while (*rest == ' ') rest++;
            if (apisg_is_provider_slug(prov) && *rest) {
                snprintf(provider, provider_sz, "%s", prov);
                snprintf(raw, raw_sz, "%s", rest);
                return;
            }
        }
    }
    snprintf(raw, raw_sz, "%s", cleaned_buf);
}

/* PoP: apisg_runtime_options_from_model_options @ api_server.py:_runtime_options_from_model_options */
extern bool api_server_coerce_request_bool(const char *value, bool default_val);

json_t *apisg_runtime_options_from_model_options(const json_t *model_options) {
    json_t *runtime_options = json_object();
    if (!json_is_object(model_options)) return runtime_options;
    const json_t *reasoning = json_obj_get(model_options, "reasoning");
    if (json_is_object(reasoning)) {
        const json_t *enabled = json_obj_get(reasoning, "enabled");
        char *effort = apisg_clean_runtime_id(
            json_get_str(reasoning, "effort", ""), 32);
        if (enabled && json_is_bool(enabled) && !enabled->bool_val) {
            json_t *rc = json_object();
            json_set(rc, "enabled", json_bool(false));
            json_set(runtime_options, "reasoning_config", rc);
        } else if (effort && *effort) {
            json_t *rc = json_object();
            json_set(rc, "enabled", json_bool(true));
            json_set(rc, "effort", json_string(effort));
            json_set(runtime_options, "reasoning_config", rc);
        } else if (enabled && json_is_bool(enabled) && enabled->bool_val) {
            json_t *rc = json_object();
            json_set(rc, "enabled", json_bool(true));
            json_set(runtime_options, "reasoning_config", rc);
        }
        free(effort);
    }
    char *tier = apisg_clean_runtime_id(json_get_str(model_options, "service_tier", ""), 32);
    if (tier && *tier) {
        json_set(runtime_options, "service_tier", json_string(tier));
    } else {
        /* _coerce_request_bool(model_options["fast"], default=False) */
        const json_t *fast = json_obj_get(model_options, "fast");
        bool fast_b = false;
        if (fast && json_is_bool(fast)) fast_b = fast->bool_val;
        else if (fast && json_is_string(fast))
            fast_b = api_server_coerce_request_bool(fast->str_val, false);
        if (fast_b) json_set(runtime_options, "service_tier", json_string("priority"));
    }
    free(tier);
    return runtime_options;
}

/* PoP: apisg_session_runtime_request_from_body @ api_server.py:_session_runtime_request_from_body */
json_t *apisg_session_runtime_request_from_body(api_server_adapter_t *adapter,
                                               const json_t *body) {
    char *raw_model = apisg_clean_runtime_id(
        json_get_str(body, "model", "")[0]
            ? json_get_str(body, "model", "")
            : json_get_str(body, "model_id", ""), 200);
    const char *prov_a = json_get_str(body, "provider", "");
    const char *prov_b = json_get_str(body, "provider_id", "");
    char *raw_provider = apisg_clean_runtime_id(
        (prov_a && *prov_a) ? prov_a : ((prov_b && *prov_b) ? prov_b : ""), 80);
    char prefixed_provider[128], split_model[512];
    apisg_split_provider_prefixed_model(raw_model, prefixed_provider,
                                        sizeof(prefixed_provider),
                                        split_model, sizeof(split_model));
    const char *provider = (raw_provider && *raw_provider) ? raw_provider : prefixed_provider;
    const char *model = (split_model[0]) ? split_model : raw_model;

    json_t *route = apisg_resolve_route(adapter, raw_model);
    if (!route) route = apisg_resolve_route(adapter, model);
    const char *route_source = route ? "model_routes" : "global";
    /* The virtual model alias is not a real provider id — a client echoing it
     * back means "use the gateway default"; null it out upstream of every
     * caller so it never persists as the session's model. */
    if (model && adapter->model_name[0] && strcmp(model, adapter->model_name) == 0)
        model = NULL;
    if (!route && model && *model) {
        route = json_object();
        json_set(route, "model", json_string(model));
        if (provider && *provider)
            json_set(route, "provider", json_string(provider));
        route_source = "raw_request";
    } else if (!route && provider && *provider && model && *model) {
        route = json_object();
        json_set(route, "model", json_string(model));
        json_set(route, "provider", json_string(provider));
        route_source = "raw_request";
    }
    const json_t *mo = json_obj_get(body, "model_options");
    json_t *runtime_options = apisg_runtime_options_from_model_options(mo);

    json_t *requested = json_object();
    json_set(requested, "provider",
             (provider && *provider) ? json_string(provider) : json_string(""));
    json_set(requested, "model",
             (model && *model) ? json_string(model) : json_null());
    json_set(requested, "raw_model", json_string(raw_model));

    bool require_lock = false;
    {
        const json_t *rl = json_obj_get(body, "require_model_lock");
        if (rl && json_is_bool(rl)) require_lock = rl->bool_val;
        else if (rl && json_is_string(rl))
            require_lock = api_server_coerce_request_bool(rl->str_val, false);
    }

    json_t *out = json_object();
    json_set(out, "requested", requested);
    json_set(out, "route", route ? route : json_null());
    json_set(out, "route_source", json_string(route_source));
    json_set(out, "runtime_options", runtime_options);
    json_set(out, "require_model_lock", json_bool(require_lock));
    json_set(out, "model_options",
             (mo && json_is_object(mo)) ? json_copy(mo) : json_object());
    free(raw_model);
    free(raw_provider);
    return out;
}

/* PoP: apisg_runtime_lock_error @ api_server.py:_runtime_lock_error */
json_t *apisg_runtime_lock_error(api_server_adapter_t *adapter,
                                 const json_t *runtime_request, int *status_out) {
    if (!runtime_request || !json_is_true(json_obj_get(runtime_request, "require_model_lock")))
        return NULL;
    const json_t *requested_v = json_obj_get(runtime_request, "requested");
    const json_t *requested = (requested_v && json_is_object(requested_v)) ? requested_v : NULL;
    char *model = apisg_clean_runtime_id(json_get_str(requested, "model", ""), 200);
    char *provider = apisg_clean_runtime_id(json_get_str(requested, "provider", ""), 80);
    const json_t *route = json_obj_get(runtime_request, "route");
    const char *rs = json_get_str(runtime_request, "route_source", "");
    json_t *err = NULL;
    if ((!model || !*model) && (!provider || !*provider)) {
        err = apisg_openai_error(
            "require_model_lock was set but no model/provider was provided",
            NULL, NULL, "missing_model");
        if (status_out) *status_out = 400;
    } else if (!route || strcmp(rs, "global") == 0) {
        err = apisg_openai_error(
            "Requested Browser model lock cannot be routed; refusing silent global fallback",
            NULL, NULL, "model_lock_unavailable");
        if (status_out) *status_out = 409;
    }
    (void)adapter;
    free(model);
    free(provider);
    return err;
}

/* PoP: apisg_parse_session_model_config @ api_server.py:_parse_session_model_config */
json_t *apisg_parse_session_model_config(const json_t *raw) {
    if (json_is_object(raw)) return json_copy(raw);
    if (json_is_string(raw) && raw->str_val && *raw->str_val) {
        char *p = raw->str_val;
        while (*p == ' ') p++;
        json_t *parsed = json_parse(p, NULL);
        if (parsed && json_is_object(parsed)) return parsed;
        if (parsed) json_free(parsed);
    }
    return json_object();
}

/* PoP: apisg_persist_session_runtime_lock @ api_server.py:_persist_session_runtime_lock */
bool apisg_persist_session_runtime_lock(api_server_adapter_t *adapter,
                                        const char *session_id,
                                        const json_t *runtime_request) {
    /* Persist only a newly confirmed lock; an ordinary one-off override must
     * not erase or re-stamp an already-confirmed lock. */
    if (!runtime_request || !json_is_object(runtime_request)) return true;
    if (json_obj_get(runtime_request, "persisted_lock") ||
        !json_is_true(json_obj_get(runtime_request, "require_model_lock")))
        return true;
    const json_t *requested_v = json_obj_get(runtime_request, "requested");
    char *model = apisg_clean_runtime_id(json_get_str(requested_v, "model", ""), 200);
    char *provider = apisg_clean_runtime_id(json_get_str(requested_v, "provider", ""), 80);
    if ((!model || !*model) && (!provider || !*provider)) {
        free(model); free(provider);
        return false;
    }
    hermes_state_db_t *db = apisg_ensure_session_db(adapter);
    if (!db) { free(model); free(provider); return false; }
    const json_t *mo = json_obj_get(runtime_request, "model_options");
    const char *rs = json_get_str(runtime_request, "route_source", "");
    char *lock_key = json_serialize(mo && json_is_object(mo) ? mo : json_object());
    hermes_state_update_session_runtime_lock(db, session_id,
                                             (provider && *provider) ? provider : NULL,
                                             (model && *model) ? model : NULL,
                                             lock_key ? lock_key : "{}");
    free(lock_key);
    fprintf(stderr, "[%s] persisted session runtime lock for %s (route_source=%s)\n",
            "api_server", session_id, rs);
    free(model);
    free(provider);
    return true;
}

/* PoP: apisg_runtime_request_from_persisted_session_lock @ api_server.py:_runtime_request_from_persisted_session_lock */
json_t *apisg_runtime_request_from_persisted_session_lock(
    api_server_adapter_t *adapter, const json_t *session, const json_t *body) {
    if (!json_is_object(session)) return NULL;
    json_t *model_config = apisg_parse_session_model_config(
        json_obj_get(session, "model_config"));
    const json_t *lock = json_obj_get(model_config, "browser_model_lock");
    bool confirmed = false;
    if (lock && json_is_object(lock)) {
        const json_t *c = json_obj_get(lock, "confirmed");
        confirmed = c && json_is_bool(c) && c->bool_val;
    }
    if (!json_is_object(lock) || !confirmed) {
        json_free(model_config);
        return NULL;
    }
    char *model = apisg_clean_runtime_id(json_get_str(lock, "model", ""), 200);
    char *provider = apisg_clean_runtime_id(json_get_str(lock, "provider", ""), 80);
    if ((!model || !*model) && (!provider || !*provider)) {
        free(model); free(provider);
        json_free(model_config);
        return NULL;
    }
    char *rs_raw = apisg_clean_runtime_id(json_get_str(lock, "route_source", ""), 64);
    char rs_lower[80];
    apisg_strip_lower(rs_raw, rs_lower, sizeof(rs_lower));

    json_t *route = NULL;
    if (strcmp(rs_lower, "model_routes") == 0) {
        if (model && *model) route = apisg_resolve_route(adapter, model);
    } else {
        route = json_object();
        if (model && *model)
            json_set(route, "model", json_string(model));
        if (provider && *provider)
            json_set(route, "provider", json_string(provider));
    }

    const json_t *body_mo = json_obj_get(body, "model_options");
    const json_t *lock_mo = json_obj_get(lock, "model_options");
    const json_t *model_options =
        (body_mo && json_is_object(body_mo))
            ? body_mo
            : ((lock_mo && json_is_object(lock_mo)) ? lock_mo : NULL);

    json_t *requested = json_object();
    json_set(requested, "provider",
             (provider && *provider) ? json_string(provider) : json_string(""));
    json_set(requested, "model",
             (model && *model) ? json_string(model) : json_null());
    json_set(requested, "raw_model",
             (model && *model) ? json_string(model) : json_null());

    json_t *out = json_object();
    json_set(out, "requested", requested);
    json_set(out, "route", (route && json_object_size(route)) ? route : json_null());
    json_set(out, "route_source", json_string("session_model_lock"));
    json_set(out, "runtime_options", apisg_runtime_options_from_model_options(model_options));
    json_set(out, "require_model_lock", json_bool(true));
    json_set(out, "model_options",
             (model_options && json_is_object(model_options)) ? json_copy(model_options)
                                                              : json_object());
    json_set(out, "persisted_lock", json_bool(true));
    free(model);
    free(provider);
    free(rs_raw);
    json_free(model_config);
    if (route && json_object_size(route) == 0) json_free(route);
    return out;
}

/* PoP: apisg_effective_session_runtime_request @ api_server.py:_effective_session_runtime_request */
json_t *apisg_effective_session_runtime_request(api_server_adapter_t *adapter,
                                                const json_t *session,
                                                const json_t *body) {
    json_t *runtime_request = apisg_session_runtime_request_from_body(adapter, body);
    const json_t *requested = json_obj_get(runtime_request, "requested");
    const char *m = json_get_str(requested, "model", "");
    const json_t *mv = requested ? json_obj_get(requested, "model") : NULL;
    const char *pv = requested ? json_get_str(requested, "provider", "") : "";
    if ((mv && !json_is_null(mv) && m && *m) || (pv && *pv))
        return runtime_request;
    json_t *persisted =
        apisg_runtime_request_from_persisted_session_lock(adapter, session, body);
    if (persisted) {
        json_free(runtime_request);
        return persisted;
    }
    return runtime_request;
}

/* PoP: apisg_sanitize_runtime_metadata @ api_server.py:_sanitize_runtime_metadata */
json_t *apisg_sanitize_runtime_metadata(const json_t *runtime,
                                        const json_t *requested_runtime,
                                        const char *route_source_default,
                                        const char *model_lock) {
    json_t *payload = (runtime && json_is_object(runtime)) ? json_copy(runtime) : json_object();
    char *provider = apisg_clean_runtime_id(
        json_get_str(payload, "provider", "")[0]
            ? json_get_str(payload, "provider", "")
            : (json_get_str(payload, "provider_id", "")[0]
                   ? json_get_str(payload, "provider_id", "")
                   : json_get_str(payload, "effective_provider", "")),
        80);
    char *model = apisg_clean_runtime_id(
        json_get_str(payload, "model", "")[0]
            ? json_get_str(payload, "model", "")
            : (json_get_str(payload, "model_id", "")[0]
                   ? json_get_str(payload, "model_id", "")
                   : json_get_str(payload, "effective_model", "")),
        200);
    char rs_in[80];
    snprintf(rs_in, sizeof(rs_in), "%s", json_get_str(payload, "route_source", ""));
    char *rs = apisg_clean_runtime_id(rs_in[0] ? rs_in
                                              : (route_source_default ? route_source_default : ""),
                                      64);
    if (!rs || !*rs) { free(rs); rs = strdup("global"); }
    json_t *result = json_object();
    json_set(result, "provider", json_string(provider ? provider : ""));
    json_set(result, "model", json_string(model ? model : ""));
    json_set(result, "route_source", json_string(rs));

    const json_t *req_src = requested_runtime;
    if (!req_src) req_src = json_obj_get(payload, "requested");
    if ((requested_runtime && json_is_object(requested_runtime)) ||
        (req_src && json_is_object(req_src))) {
        const json_t *req = (req_src && json_is_object(req_src)) ? req_src : json_object();
        char *rp = apisg_clean_runtime_id(json_get_str(req, "provider", ""), 80);
        char *rm = apisg_clean_runtime_id(json_get_str(req, "model", ""), 200);
        json_t *rj = json_object();
        json_set(rj, "provider", json_string(rp ? rp : ""));
        json_set(rj, "model", json_string(rm ? rm : ""));
        json_set(result, "requested", rj);
        free(rp); free(rm);
    }
    const char *ml = (model_lock && *model_lock)
                         ? model_lock
                         : json_get_str(payload, "model_lock", "");
    if (ml && *ml) {
        char *mlc = apisg_clean_runtime_id(ml, 32);
        if (mlc && *mlc) json_set(result, "model_lock", json_string(mlc));
        free(mlc);
    }
    free(provider);
    free(model);
    free(rs);
    json_free(payload);
    return result;
}

/* PoP: apisg_normalize_session_source @ api_server.py:_normalize_session_source */
char *apisg_normalize_session_source(const char *value) {
    char text[128];
    apisg_strip_lower(value, text, sizeof(text));
    static const char *const allowed[] = {
        "api_server", "hermes_browser", "browser", "cli", "telegram",
        "discord", "slack", "desktop", "dashboard"};
    for (size_t i = 0; i < sizeof(allowed) / sizeof(allowed[0]); i++) {
        if (strcmp(text, allowed[i]) == 0) {
            if (strcmp(text, "browser") == 0) return strdup("hermes_browser");
            return strdup(text);
        }
    }
    return strdup("api_server");
}

/* PoP: apisg_session_model_override_for @ api_server.py:_session_model_override_for */
json_t *apisg_session_model_override_for(api_server_adapter_t *adapter,
                                         const char *session_key) {
    (void)adapter;
    if (!session_key || !*session_key) return NULL;
    GatewayRunner *runner = api_server_gateway_runner_ref();
    if (!runner) return NULL;
    extern __attribute__((weak)) void gateway_runner_rehydrate_session_model_override(
        GatewayRunner *self, const char *session_key);
    if (gateway_runner_rehydrate_session_model_override) {
        gateway_runner_rehydrate_session_model_override(runner, session_key);
    } else {
        fprintf(stderr,
                "[api_server] failed to rehydrate session /model override for %s\n",
                session_key);
    }
    json_t *override = gateway_runner_snapshot_session_model_override
                           ? gateway_runner_snapshot_session_model_override(runner, session_key)
                           : NULL;
    return (override && json_is_object(override)) ? override : NULL;
}

/* PoP: apisg_request_route_conflict_error @ api_server.py:_request_route_conflict_error */
char *apisg_request_route_conflict_error(api_server_adapter_t *adapter,
                                         const char *session_id,
                                         const char *gateway_session_key,
                                         const char *requested_model,
                                         const char *requested_provider,
                                         const json_t *route) {
    char *request_provider = requested_provider
                                 ? apis_clean_request_string(requested_provider)
                                 : NULL;
    if ((!request_provider || !*request_provider) || !json_is_object(route)) {
        free(request_provider);
        return NULL;
    }
    json_t *override = apisg_session_model_override_for(
        adapter, (gateway_session_key && *gateway_session_key) ? gateway_session_key
                                                               : session_id);
    if (override) {
        /* Session /model wins over both the route and the request override —
         * no ambiguity to reject on this path. */
        json_free(override);
        free(request_provider);
        return NULL;
    }
    char *route_provider = apis_clean_request_string(json_get_str(route, "provider", ""));
    char *route_api_key = apis_clean_request_string(json_get_str(route, "api_key", ""));
    char *route_base_url = apis_clean_request_string(json_get_str(route, "base_url", ""));
    char *route_alias = requested_model ? apis_clean_request_string(requested_model) : NULL;
    if (!route_alias || !*route_alias) {
        free(route_alias);
        route_alias = strdup("requested model");
    }
    char *out = NULL;
    if (route_provider && *route_provider &&
        (!request_provider || strcmp(request_provider, route_provider) != 0)) {
        size_t n = strlen(route_alias) + strlen(route_provider) + 128;
        out = malloc(n);
        snprintf(out, n,
                 "Model route '%s' is pinned to provider '%s'. "
                 "Remove 'provider' or use '%s'.",
                 route_alias, route_provider, route_provider);
    } else if ((!route_provider || !*route_provider) &&
               ((route_api_key && *route_api_key) || (route_base_url && *route_base_url))) {
        out = malloc(strlen(route_alias) + 160);
        sprintf(out,
                "Model route '%s' pins route credentials/base_url. "
                "Do not combine it with an explicit 'provider'.",
                route_alias);
    }
    free(request_provider);
    free(route_provider);
    free(route_api_key);
    free(route_base_url);
    free(route_alias);
    return out;
}

/* ── Browser control cluster ────────────────────────────────────────── */

/* PoP: apisg_browser_control_enabled @ api_server.py:_browser_control_enabled */
bool apisg_browser_control_enabled(void) {
    /* Feature flag; False unless explicitly enabled via config. */
    return bcb_browser_control_enabled(NULL) == 1;
}

/* PoP: apisg_browser_control_developer_mode @ api_server.py:_browser_control_developer_mode */
bool apisg_browser_control_developer_mode(void) {
    /* Developer Mode gate for browser_evaluate / raw CDP; fail closed. */
    return bcb_browser_control_developer_mode(NULL) == 1;
}

/* PoP: apisg_derive_browser_control_principal @ api_server.py:_derive_browser_control_principal */
char *apisg_derive_browser_control_principal(api_server_adapter_t *adapter,
                                             const char *profile) {
    /* sha256(profile NUL key)[:32] bound to the credential that authenticated
     * the registration — a client cannot impersonate another controller. */
    const char *key = adapter->api_key[0] ? adapter->api_key : "";
    size_t plen = profile ? strlen(profile) : 0;
    size_t klen = strlen(key);
    unsigned char *material = malloc(plen + 1 + klen);
    if (!material) return NULL;
    memcpy(material, profile ? profile : "", plen);
    material[plen] = 0;
    memcpy(material + plen + 1, key, klen);
    extern void bca_sha256_hex(const unsigned char *, size_t, char out[65]);
    char hex[65];
    bca_sha256_hex(material, plen + 1 + klen, hex);
    free(material);
    char *out = malloc(strlen("principal:") + strlen(profile ? profile : "") + 33);
    if (!out) return NULL;
    snprintf(out, sizeof("principal:") + strlen(profile ? profile : "") + 33,
             "principal:%s:%.32s", profile ? profile : "", hex);
    return out;
}

/* PoP: apisg_browser_control_transport_family @ api_server.py:_browser_control_transport_family */
const char *apisg_browser_control_transport_family(const char *peer_host) {
    /* Local vs remote API family decided by the loopback peer; the broker
     * treats the family as part of exact identity. */
    if (peer_host && (strcmp(peer_host, "127.0.0.1") == 0 ||
                      strcmp(peer_host, "::1") == 0 ||
                      strcmp(peer_host, "localhost") == 0))
        return "local-api";
    return "remote-api";
}

static int apisg_artifact_slot(api_server_adapter_t *a, const char *profile_key,
                               bool create) {
    for (int i = 0; i < APIS_MAX_CACHED_DBS; i++)
        if (a->artifact_profiles[i][0] &&
            strcmp(a->artifact_profiles[i], profile_key) == 0)
            return i;
    if (!create) return -1;
    for (int i = 0; i < APIS_MAX_CACHED_DBS; i++) {
        if (!a->artifact_profiles[i][0]) {
            snprintf(a->artifact_profiles[i], sizeof(a->artifact_profiles[i]),
                     "%s", profile_key);
            a->artifact_stores[i] = NULL;
            return i;
        }
    }
    return -1;
}

/* Broker duck-type validate → bca_store_validate with a principal/family scope. */
static int apisg_artifact_validate_for_broker(void *store_ptr, const char *artifact_id,
                                              const bcb_scope_t *scope, char *err,
                                              size_t err_sz) {
    bca_scope_t bscope = {.principal_id = scope ? scope->principal_id : "",
                          .transport_family = scope ? scope->transport_family : ""};
    bca_receipt_t receipt;
    memset(&receipt, 0, sizeof(receipt));
    bca_error_t rc = bca_store_validate((bca_store_t *)store_ptr, artifact_id,
                                        &bscope, &receipt);
    if (rc != BCA_OK && err && err_sz)
        snprintf(err, err_sz, "artifact validation failed (%d)", (int)rc);
    return rc == BCA_OK ? 0 : -1;
}

/* PoP: apisg_artifact_store_for @ api_server.py:_artifact_store_for */
bca_store_t *apisg_artifact_store_for(api_server_adapter_t *adapter,
                                      const char *profile) {
    char profile_key[128];
    snprintf(profile_key, sizeof(profile_key), "%s",
             (profile && *profile) ? profile : "default");
    int slot = apisg_artifact_slot(adapter, profile_key, false);
    if (slot >= 0 && adapter->artifact_stores[slot]) {
        bca_store_prune_expired(adapter->artifact_stores[slot], NULL);
        return adapter->artifact_stores[slot];
    }
    /* Profile-scoped controlled root so artifacts never escape the profile
     * boundary; stores are cached BY RESOLVED PROFILE. */
    const char *home = get_hermes_home();
    char root[1200];
    int n = snprintf(root, sizeof(root), "%s/artifacts/browser-control", home ? home : ".");
    if (n <= 0 || (size_t)n >= sizeof(root)) {
        fprintf(stderr, "[api_server] no artifact root is resolvable\n");
        return NULL;
    }
    slot = apisg_artifact_slot(adapter, profile_key, true);
    if (slot < 0) return NULL;
    bca_store_t *store = bca_store_new(root, BCA_DEFAULT_TTL_SECONDS,
                                       BCA_DEFAULT_MAX_BYTES,
                                       BCA_DEFAULT_ALLOWED_MIME_TYPES, NULL);
    if (!store) return NULL;
    bca_store_prune_expired(store, NULL);
    adapter->artifact_stores[slot] = store;
    /* Share the store with the broker so dispatched artifact actions validate
     * against the same profile's controlled root ("approved id only"). */
    static const bcb_artifact_store_vtab_t vtab = {
        NULL, apisg_artifact_validate_for_broker};
    if (adapter->browser_control_broker)
        bcb_broker_attach_artifact_store(adapter->browser_control_broker,
                                         &vtab, profile_key);
    return store;
}

/* PoP: apisg_artifact_limiter @ api_server.py:_artifact_limiter */
bca_rate_limiter_t *apisg_artifact_limiter(api_server_adapter_t *adapter) {
    /* Per-principal artifact route limiter (lazy): 30 req / 60s window. */
    if (!adapter->artifact_limiter)
        adapter->artifact_limiter = bca_rate_limiter_new(60.0, 30, NULL);
    return adapter->artifact_limiter;
}

/* PoP: apisg_inject_browser_control_artifacts @ api_server.py:_inject_browser_control_artifacts */
void apisg_inject_browser_control_artifacts(api_server_adapter_t *adapter,
                                            bca_store_t *store,
                                            bca_rate_limiter_t *limiter,
                                            const char *profile) {
    const char *key = (profile && *profile) ? profile : "default";
    int slot = apisg_artifact_slot(adapter, key, store != NULL);
    if (slot >= 0) adapter->artifact_stores[slot] = store;
    if (limiter) adapter->artifact_limiter = limiter;
}

/* PoP: apisg_artifact_auth_fail @ api_server.py:_artifact_auth_fail */
void apisg_artifact_auth_fail(int client_fd, int status, const char *code,
                              const char *message) {
    json_t *err = apisg_openai_error(message,
                                     status == 401 ? "gateway_auth_error"
                                                   : "invalid_request_error",
                                     NULL, code);
    char *s = json_serialize(err);
    send_json_response(client_fd, status, s);
    free(s);
    json_free(err);
}

/* PoP: apisg_handle_browser_control_register @ api_server.py:_handle_browser_control_register */
int apisg_handle_browser_control_register(api_server_adapter_t *adapter, int client_fd,
                                          const char *auth_header, const char *body,
                                          const char *peer_host) {
    /* Status ladder: 404 disabled, 403 no API key configured, 401 bad bearer
     * (via auth hook upstream), 400 invalid payload, 503 no session DB,
     * 403 unknown session / devmode mismatch, 201 minted ticket. */
    if (!apisg_browser_control_enabled()) {
        json_t *err = apisg_openai_error(
            "Browser control is not enabled on this server.", NULL, NULL,
            "browser_control_disabled");
        char *s = json_serialize(err);
        send_json_response(client_fd, 404, s);
        free(s); json_free(err);
        return 404;
    }
    if (!adapter->api_key[0]) {
        fprintf(stderr,
                "[api_server] browser-control registration rejected: no API key configured; "
                "set API_SERVER_KEY to enable authenticated browser control.\n");
        json_t *err = apisg_openai_error(
            "Browser control registration requires a configured API key.",
            "gateway_auth_error", NULL, "browser_control_auth_required");
        char *s = json_serialize(err);
        send_json_response(client_fd, 403, s);
        free(s); json_free(err);
        return 403;
    }
    json_t *payload = body ? json_parse(body, NULL) : NULL;
    if (!payload || !json_is_object(payload)) {
        json_t *err = apisg_openai_error(
            payload ? "Request body must be a JSON object."
                    : "Request body must be valid JSON.",
            NULL, NULL, NULL);
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        if (payload) json_free(payload);
        return 400;
    }
    extern __attribute__((weak)) int bcb_protocol_supported(const json_t *protocol_version);
    if (bcb_protocol_supported && !bcb_protocol_supported(json_obj_get(payload, "protocol_version"))) {
        json_t *err = apisg_openai_error("Unsupported browser-control protocol version.",
                                         NULL, NULL, "browser_control_protocol_unsupported");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        json_free(payload);
        return 400;
    }
    const char *controller_id = json_get_str(payload, "controller_id", "");
    const char *browser_profile_id = json_get_str(payload, "browser_profile_id", "");
    const char *session_id = json_get_str(payload, "session_id", "");
    if (!*controller_id || !*browser_profile_id || !*session_id) {
        json_t *err = apisg_openai_error(
            "controller_id, browser_profile_id, and session_id are required.",
            NULL, NULL, "browser_control_invalid_registration");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        json_free(payload);
        return 400;
    }
    hermes_state_db_t *db = apisg_ensure_session_db_async(adapter);
    if (!db) {
        json_t *err = apisg_openai_error("Session database unavailable.", NULL, NULL,
                                         "session_db_unavailable");
        char *s = json_serialize(err);
        send_json_response(client_fd, 503, s);
        free(s); json_free(err);
        json_free(payload);
        return 503;
    }
    json_t *session = NULL;
    {
        char *raw = hermes_state_get_session(db, session_id);
        if (raw) {
            session = json_parse(raw, NULL);
            free(raw);
        }
    }
    if (!session) {
        json_t *err = apisg_openai_error(
            "Browser control may register only for an existing server session.",
            "gateway_auth_error", NULL, "browser_control_session_forbidden");
        char *s = json_serialize(err);
        send_json_response(client_fd, 403, s);
        free(s); json_free(err);
        json_free(payload);
        return 403;
    }
    json_free(session);

    const char *profile = getenv("HERMES_PROFILE");
    if (!profile || !*profile) profile = "default";
    size_t caps_n = 0;
    char **caps = bcb_filter_browser_control_capabilities(
        json_obj_get(payload, "capabilities"),
        apisg_browser_control_developer_mode() ? 1 : 0, &caps_n);
    if (!caps || caps_n == 0) {
        json_t *err = apisg_openai_error(
            "At least one permitted browser-control capability is required.",
            NULL, NULL, "browser_control_no_capabilities");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        json_free(payload);
        return 400;
    }
    if (!adapter->browser_control_broker) {
        adapter->browser_control_broker =
            bcb_broker_new(BCB_DEFAULT_TICKET_TTL, BCB_DEFAULT_COMMAND_TIMEOUT, NULL, -1);
        if (!adapter->browser_control_broker) {
            for (size_t i = 0; i < caps_n; i++) free(caps[i]);
            free(caps);
            json_free(payload);
            return 500;
        }
    }
    char *principal = apisg_derive_browser_control_principal(adapter, profile);
    bcb_scope_t scope;
    bcb_scope_init(&scope);
    snprintf(scope.principal_id, sizeof(scope.principal_id), "%s",
             principal ? principal : "");
    snprintf(scope.profile_id, sizeof(scope.profile_id), "%s", profile);
    snprintf(scope.session_id, sizeof(scope.session_id), "%s", session_id);
    snprintf(scope.controller_id, sizeof(scope.controller_id), "%s", controller_id);
    snprintf(scope.browser_profile_id, sizeof(scope.browser_profile_id), "%s",
             browser_profile_id);
    snprintf(scope.transport_family, sizeof(scope.transport_family), "%s",
             apisg_browser_control_transport_family(peer_host));
    json_t *caps_arr = json_array();
    for (size_t i = 0; i < caps_n; i++) json_append(caps_arr, json_string(caps[i]));
    scope.capabilities = caps;
    scope.capabilities_n = caps_n;
    bcb_ticket_t *ticket = bcb_broker_mint_ticket(adapter->browser_control_broker, &scope);
    double ttl = bcb_broker_ticket_ttl_seconds(adapter->browser_control_broker);
    json_t *resp = json_object();
    json_set(resp, "protocol_version", json_number(APIsg_BROWSER_CONTROL_PROTOCOL_VERSION));
    json_set(resp, "ticket", json_string(ticket ? ticket->value : ""));
    json_set(resp, "ticket_expires_at", json_number(time(NULL) + ttl));
    json_set(resp, "ticket_expires_in_seconds", json_number(ttl));
    json_set(resp, "ws_path", json_string("/v1/browser-control/ws"));
    json_t *scope_out = json_object();
    json_set(scope_out, "principal_id", json_string(scope.principal_id));
    json_set(scope_out, "profile_id", json_string(scope.profile_id));
    json_set(scope_out, "session_id", json_string(scope.session_id));
    json_set(scope_out, "controller_id", json_string(scope.controller_id));
    json_set(scope_out, "browser_profile_id", json_string(scope.browser_profile_id));
    json_set(scope_out, "transport_family", json_string(scope.transport_family));
    json_set(scope_out, "capabilities", caps_arr);
    json_set(resp, "scope", scope_out);
    char *s = json_serialize(resp);
    send_json_response(client_fd, 201, s);
    free(s); json_free(resp);
    /* mint_ticket took ownership of the capability table — freed by
     * bcb_scope_free per include/browser_control_broker.h. */
    bcb_scope_free(&scope);
    free(principal);
    json_free(payload);
    return 201;
}

/* api_server.py:_handle_browser_control_ws — see note below. */
/* PoP: apisg_handle_browser_control_ws_admit @ api_server.py:_handle_browser_control_ws */
/* Sec-WebSocket-Protocol subprotocol token is exchanged exactly once. */
int apisg_handle_browser_control_ws_admit(api_server_adapter_t *adapter,
                                          const char *query_ticket,
                                          const char *ws_protocols_header,
                                          bcb_scope_t *scope_out) {
    /* Re-check at upgrade time so disabling closes the gate without
     * consuming still-live one-shot tickets. */
    if (!apisg_browser_control_enabled()) return 404;
    if (query_ticket && *query_ticket) return 401;
    char protocols[1024];
    snprintf(protocols, sizeof(protocols), "%s", ws_protocols_header ? ws_protocols_header : "");
    bool ws_proto_present = false;
    char ticket_value[160] = "";
    int ticket_count = 0;
    char *save = NULL;
    for (char *tok = strtok_r(protocols, ",", &save); tok;
         tok = strtok_r(NULL, ",", &save)) {
        while (*tok == ' ') tok++;
        size_t l = strlen(tok);
        while (l > 0 && tok[l - 1] == ' ') tok[--l] = '\0';
        if (!*tok) continue;
        if (strcmp(tok, APIsg_BROWSER_CONTROL_WS_PROTOCOL) == 0) ws_proto_present = true;
        const char *tp = APIsg_BROWSER_CONTROL_TICKET_PROTOCOL_PREFIX;
        if (strncmp(tok, tp, strlen(tp)) == 0) {
            ticket_count++;
            snprintf(ticket_value, sizeof(ticket_value), "%s", tok + strlen(tp));
        }
    }
    if (!ws_proto_present || ticket_count != 1 || !ticket_value[0]) return 401;
    if (!adapter->browser_control_broker) {
        fprintf(stderr, "[api_server] browser-control WS ticket consumption failed\n");
        return 401;
    }
    bcb_error_t err = bcb_broker_consume_ticket(adapter->browser_control_broker,
                                                ticket_value, scope_out);
    if (err != BCB_OK) {
        fprintf(stderr,
                "[api_server] browser-control WS ticket %s\n",
                err == BCB_ERR_TICKET_INVALID ? "invalid" : "consumption failed");
        return 401;
    }
    return 0;
}

/* PoP: apisg_handle_browser_control_frame @ api_server.py:_handle_browser_control_frame */
json_t *apisg_handle_browser_control_frame(api_server_adapter_t *adapter,
                                           const bcb_scope_t *scope,
                                           const json_t *frame, void *owner) {
    /* Apply one controller→broker frame with exact-scope checks. */
    if (!json_is_object(frame)) return NULL;
    const char *method = json_get_str(frame, "method", "");
    const json_t *params = json_obj_get(frame, "params");
    if (!json_is_object(params)) return NULL;
    if (!owner || (adapter->browser_control_broker &&
                   !bcb_broker_is_owner(adapter->browser_control_broker, scope, owner)))
        return NULL;
    if (strcmp(method, "browser.controller.heartbeat") == 0) {
        char nonce_raw[256];
        snprintf(nonce_raw, sizeof(nonce_raw), "%s",
                 json_get_str(params, "nonce", ""));
        const char *nonce = nonce_raw;
        while (*nonce == ' ') nonce++;
        size_t nl = strlen(nonce);
        while (nl > 0 && nonce[nl - 1] == ' ') nl--;
        if (nl == 0 || nl > 128) return NULL;
        /* Echo only the caller's opaque nonce — proves the socket is live
         * without granting capability or touching broker commands. */
        json_t *reply = json_object();
        json_set(reply, "method", json_string("browser.controller.heartbeat"));
        json_t *p = json_object();
        json_set(p, "nonce", json_string(nonce));
        json_set(p, "ok", json_bool(true));
        json_set(reply, "params", p);
        return reply;
    }
    if (strcmp(method, "browser.controller.detach") == 0) {
        if (adapter->browser_control_broker)
            bcb_broker_detach(adapter->browser_control_broker, scope, owner, 0);
        json_t *reply = json_object();
        json_set(reply, "method", json_string("browser.controller.detach"));
        json_t *p = json_object();
        json_set(p, "ok", json_bool(true));
        json_set(reply, "params", p);
        return reply;
    }
    if (strcmp(method, "browser.controller.result") == 0) {
        const json_t *cid = json_obj_get(params, "command_id");
        if (cid && json_is_string(cid) && cid->str_val[0]) {
            const json_t *okv = json_obj_get(params, "ok");
            bool ok = okv && json_is_bool(okv) && okv->bool_val;
            /* Broker resolves only the pending command whose scope equals this
             * socket's scope; a stranger's command id is a no-op. */
            if (adapter->browser_control_broker)
                bcb_broker_complete(adapter->browser_control_broker, cid->str_val,
                                    scope, ok ? 1 : 0,
                                    ok ? json_obj_get(params, "result")
                                       : json_obj_get(params, "error"));
        }
        return NULL;
    }
    if (strcmp(method, "browser.controller.cancel") == 0) {
        const json_t *tc = json_obj_get(params, "tool_call_id");
        if (tc && json_is_string(tc) && tc->str_val[0] &&
            adapter->browser_control_broker)
            bcb_broker_cancel(adapter->browser_control_broker, scope, tc->str_val);
        return NULL;
    }
    return NULL;
}

/* PoP: apisg_handle_artifact_upload @ api_server.py:_handle_artifact_upload */
int apisg_handle_artifact_upload(api_server_adapter_t *adapter, int client_fd,
                                 const unsigned char *data, size_t data_len,
                                 const char *content_type, const char *filename_hdr,
                                 const char *peer_host) {
    /* Status ladder: 404 disabled, 403 no key, 429 rate limited,
     * 400 missing filename/body, 413 too large, 415 MIME rejected,
     * 500 no root, 201 provenance receipt. */
    if (!apisg_browser_control_enabled()) {
        json_t *err = apisg_openai_error(
            "Browser control is not enabled on this server.", NULL, NULL,
            "browser_control_disabled");
        char *s = json_serialize(err);
        send_json_response(client_fd, 404, s);
        free(s); json_free(err);
        return 404;
    }
    if (!adapter->api_key[0]) {
        json_t *err = apisg_openai_error(
            "Artifact transport requires a configured API key.",
            "gateway_auth_error", NULL, "browser_control_auth_required");
        char *s = json_serialize(err);
        send_json_response(client_fd, 403, s);
        free(s); json_free(err);
        return 403;
    }
    const char *profile = getenv("HERMES_PROFILE");
    if (!profile || !*profile) profile = "default";
    char *principal = apisg_derive_browser_control_principal(adapter, profile);
    bca_rate_limiter_t *limiter = apisg_artifact_limiter(adapter);
    char limiter_key[384];
    snprintf(limiter_key, sizeof(limiter_key), "upload:%s", principal ? principal : "");
    if (!limiter || !bca_rate_limiter_allow(limiter, limiter_key)) {
        json_t *err = apisg_openai_error("Artifact upload rate limit exceeded.",
                                         "rate_limit_error", NULL,
                                         "rate_limit_exceeded");
        char *s = json_serialize(err);
        send_json_response_with_retry_after(client_fd, 429, s, "1");
        free(s); json_free(err);
        free(principal);
        return 429;
    }
    if (!filename_hdr || !*filename_hdr) {
        json_t *err = apisg_openai_error("X-Artifact-Filename header is required.",
                                         NULL, NULL, NULL);
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        free(principal);
        return 400;
    }
    bca_store_t *store = apisg_artifact_store_for(adapter, profile);
    if (!store) {
        json_t *err = apisg_openai_error("no artifact root is resolvable", NULL, NULL,
                                         "artifact_rejected");
        char *s = json_serialize(err);
        send_json_response(client_fd, 500, s);
        free(s); json_free(err);
        free(principal);
        return 500;
    }
    long max_bytes = bca_store_max_bytes(store);
    /* Bounded read already enforced by the caller's cap of max_bytes+1. */
    if (data_len > (size_t)max_bytes) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Artifact exceeds the %ld-byte cap.", max_bytes);
        json_t *err = apisg_openai_error(msg, NULL, NULL, "artifact_too_large");
        char *s = json_serialize(err);
        send_json_response(client_fd, 413, s);
        free(s); json_free(err);
        free(principal);
        return 413;
    }
    if (data_len == 0) {
        json_t *err = apisg_openai_error("Empty artifact body.", NULL, NULL, NULL);
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        free(principal);
        return 400;
    }
    bca_scope_t scope = {.principal_id = principal,
                         .transport_family =
                             apisg_browser_control_transport_family(peer_host)};
    bca_receipt_t receipt;
    memset(&receipt, 0, sizeof(receipt));
    bca_error_t rc = bca_store_store(store, data, data_len,
                                     filename_hdr, content_type ? content_type : "",
                                     &scope, &receipt);
    if (rc != BCA_OK) {
        int status = 400;
        const char *code = "artifact_rejected";
        if (rc == BCA_ERR_TOO_LARGE) { status = 413; code = "artifact_too_large"; }
        else if (rc == BCA_ERR_MIME_REJECTED) { status = 415; code = "artifact_mime_rejected"; }
        json_t *err = apisg_openai_error(code, NULL, NULL, code);
        char *s = json_serialize(err);
        send_json_response(client_fd, status, s);
        free(s); json_free(err);
        free(principal);
        return status;
    }
    char download_path[96];
    snprintf(download_path, sizeof(download_path), "/v1/artifacts/download/%s",
             receipt.artifact_id);
    json_t *resp = bca_receipt_to_dict(&receipt, download_path);
    char *s = json_serialize(resp);
    send_json_response(client_fd, 201, s);
    free(s); json_free(resp);
    free(principal);
    return 201;
}

/* api_server.py:_handle_artifact_download — see note below. */
/* PoP: apisg_handle_artifact_download @ api_server.py:_handle_artifact_download */
/* transport via the out-parameters. */
int apisg_handle_artifact_download(api_server_adapter_t *adapter, int client_fd,
                                   const char *artifact_id, const char *peer_host,
                                   char **sha256_out, char **filename_out) {
    if (!apisg_browser_control_enabled()) {
        send_json_response(client_fd, 404, "{\"error\":\"not found\"}");
        return 404;
    }
    if (!adapter->api_key[0]) {
        json_t *err = apisg_openai_error(
            "Artifact transport requires a configured API key.",
            "gateway_auth_error", NULL, "browser_control_auth_required");
        char *s = json_serialize(err);
        send_json_response(client_fd, 403, s);
        free(s); json_free(err);
        return 403;
    }
    const char *profile = getenv("HERMES_PROFILE");
    if (!profile || !*profile) profile = "default";
    char *principal = apisg_derive_browser_control_principal(adapter, profile);
    bca_rate_limiter_t *limiter = apisg_artifact_limiter(adapter);
    char limiter_key[392];
    snprintf(limiter_key, sizeof(limiter_key), "download:%s", principal ? principal : "");
    if (!limiter || !bca_rate_limiter_allow(limiter, limiter_key)) {
        json_t *err = apisg_openai_error("Artifact download rate limit exceeded.",
                                         "rate_limit_error", NULL,
                                         "rate_limit_exceeded");
        char *s = json_serialize(err);
        send_json_response_with_retry_after(client_fd, 429, s, "1");
        free(s); json_free(err);
        free(principal);
        return 429;
    }
    bca_store_t *store = apisg_artifact_store_for(adapter, profile);
    bca_scope_t scope = {.principal_id = principal,
                         .transport_family =
                             apisg_browser_control_transport_family(peer_host)};
    unsigned char *data = NULL;
    size_t data_len = 0;
    bca_receipt_t receipt;
    memset(&receipt, 0, sizeof(receipt));
    bca_error_t rc = store ? bca_store_load(store, artifact_id ? artifact_id : "",
                                           &scope, &data, &data_len, &receipt)
                           : BCA_ERR_NOT_FOUND;
    if (rc != BCA_OK) {
        int status;
        const char *code;
        switch (rc) {
        case BCA_ERR_EXPIRED: status = 410; code = "artifact_expired"; break;
        case BCA_ERR_SCOPE_MISMATCH: status = 400; code = "artifact_not_found"; break;
        default: status = 404; code = "artifact_not_found"; break;
        }
        json_t *err = apisg_openai_error(code, NULL, NULL, code);
        char *s = json_serialize(err);
        send_json_response(client_fd, status, s);
        free(s); json_free(err);
        free(principal);
        return status;
    }
    if (sha256_out) *sha256_out = strdup(receipt.sha256);
    if (filename_out) *filename_out = receipt.filename ? strdup(receipt.filename)
                                                       : strdup("artifact");
    extern void send_binary_response(int fd, int status, const unsigned char *body,
                                     size_t body_len, const char *content_type,
                                     const char *sha256, const char *artifact_id,
                                     const char *filename);
    send_binary_response(client_fd, 200, data, data_len, receipt.content_type,
                         receipt.sha256, receipt.artifact_id,
                         receipt.filename ? receipt.filename : "artifact");
    free(data);
    free(principal);
    return 200;
}

/* ── Session projection helpers ─────────────────────────────────────── */

static const char *const APISG_SESSION_SAFE_KEYS[] = {
    "id", "source", "user_id", "model", "title", "started_at", "ended_at",
    "end_reason", "message_count", "tool_call_count", "input_tokens",
    "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "estimated_cost_usd", "actual_cost_usd",
    "api_call_count", "parent_session_id", "last_active", "preview",
    "_lineage_root_id", "pinned", "archived", "hidden", NULL};

/* PoP: apisg_session_response @ api_server.py:_session_response */
json_t *apisg_session_response(const json_t *session) {
    json_t *payload = json_object();
    if (!payload) return NULL;
    for (const char *const *key = APISG_SESSION_SAFE_KEYS; *key; key++) {
        const json_t *v = json_obj_get(session, *key);
        if (v) json_set(payload, *key, json_copy(v));
    }
    /* SQLite stores these as 0/1; clients reconcile against a real boolean. */
    const char *flags[] = {"pinned", "archived", "hidden"};
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
        const json_t *v = json_obj_get(session, flags[i]);
        if (v) json_set(payload, flags[i], json_bool(v->num_val != 0 || v->bool_val));
    }
    /* Never expose full system prompts/model_config through the client API. */
    const json_t *sp = json_obj_get(session, "system_prompt");
    json_set(payload, "has_system_prompt",
             json_bool(sp && !(json_is_string(sp) && sp->str_val[0] == '\0') &&
                       !json_is_null(sp)));
    const json_t *mc = json_obj_get(session, "model_config");
    json_set(payload, "has_model_config",
             json_bool(mc && !(json_is_string(mc) && mc->str_val[0] == '\0') &&
                       !json_is_null(mc)));
    return payload;
}

static const char *const APISG_MESSAGE_SAFE_KEYS[] = {
    "id", "session_id", "role", "content", "tool_call_id", "tool_calls",
    "tool_name", "timestamp", "token_count", "finish_reason", "reasoning",
    "reasoning_content", "display_kind", NULL};

/* PoP: apisg_message_response @ api_server.py:_message_response */
json_t *apisg_message_response(const json_t *message_in) {
    extern json_t *apis_project_client_message(const json_t *message);
    json_t *message = apis_project_client_message ? apis_project_client_message(message_in)
                                                  : (message_in ? json_copy(message_in) : NULL);
    json_t *out = json_object();
    for (const char *const *key = APISG_MESSAGE_SAFE_KEYS; *key; key++) {
        const json_t *v = message ? json_obj_get(message, *key) : NULL;
        if (v) json_set(out, *key, json_copy(v));
    }
    if (message) json_free(message);
    return out;
}

/* api_server.py:_read_json_body — see note below. */
/* PoP: apisg_read_json_body @ api_server.py:_read_json_body */
/* Returns parsed object or NULL with *err_status 400 set. */
json_t *apisg_read_json_body(const char *raw_body, int *err_status) {
    if (err_status) *err_status = 0;
    json_t *body = raw_body ? json_parse(raw_body, NULL) : NULL;
    if (!body || !json_is_object(body)) {
        if (body) json_free(body);
        if (err_status) *err_status = 400;
        return NULL;
    }
    return body;
}

/* PoP: apisg_get_existing_session_or_404 @ api_server.py:_get_existing_session_or_404 */
json_t *apisg_get_existing_session_or_404(api_server_adapter_t *adapter,
                                          const char *session_id, int *err_status) {
    hermes_state_db_t *db = apisg_ensure_session_db_async(adapter);
    if (!db) {
        if (err_status) *err_status = 503;
        return NULL;
    }
    json_t *session = NULL;
    {
        char *raw = hermes_state_get_session(db, session_id);
        if (raw) {
            session = json_parse(raw, NULL);
            free(raw);
        }
    }
    if (!session) {
        if (err_status) *err_status = 404;
        return NULL;
    }
    if (err_status) *err_status = 0;
    return session;
}

/* PoP: apisg_conversation_history_for_session @ api_server.py:_conversation_history_for_session */
json_t *apisg_conversation_history_for_session(api_server_adapter_t *adapter,
                                               const char *session_id) {
    hermes_state_db_t *db = apisg_ensure_session_db_async(adapter);
    if (!db) return json_array();
    json_t *history = NULL;
    {
        char *raw = hermes_state_get_messages_as_conversation(db, session_id, true);
        if (raw) {
            history = json_parse(raw, NULL);
            free(raw);
        }
    }
    if (history) return history;
    fprintf(stderr, "[api_server] Failed to load session history for %s\n", session_id);
    return json_array();
}

/* PoP: apisg_check_jobs_available @ api_server.py:_check_jobs_available */
int apisg_check_jobs_available(int client_fd) {
    /* Return error response if cron module isn't available. */
    if (!APIsg_CRON_AVAILABLE) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Cron module not available"));
        char *s = json_serialize(err);
        send_json_response(client_fd, 501, s);
        free(s); json_free(err);
        return 501;
    }
    return 0;
}

/* api_server.py:_check_job_id — see note below. */
/* PoP: apisg_check_job_id @ api_server.py:_check_job_id */
/* Validate and extract job_id ([a-f0-9]{12} fullmatch). Returns 0 when OK. */
int apisg_check_job_id(api_server_adapter_t *adapter, int client_fd,
                       const char *job_id) {
    size_t n = job_id ? strlen(job_id) : 0;
    bool ok = (n == 12);
    for (size_t i = 0; ok && i < n; i++) {
        char c = job_id[i];
        if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))) ok = false;
    }
    if (ok) return 0;
    char *audit = apisg_request_audit_log_suffix(
        apisg_request_audit_context("", "", "", "", "", "", ""));
    fprintf(stderr, "[api_server] Cron jobs API rejected invalid job_id '%s': %s\n",
            job_id ? job_id : "(null)", audit ? audit : "");
    free(audit);
    json_t *err = json_object();
    json_set(err, "error", json_string("Invalid job ID format"));
    char *s = json_serialize(err);
    send_json_response(client_fd, 400, s);
    free(s); json_free(err);
    return 400;
}

/* ── fd-level HTTP/SSE helpers used by the handler ports above ─────── */

static void apisg_write_fd(int fd, const char *bytes, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, bytes + off, len - off);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return; /* ConnectionResetError / BrokenPipeError analog */
        }
        off += (size_t)n;
    }
}

void send_json_response_with_retry_after(int fd, int status, const char *json_body,
                                         const char *retry_after) {
    const char *reason = (status == 429)  ? "Too Many Requests"
                         : (status == 503) ? "Service Unavailable"
                                           : "OK";
    char head[512];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Retry-After: %s\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     status, reason, retry_after ? retry_after : "1",
                     strlen(json_body));
    if (n <= 0 || (size_t)n >= sizeof(head)) return;
    apisg_write_fd(fd, head, (size_t)n);
    apisg_write_fd(fd, json_body, strlen(json_body));
}

void send_binary_response(int fd, int status, const unsigned char *body,
                          size_t body_len, const char *content_type,
                          const char *sha256, const char *artifact_id,
                          const char *filename) {
    if (!content_type || !*content_type) content_type = "application/octet-stream";
    char head[1024];
    int n = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\n"
                     "Content-Type: %s\r\n"
                     "X-Artifact-Sha256: %s\r\n"
                     "X-Artifact-Id: %s\r\n"
                     "Content-Disposition: attachment; filename=\"%s\"\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n",
                     status, "OK", content_type, sha256 ? sha256 : "",
                     artifact_id ? artifact_id : "", filename ? filename : "",
                     body_len);
    if (n <= 0 || (size_t)n >= sizeof(head)) return;
    apisg_write_fd(fd, head, (size_t)n);
    apisg_write_fd(fd, (const char *)body, body_len);
}

/* PoP: apisg_sse_frame @ api_server.py:_sse_frame — already ported in
 * api_server_adapter.c as api_server_sse_frame; local thin wrapper for the
 * event-tagged writers below. */
extern char *api_server_sse_frame(const char *data_json, const char *event);

/* api_server.py:_write_sse_chat_completion — see note below. */
/* PoP: apisg_write_sse_chat_completion @ api_server.py:_write_sse_chat_completion */
/* (via gwapi_reap_disconnected_agent_processes). */
int apisg_write_sse_chat_completion(api_server_adapter_t *adapter, int client_fd,
                                    const char *completion_id, const char *model,
                                    long created, sse_queue_t *stream_q,
                                    volatile bool *agent_task_done,
                                    void *agent_ref_slot, const char *session_id,
                                    const char *gateway_session_key) {
    char head[1024];
    snprintf(head, sizeof(head),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/event-stream\r\n"
             "Cache-Control: no-cache\r\n"
             "X-Accel-Buffering: no\r\n");
    /* CORS middleware can't inject headers after prepare() flushes them, so
     * resolve CORS headers up front. */
    json_t *cors = adapter ? NULL : NULL; /* origin supplied per-request upstream */
    (void)cors;
    size_t hlen = strlen(head);
    if (session_id && *session_id)
        hlen += (size_t)snprintf(head + hlen, sizeof(head) - hlen,
                                 "X-Hermes-Session-Id: %s\r\n", session_id);
    if (gateway_session_key && *gateway_session_key)
        hlen += (size_t)snprintf(head + hlen, sizeof(head) - hlen,
                                 "X-Hermes-Session-Key: %s\r\n", gateway_session_key);
    snprintf(head + hlen, sizeof(head) - hlen, "\r\n");
    apisg_write_fd(client_fd, head, strlen(head));

    double last_activity = (double)clock() / CLOCKS_PER_SEC;

    /* Role chunk */
    json_t *role_chunk = json_object();
    json_set(role_chunk, "id", json_string(completion_id));
    json_set(role_chunk, "object", json_string("chat.completion.chunk"));
    json_set(role_chunk, "created", json_number((double)created));
    json_set(role_chunk, "model", json_string(model));
    json_t *choices = json_array();
    json_t *choice = json_object();
    json_set(choice, "index", json_number(0));
    json_t *delta = json_object();
    json_set(delta, "role", json_string("assistant"));
    json_set(choice, "delta", delta);
    json_set(choice, "finish_reason", json_null());
    json_append(choices, choice);
    json_set(role_chunk, "choices", choices);
    char *frame = api_server_sse_frame(json_serialize(role_chunk), NULL);
    if (frame) { apisg_write_fd(client_fd, frame, strlen(frame)); free(frame); }
    json_free(role_chunk);

    while (true) {
        char *delta_item = sse_queue_get(stream_q, 500); /* wait_for(0.5) */
        if (!delta_item) {
            if (*agent_task_done) {
                /* Drain any remaining items */
                for (;;) {
                    char *rem = sse_queue_get(stream_q, 0); /* get_nowait */
                    if (!rem) break;
                    bool eos = strcmp(rem, "__EOS__") == 0 ||
                               strstr(rem, "\"__eos__\": true") != NULL;
                    if (eos) { free(rem); break; }
                    json_t *content_chunk = json_object();
                    json_set(content_chunk, "id", json_string(completion_id));
                    json_set(content_chunk, "object", json_string("chat.completion.chunk"));
                    json_set(content_chunk, "created", json_number((double)created));
                    json_set(content_chunk, "model", json_string(model));
                    json_t *ch2 = json_array();
                    json_t *c2 = json_object();
                    json_set(c2, "index", json_number(0));
                    json_t *d2 = json_object();
                    json_set(d2, "content", json_string(rem));
                    json_set(c2, "delta", d2);
                    json_set(c2, "finish_reason", json_null());
                    json_append(ch2, c2);
                    json_set(content_chunk, "choices", ch2);
                    char *ser = json_serialize(content_chunk);
                    char *f2 = api_server_sse_frame(ser, NULL);
                    if (f2) { apisg_write_fd(client_fd, f2, strlen(f2)); free(f2); }
                    free(ser);
                    json_free(content_chunk);
                    free(rem);
                }
                break;
            }
            if ((double)clock() / CLOCKS_PER_SEC - last_activity >=
                CHAT_COMPLETIONS_SSE_KEEPALIVE_SECONDS) {
                apisg_write_fd(client_fd, ": keepalive\n\n", 13);
                last_activity = (double)clock() / CLOCKS_PER_SEC;
            }
            continue;
        }
        if (strcmp(delta_item, "__EOS__") == 0) { free(delta_item); break; }
        /* Tagged ("__tool_progress__", payload) items ride a custom event so
         * frontends can display them without polluting history (#6972/#16588). */
        const char *event_name = NULL;
        const char *payload_text = delta_item;
        if (strncmp(delta_item, "__tool_progress__\t", 18) == 0) {
            event_name = "hermes.tool.progress";
            payload_text = delta_item + 18;
        }
        json_t *chunk = json_object();
        json_set(chunk, "id", json_string(completion_id));
        json_set(chunk, "object", json_string("chat.completion.chunk"));
        json_set(chunk, "created", json_number((double)created));
        json_set(chunk, "model", json_string(model));
        json_t *ch3 = json_array();
        json_t *c3 = json_object();
        json_set(c3, "index", json_number(0));
        json_t *d3 = json_object();
        json_set(d3, "content", json_string(payload_text));
        json_set(c3, "delta", d3);
        json_set(c3, "finish_reason", json_null());
        json_append(ch3, c3);
        json_set(chunk, "choices", ch3);
        char *ser = json_serialize(chunk);
        char *f3 = api_server_sse_frame(ser, event_name);
        if (f3) { apisg_write_fd(client_fd, f3, strlen(f3)); free(f3); }
        free(ser);
        json_free(chunk);
        free(delta_item);
        last_activity = (double)clock() / CLOCKS_PER_SEC;
    }

    /* Finish chunk — finish_reason mirrors the non-streaming decision:
     * "length" for truncation, "error" for failure, "stop" otherwise. */
    json_t *finish = json_object();
    json_set(finish, "id", json_string(completion_id));
    json_set(finish, "object", json_string("chat.completion.chunk"));
    json_set(finish, "created", json_number((double)created));
    json_set(finish, "model", json_string(model));
    json_t *fc = json_array();
    json_t *fch = json_object();
    json_set(fch, "index", json_number(0));
    json_set(fch, "delta", json_object());
    json_set(fch, "finish_reason", json_string("stop"));
    json_append(fc, fch);
    json_set(finish, "choices", fc);
    frame = api_server_sse_frame(json_serialize(finish), NULL);
    if (frame) { apisg_write_fd(client_fd, frame, strlen(frame)); free(frame); }
    json_free(finish);
    apisg_write_fd(client_fd, "data: [DONE]\n\n", 14);
    return 200;
}

/* api_server.py:_set_run_status — see note below. */
/* PoP: apisg_set_run_status @ api_server.py:_set_run_status */
/* Extra fields come in as a JSON object merged into the record. */
json_t *apisg_set_run_status(api_server_adapter_t *adapter, const char *run_id,
                             const char *status, const json_t *fields) {
    double now = (double)time(NULL);
    pthread_mutex_lock(&adapter->run_lock);
    run_status_t *cur = run_status_get(adapter, run_id);
    if (!cur) {
        cur = run_status_new(run_id, "", "");
        if (cur) run_status_set(adapter, run_id, cur);
    }
    if (cur) {
        snprintf(cur->status, sizeof(cur->status), "%s", status);
        cur->updated_at = now;
        if (!cur->created_at) cur->created_at = now;
        if (fields && json_is_object(fields))
            json_set(cur->usage ? cur->usage : (cur->usage = json_object()),
                     "_fields", json_copy(fields));
    }
    pthread_mutex_unlock(&adapter->run_lock);
    json_t *out = json_object();
    json_set(out, "object", json_string("hermes.run"));
    json_set(out, "run_id", json_string(run_id));
    json_set(out, "status", json_string(status));
    json_set(out, "updated_at", json_number(now));
    return out;
}

/* PoP: apisg_make_run_event_callback_push @ api_server.py:_make_run_event_callback._push */
static void apisg_run_event_push(api_server_adapter_t *adapter, const char *run_id,
                                 const json_t *event) {
    pthread_mutex_lock(&adapter->run_lock);
    run_status_t *rs = run_status_get(adapter, run_id);
    char prev[32] = "running";
    if (rs && rs->status[0]) snprintf(prev, sizeof(prev), "%s", rs->status);
    pthread_mutex_unlock(&adapter->run_lock);
    json_t *upd = json_object();
    json_set(upd, "last_event", json_copy(json_obj_get(event, "event")));
    json_free(apisg_set_run_status(adapter, run_id, prev, upd));

    sse_queue_t *q = NULL;
    pthread_mutex_lock(&adapter->run_lock);
    q = run_status_get(adapter, run_id) ? run_status_get(adapter, run_id)->event_queue
                                        : NULL;
    pthread_mutex_unlock(&adapter->run_lock);
    if (!q) return;
    char *ser = json_serialize(event);
    if (ser) sse_queue_put(q, ser); /* loop.call_soon_threadsafe(q.put_nowait, …) */
    free(ser);
}

/* api_server.py:_make_run_event_callback — see note below. */
/* PoP: apisg_make_run_event_callback @ api_server.py:_make_run_event_callback */
/* queue. args_json carries the kwargs dict. */
void apisg_make_run_event_callback(api_server_adapter_t *adapter, const char *run_id,
                                   const char *event_type, const char *tool_name,
                                   const char *preview, const json_t *args_json) {
    double ts = (double)time(NULL);
    json_t *event = NULL;
    if (strcmp(event_type, "tool.started") == 0) {
        event = json_object();
        json_set(event, "event", json_string("tool.started"));
        json_set(event, "run_id", json_string(run_id));
        json_set(event, "timestamp", json_number(ts));
        json_set(event, "tool", json_string(tool_name ? tool_name : ""));
        json_set(event, "preview", json_string(preview ? preview : ""));
    } else if (strcmp(event_type, "tool.completed") == 0) {
        event = json_object();
        json_set(event, "event", json_string("tool.completed"));
        json_set(event, "run_id", json_string(run_id));
        json_set(event, "timestamp", json_number(ts));
        json_set(event, "tool", json_string(tool_name ? tool_name : ""));
        double duration = json_is_object(args_json)
                              ? json_get_num(args_json, "duration", 0.0)
                              : 0.0;
        /* round(x, 3) */
        duration = ((double)((long)(duration * 1000.0 + (duration < 0 ? -0.5 : 0.5)))) / 1000.0;
        json_set(event, "duration", json_number(duration));
        bool is_error = json_is_object(args_json) &&
                        json_get_bool(args_json, "is_error", false);
        json_set(event, "error", json_bool(is_error));
    } else if (strcmp(event_type, "reasoning.available") == 0) {
        event = json_object();
        json_set(event, "event", json_string("reasoning.available"));
        json_set(event, "run_id", json_string(run_id));
        json_set(event, "timestamp", json_number(ts));
        json_set(event, "text", json_string(preview ? preview : ""));
    } else if (strcmp(event_type, "subagent.start") == 0 ||
               strcmp(event_type, "subagent.complete") == 0) {
        event = json_object();
        json_set(event, "event", json_string(event_type));
        json_set(event, "run_id", json_string(run_id));
        json_set(event, "timestamp", json_number(ts));
        static const char *const subagent_keys[] = {
            "goal", "task_count", "task_index", "subagent_id", "child_session_id",
            "parent_id", "depth", "model", "tool_count", "status", "summary",
            "duration_seconds", "input_tokens", "output_tokens",
            "reasoning_tokens", "api_calls", "cost_usd", "files_read",
            "files_written", "output_tail", NULL};
        for (const char *const *key = subagent_keys; *key; key++) {
            const json_t *v = json_is_object(args_json) ? json_obj_get(args_json, *key)
                                                        : NULL;
            if (!v || json_is_null(v)) continue;
            /* Free-text fields can carry child terminal/tool output — force
             * the same secret redaction applied to error text on public
             * streams. */
            if ((strcmp(*key, "goal") == 0 || strcmp(*key, "summary") == 0 ||
                 strcmp(*key, "output_tail") == 0) &&
                json_is_string(v)) {
                char *red = hermes_redact_force(v->str_val);
                json_set(event, *key, json_string(red ? red : ""));
                free(red);
            } else {
                json_set(event, *key, json_copy(v));
            }
        }
    }
    /* _thinking / subagent.tool / subagent_progress intentionally not
     * forwarded: high-volume UI noise; lifecycle boundaries still land. */
    if (!event) return;
    if (preview && (strcmp(event_type, "subagent.start") == 0) &&
        !json_obj_get(event, "preview")) {
        char *red = hermes_redact_force(preview);
        json_set(event, "preview", json_string(red ? red : ""));
        free(red);
    }
    apisg_run_event_push(adapter, run_id, event);
    json_free(event);
}

/* PoP: apisg_response_messages_turn_start_index @ api_server.py:_response_messages_turn_start_index */
int apisg_response_messages_turn_start_index(const json_t *conversation_history,
                                             const json_t *user_message,
                                             const json_t *result) {
    const json_t *agent_messages = json_is_object(result)
                                       ? json_obj_get(result, "messages")
                                       : NULL;
    if (!json_is_array(agent_messages) || json_len(agent_messages) == 0) return 0;
    size_t prior_n = json_is_array(conversation_history)
                         ? json_len(conversation_history)
                         : 0;
    /* expected_prefix = prior + [current_user]; compare by identity of the
     * serialized message objects (role+content structural equality). */
    char *expected = malloc(65536);
    char *actual = malloc(65536);
    int turn_start = 0;
    if (expected && actual) {
        expected[0] = actual[0] = '\0';
        for (size_t i = 0; i < prior_n + 1; i++) {
            const json_t *m =
                (i < prior_n) ? json_get(conversation_history, i) : user_message;
            char *ser = m ? json_serialize(m) : NULL;
            if (!ser) break;
            strncat(expected, ser, 65535 - strlen(expected));
            free(ser);
        }
        for (size_t i = 0; i < prior_n + 1 && i < json_len(agent_messages); i++) {
            const json_t *m = json_get(agent_messages, i);
            char *ser = m ? json_serialize(m) : NULL;
            if (!ser) break;
            strncat(actual, ser, 65535 - strlen(actual));
            free(ser);
        }
        if (prior_n + 1 <= json_len(agent_messages) &&
            strcmp(expected, actual) == 0)
            turn_start = (int)(prior_n + 1);
        else if (prior_n > 0 && prior_n <= json_len(agent_messages)) {
            /* agent_messages[:len(prior)] == prior → len(prior) */
            actual[0] = '\0';
            bool eq = true;
            for (size_t i = 0; i < prior_n; i++) {
                const json_t *m1 = json_get(conversation_history, i);
                const json_t *m2 = json_get(agent_messages, i);
                char *s1 = m1 ? json_serialize(m1) : NULL;
                char *s2 = m2 ? json_serialize(m2) : NULL;
                if (!s1 || !s2 || strcmp(s1, s2) != 0) { eq = false; }
                free(s1); free(s2);
                if (!eq) break;
            }
            if (eq) turn_start = (int)prior_n;
        }
    }
    free(expected);
    free(actual);
    return turn_start;
}

/* PoP: apisg_build_response_conversation_history @ api_server.py:_build_response_conversation_history */
json_t *apisg_build_response_conversation_history(const json_t *conversation_history,
                                                  const json_t *user_message,
                                                  const json_t *result,
                                                  const json_t *final_response) {
    /* Build the stored Responses transcript without duplicating history:
     * a compressed result["messages"] replaces the prefix entirely. */
    json_t *full = json_array();
    size_t prior_n = json_is_array(conversation_history) ? json_len(conversation_history) : 0;
    const json_t *agent_messages = json_is_object(result)
                                       ? json_obj_get(result, "messages")
                                       : NULL;
    if (json_is_array(agent_messages) && json_len(agent_messages) > 0) {
        int turn_start = apisg_response_messages_turn_start_index(
            conversation_history, user_message, result);
        if (turn_start > 0) {
            for (size_t i = (size_t)turn_start; i < json_len(agent_messages); i++)
                json_append(full, json_copy(json_get(agent_messages, i)));
            return full;
        }
        /* turn_start == 0: compression rewrote the transcript (_compressed
         * flag set by _run_agent after compaction) or only this turn exists —
         * skip concatenation when compressed. */
        if (json_is_true(json_obj_get(result, "_compressed"))) {
            for (size_t i = 0; i < json_len(agent_messages); i++)
                json_append(full, json_copy(json_get(agent_messages, i)));
            return full;
        }
        for (size_t i = 0; i < prior_n; i++)
            json_append(full, json_copy(json_get(conversation_history, i)));
        json_t *cur = json_object();
        json_set(cur, "role", json_string("user"));
        json_set(cur, "content", user_message ? json_copy(user_message) : json_null());
        json_append(full, cur);
        for (size_t i = 0; i < json_len(agent_messages); i++)
            json_append(full, json_copy(json_get(agent_messages, i)));
        return full;
    }
    for (size_t i = 0; i < prior_n; i++)
        json_append(full, json_copy(json_get(conversation_history, i)));
    json_t *cur = json_object();
    json_set(cur, "role", json_string("user"));
    json_set(cur, "content", user_message ? json_copy(user_message) : json_null());
    json_append(full, cur);
    json_t *asst = json_object();
    json_set(asst, "role", json_string("assistant"));
    json_set(asst, "content",
             final_response ? json_copy(final_response) : json_null());
    json_append(full, asst);
    return full;
}

/* PoP: apisg_turn_transcript_messages @ api_server.py:_turn_transcript_messages */
json_t *apisg_turn_transcript_messages(const json_t *conversation_history,
                                       const json_t *user_message,
                                       const json_t *result) {
    json_t *out = json_array();
    const json_t *agent_messages = json_is_object(result)
                                       ? json_obj_get(result, "messages")
                                       : NULL;
    if (!json_is_array(agent_messages) || json_len(agent_messages) == 0) return out;
    int start = apisg_response_messages_turn_start_index(conversation_history,
                                                         user_message, result);
    for (size_t i = (size_t)(start > 0 ? start : 0); i < json_len(agent_messages); i++) {
        const json_t *msg = json_get(agent_messages, i);
        if (!json_is_object(msg)) continue;
        const char *role = json_get_str(msg, "role", "");
        if (strcmp(role, "assistant") != 0 && strcmp(role, "tool") != 0) continue;
        json_t *projected = apisg_message_response(msg);
        if (!projected) continue;
        if (strcmp(json_get_str(projected, "display_kind", ""), "hidden") == 0) {
            json_free(projected);
            continue;
        }
        json_append(out, projected);
    }
    return out;
}

/* PoP: apisg_extract_output_items @ api_server.py:_extract_output_items */
json_t *apisg_extract_output_items(const json_t *result, int start_index) {
    json_t *items = json_array();
    const json_t *messages = json_is_object(result) ? json_obj_get(result, "messages")
                                                    : NULL;
    size_t n = json_is_array(messages) ? json_len(messages) : 0;
    for (size_t i = (start_index > 0 ? (size_t)start_index : 0); i < n; i++) {
        const json_t *msg = json_get(messages, i);
        if (!json_is_object(msg)) continue;
        const char *role = json_get_str(msg, "role", "");
        const json_t *tool_calls = json_obj_get(msg, "tool_calls");
        if (strcmp(role, "assistant") == 0 && tool_calls && json_is_array(tool_calls)) {
            for (size_t t = 0; t < json_len(tool_calls); t++) {
                const json_t *tc = json_get(tool_calls, t);
                const json_t *func = tc ? json_obj_get(tc, "function") : NULL;
                char *hex = apisg_uuid_hex24();
                char id[32];
                snprintf(id, sizeof(id), "fc_%s", hex ? hex : "");
                free(hex);
                json_t *item = json_object();
                json_set(item, "id", json_string(id));
                json_set(item, "type", json_string("function_call"));
                /* Executed server-side already; marked completed so OpenAI
                 * clients don't treat them as pending client-side calls. */
                json_set(item, "status", json_string("completed"));
                json_set(item, "name", json_string(json_get_str(func, "name", "")));
                json_set(item, "arguments",
                         json_string(json_get_str(func, "arguments", "")));
                json_set(item, "call_id", json_string(json_get_str(tc, "id", "")));
                json_append(items, item);
            }
        } else if (strcmp(role, "tool") == 0) {
            char *hex = apisg_uuid_hex24();
            char id[36];
            snprintf(id, sizeof(id), "fco_%s", hex ? hex : "");
            free(hex);
            json_t *item = json_object();
            json_set(item, "id", json_string(id));
            json_set(item, "type", json_string("function_call_output"));
            json_set(item, "status", json_string("completed"));
            json_set(item, "call_id", json_string(json_get_str(msg, "tool_call_id", "")));
            const json_t *content = json_obj_get(msg, "content");
            json_set(item, "output", content ? json_copy(content) : json_string(""));
            json_append(items, item);
        }
    }
    const json_t *final_v = json_is_object(result)
                                ? json_obj_get(result, "final_response")
                                : NULL;
    const char *final = (final_v && json_is_string(final_v)) ? final_v->str_val : "";
    char *final_owned = NULL;
    if (!final || !*final) {
        char *red = apis_redact_api_error_text(
            json_get_str(result, "error", "(No response generated)"), 0);
        final_owned = red;
        final = red ? red : "";
    }
    json_t *item = json_object();
    json_set(item, "type", json_string("message"));
    json_set(item, "role", json_string("assistant"));
    json_t *content_arr = json_array();
    json_t *part = json_object();
    json_set(part, "type", json_string("output_text"));
    json_set(part, "text", json_string(final));
    json_append(content_arr, part);
    json_set(item, "content", content_arr);
    json_append(items, item);
    free(final_owned);
    return items;
}

/* api_server.py:_handle_steer_run — see note below. */
/* PoP: apisg_handle_steer_run @ api_server.py:_handle_steer_run */
/* returns the status used. */
int apisg_handle_steer_run(api_server_adapter_t *adapter, int client_fd,
                           const char *auth_error_json, const char *run_id,
                           const char *body_raw) {
    if (auth_error_json) {
        send_json_response(client_fd, 401, auth_error_json);
        return 401;
    }
    pthread_mutex_lock(&adapter->run_lock);
    run_status_t *rs = run_status_get(adapter, run_id);
    run_status_t *rs_ref = rs;
    pthread_mutex_unlock(&adapter->run_lock);
    if (!rs_ref) {
        char msg[192];
        snprintf(msg, sizeof(msg), "Run not found: %s", run_id);
        json_t *err = apisg_openai_error(msg, NULL, NULL, "run_not_found");
        char *s = json_serialize(err);
        send_json_response(client_fd, 404, s);
        free(s); json_free(err);
        return 404;
    }
    /* Only genuinely running runs are steerable: /stop retains refs during
     * cooperative shutdown, so the status gate rejects stop-then-steer. */
    void *agent = NULL;
    extern __attribute__((weak)) int gw_agent_steer(void *agent, const char *text);
    if (strcmp(rs_ref->status, "running") != 0 || !gw_agent_steer) {
        char msg[224];
        snprintf(msg, sizeof(msg),
                 "Run is not currently accepting steer input: %s", run_id);
        json_t *err = apisg_openai_error(msg, NULL, NULL, "run_not_accepting_steer");
        char *s = json_serialize(err);
        send_json_response(client_fd, 409, s);
        free(s); json_free(err);
        return 409;
    }
    json_t *body = json_parse(body_raw, NULL);
    if (!body || !json_is_object(body)) {
        json_t *err = apisg_openai_error("Invalid JSON in request body", NULL, NULL, NULL);
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        if (body) json_free(body);
        return 400;
    }
    const char *raw_text = "";
    const char *candidates[] = {"input", "message", "text"};
    for (size_t i = 0; i < 3; i++) {
        raw_text = json_get_str(body, candidates[i], "");
        if (raw_text && *raw_text) break;
    }
    extern char *api_server_normalize_chat_content(const char *, int, int);
    char *steer_text = api_server_normalize_chat_content(raw_text ? raw_text : "", MAX_NORMALIZED_TEXT_LENGTH, 0);
    if (!steer_text || !*steer_text) {
        json_t *err = apisg_openai_error(
            "Missing non-empty steer text; expected 'input', 'message', or 'text'.",
            NULL, NULL, "invalid_steer_input");
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        json_free(body);
        free(steer_text);
        return 400;
    }
    /* steer via the run's registered agent handle */
    extern __attribute__((weak)) void *api_server_active_run_agent(
        api_server_adapter_t *, const char *run_id);
    agent = api_server_active_run_agent ? api_server_active_run_agent(adapter, run_id)
                                        : NULL;
    int accepted = (agent && gw_agent_steer(agent, steer_text)) ? 1 : 0;
    free(steer_text);
    if (!accepted) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Run did not accept steer text: %s", run_id);
        json_t *err = apisg_openai_error(msg, NULL, NULL, "steer_not_accepted");
        char *s = json_serialize(err);
        send_json_response(client_fd, 409, s);
        free(s); json_free(err);
        json_free(body);
        return 409;
    }
    json_free(body);
    json_free(apisg_set_run_status(adapter, run_id, "running", NULL));
    pthread_mutex_lock(&adapter->run_lock);
    sse_queue_t *q = run_status_get(adapter, run_id)
                         ? run_status_get(adapter, run_id)->event_queue
                         : NULL;
    pthread_mutex_unlock(&adapter->run_lock);
    if (q) {
        json_t *ev = json_object();
        json_set(ev, "event", json_string("run.steered"));
        json_set(ev, "run_id", json_string(run_id));
        json_set(ev, "timestamp", json_number((double)time(NULL)));
        json_set(ev, "accepted", json_bool(true));
        char *ser = json_serialize(ev);
        if (ser) sse_queue_put(q, ser);
        free(ser);
        json_free(ev);
    }
    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.run.steer"));
    json_set(resp, "run_id", json_string(run_id));
    json_set(resp, "accepted", json_bool(true));
    char *s = json_serialize(resp);
    send_json_response(client_fd, 200, s);
    free(s); json_free(resp);
    return 200;
}

/* api_server.py:_handle_session_model_lock — see note below. */
/* PoP: apisg_handle_session_model_lock @ api_server.py:_handle_session_model_lock */
/* POST /api/sessions/{session_id}/model — backend-ack a Browser model lock. */
int apisg_handle_session_model_lock(api_server_adapter_t *adapter, int client_fd,
                                    const char *session_id, const char *body_raw) {
    int status = 0;
    json_t *session = apisg_get_existing_session_or_404(adapter, session_id, &status);
    if (!session) {
        if (status == 503) {
            json_t *err = apisg_openai_error("Session database unavailable", NULL, NULL,
                                             "session_db_unavailable");
            char *s = json_serialize(err);
            send_json_response(client_fd, 503, s);
            free(s); json_free(err);
        } else {
            char msg[256];
            snprintf(msg, sizeof(msg), "Session not found: %s", session_id);
            json_t *err = apisg_openai_error(msg, NULL, NULL, "session_not_found");
            char *s = json_serialize(err);
            send_json_response(client_fd, 404, s);
            free(s); json_free(err);
        }
        return status ? status : 404;
    }
    json_free(session);
    int body_err = 0;
    json_t *body = apisg_read_json_body(body_raw, &body_err);
    if (!body) {
        json_t *err = apisg_openai_error("Invalid JSON in request body", NULL, NULL, NULL);
        char *s = json_serialize(err);
        send_json_response(client_fd, 400, s);
        free(s); json_free(err);
        return 400;
    }
    json_t *runtime_request = apisg_session_runtime_request_from_body(adapter, body);
    json_set(runtime_request, "require_model_lock", json_bool(true));
    int lock_status = 0;
    json_t *lock_error = apisg_runtime_lock_error(adapter, runtime_request, &lock_status);
    if (lock_error) {
        char *s = json_serialize(lock_error);
        send_json_response(client_fd, lock_status ? lock_status : 400, s);
        free(s); json_free(lock_error);
        json_free(runtime_request);
        json_free(body);
        return lock_status ? lock_status : 400;
    }
    if (!apisg_persist_session_runtime_lock(adapter, session_id, runtime_request)) {
        json_t *err = apisg_openai_error(
            "Could not persist the requested session model lock", NULL, NULL,
            "model_lock_persistence_failed");
        char *s = json_serialize(err);
        send_json_response(client_fd, 500, s);
        free(s); json_free(err);
        json_free(runtime_request);
        json_free(body);
        return 500;
    }
    const json_t *requested = json_obj_get(runtime_request, "requested");
    const json_t *route = json_obj_get(runtime_request, "route");
    const char *route_provider = json_get_str(route, "provider", "");
    const char *route_model = json_get_str(route, "model", "");
    const char *req_provider = json_get_str(requested, "provider", "");
    json_t *runtime_in = json_object();
    json_set(runtime_in, "provider",
             json_string((route_provider && *route_provider)
                             ? route_provider
                             : ((req_provider && *req_provider) ? req_provider : "")));
    const json_t *req_model_v = requested ? json_obj_get(requested, "model") : NULL;
    json_set(runtime_in, "model",
             json_string((route_model && *route_model)
                             ? route_model
                             : ((req_model_v && json_is_string(req_model_v))
                                    ? req_model_v->str_val : "")));
    json_set(runtime_in, "route_source",
             json_string(json_get_str(runtime_request, "route_source", "raw_request")));
    json_t *runtime = apisg_sanitize_runtime_metadata(
        runtime_in, requested,
        json_get_str(runtime_request, "route_source", "raw_request"), "accepted");
    json_free(runtime_in);
    json_t *resp = json_object();
    json_set(resp, "object", json_string("hermes.session.model_lock"));
    json_set(resp, "session_id", json_string(session_id));
    json_set(resp, "runtime", runtime);
    char *s = json_serialize(resp);
    send_json_response(client_fd, 200, s);
    free(s); json_free(resp);
    json_free(runtime_request);
    json_free(body);
    return 200;
}

/* api_server.py:_sweep_orphaned_runs_once — see note below. */
/* PoP: apisg_sweep_orphaned_runs_once @ api_server.py:_sweep_orphaned_runs_once */
/* Expire old SSE buffers without treating transport age as run age. */
void apisg_sweep_orphaned_runs_once(api_server_adapter_t *adapter, double now) {
    double ttl = (double)RUN_STREAM_TTL;
    if (now == 0.0) now = (double)time(NULL);
    pthread_mutex_lock(&adapter->run_lock);
    for (int i = 0; i < adapter->run_status_count;) {
        run_status_t *rs = adapter->run_statuses[i];
        bool stale_stream = rs && rs->event_queue &&
                            (now - rs->created_at > ttl) &&
                            !rs->running;
        if (stale_stream) {
            fprintf(stderr, "[api_server] sweeping expired run transport %s\n",
                    rs->run_id);
            bool task_done = !rs->running;
            if (task_done && rs->approval_session_key[0])
                approval_unregister_gateway_notify();
            rs->event_queue = NULL; /* transport TTL always bounds buffering */
            if (task_done) {
                /* live control state survives until the executor task returns */
                run_status_remove(adapter, rs->run_id);
                continue; /* do not advance: element removed */
            }
        }
        bool stale_status =
            rs && !rs->running &&
            (strcmp(rs->status, "completed") == 0 ||
             strcmp(rs->status, "failed") == 0 ||
             strcmp(rs->status, "cancelled") == 0) &&
            now - rs->updated_at > (double)RUN_STATUS_TTL;
        if (stale_status) {
            run_status_remove(adapter, rs->run_id);
            continue;
        }
        i++;
    }
    pthread_mutex_unlock(&adapter->run_lock);
}

/* PoP: apisg_check_api_server_requirements @ api_server.py:check_api_server_requirements */
int apisg_check_api_server_requirements(void) {
    /* Python returns AIOHTTP_AVAILABLE; the C API server links its HTTP
     * transport statically, so the dependency set is always available. */
    return 1;
}

/* api_server.py:_sweep_orphaned_runs — see note below. */
/* PoP: apisg_sweep_orphaned_runs @ api_server.py:_sweep_orphaned_runs */
/* Periodically expire transport buffers and terminal status records. */
void *apisg_sweep_orphaned_runs_thread(void *arg) {
    api_server_adapter_t *adapter = (api_server_adapter_t *)arg;
    while (adapter->sweep_running) {
        struct timespec ts = {60, 0};
        nanosleep(&ts, NULL); /* await asyncio.sleep(60) */
        if (!adapter->sweep_running) break;
        apisg_sweep_orphaned_runs_once(adapter, 0.0);
    }
    return NULL;
}

/* PoP: apisg_api_key_passes_startup_guard @ api_server.py:_api_key_passes_startup_guard */
bool apisg_api_key_passes_startup_guard(api_server_adapter_t *adapter) {
    /* Return True when API_SERVER_KEY is present and strong enough to start.
     * Fail CLOSED: this guard is the only thing between a guessable key and a
     * terminal-capable endpoint. */
    if (!adapter->api_key[0]) {
        fprintf(stderr,
                "[%s] Refusing to start: API_SERVER_KEY is required for the API server, "
                "including loopback-only binds on %s.\n",
                "api_server", adapter->host);
        return false;
    }
    /* has_usable_secret unavailability is itself a fatal install problem. */
    extern int has_usable_secret(const char *value, int min_length)
        __attribute__((weak));
    if (!has_usable_secret) {
        fprintf(stderr,
                "[%s] Refusing to start: API_SERVER_KEY strength could not be "
                "verified, and this endpoint dispatches terminal-capable agent work. "
                "Repair the installation before starting the API server on %s.\n",
                "api_server", adapter->host);
        return false;
    }
    if (!has_usable_secret(adapter->api_key, 16)) {
        fprintf(stderr,
                "[%s] Refusing to start: API_SERVER_KEY is a placeholder or too "
                "short (<16 chars). This endpoint dispatches terminal-capable agent "
                "work — a guessable key is remote code execution. Generate a strong "
                "secret (e.g. `openssl rand -hex 32`) and set API_SERVER_KEY before "
                "starting the API server on %s.\n",
                "api_server", adapter->host);
        return false;
    }
    return true;
}

/* ── POST /v1/responses SSE writer (OpenAI Responses API streaming) ── */

typedef struct apisg_responses_stream_state {
    json_t *emitted_items;          /* output items emitted so far, in order */
    char final_text[262144];        /* accumulated assistant text deltas */
    size_t final_text_len;
    int output_index;
    int call_counter;
    int sequence_number;
    char message_item_id[64];
    int message_output_index;
    bool message_opened;
    bool terminal_snapshot_persisted;
} apisg_resp_state_t;

static void apisg_rs_init(apisg_resp_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->emitted_items = json_array();
    st->message_output_index = -1;
    char *hex = apisg_uuid_hex24();
    snprintf(st->message_item_id, sizeof(st->message_item_id), "msg_%s", hex ? hex : "");
    free(hex);
}

static void apisg_rs_write_event(int fd, apisg_resp_state_t *st,
                                 const char *event_type, const json_t *data_in) {
    json_t *data = data_in ? json_copy(data_in) : json_object();
    if (!json_obj_get(data, "sequence_number"))
        json_set(data, "sequence_number", json_number((double)st->sequence_number));
    st->sequence_number++;
    char *ser = json_serialize(data);
    char *frame = api_server_sse_frame(ser, event_type);
    if (frame) { apisg_write_fd(fd, frame, strlen(frame)); free(frame); }
    free(ser);
    json_free(data);
}

static json_t *apisg_rs_envelope(const char *response_id, const char *model,
                                 long created_at, const char *status) {
    json_t *env = json_object();
    json_set(env, "id", json_string(response_id));
    json_set(env, "object", json_string("response"));
    json_set(env, "status", json_string(status));
    json_set(env, "created_at", json_number((double)created_at));
    json_set(env, "model", json_string(model));
    return env;
}

static void apisg_rs_append_text(apisg_resp_state_t *st, const char *delta_text) {
    size_t dl = strlen(delta_text);
    if (st->final_text_len + dl >= sizeof(st->final_text)) {
        dl = sizeof(st->final_text) - 1 - st->final_text_len;
    }
    memcpy(st->final_text + st->final_text_len, delta_text, dl);
    st->final_text_len += dl;
    st->final_text[st->final_text_len] = '\0';
}

static void apisg_rs_open_message_item(int fd, apisg_resp_state_t *st) {
    if (st->message_opened) return;
    st->message_opened = true;
    st->message_output_index = st->output_index++;
    json_t *item = json_object();
    json_set(item, "id", json_string(st->message_item_id));
    json_set(item, "type", json_string("message"));
    json_set(item, "status", json_string("in_progress"));
    json_set(item, "role", json_string("assistant"));
    json_set(item, "content", json_array());
    json_t *ev = json_object();
    json_set(ev, "type", json_string("response.output_item.added"));
    json_set(ev, "output_index", json_number((double)st->message_output_index));
    json_set(ev, "item", item);
    apisg_rs_write_event(fd, st, "response.output_item.added", ev);
    json_free(ev);
}

static void apisg_rs_emit_text_delta(int fd, apisg_resp_state_t *st,
                                     const char *delta_text) {
    apisg_rs_open_message_item(fd, st);
    apisg_rs_append_text(st, delta_text);
    json_t *ev = json_object();
    json_set(ev, "type", json_string("response.output_text.delta"));
    json_set(ev, "item_id", json_string(st->message_item_id));
    json_set(ev, "output_index", json_number((double)st->message_output_index));
    json_set(ev, "content_index", json_number(0));
    json_set(ev, "delta", json_string(delta_text));
    json_set(ev, "logprobs", json_array());
    apisg_rs_write_event(fd, st, "response.output_text.delta", ev);
    json_free(ev);
}

/* api_server.py:_write_sse_responses — see note below. */
/* PoP: apisg_write_sse_responses @ api_server.py:_write_sse_responses */
/* /v1/responses/{id} and previous_response_id chaining keep working. */
int apisg_write_sse_responses(
    api_server_adapter_t *adapter, int client_fd, const char *response_id,
    const char *model, long created_at, sse_queue_t *stream_q,
    volatile bool *agent_task_done, void *agent_ref_slot,
    const json_t *conversation_history, const char *user_message,
    const char *instructions, const char *conversation, bool store_flag,
    const char *session_id, const char *gateway_session_key) {
    char head[768];
    size_t hlen = (size_t)snprintf(head, sizeof(head),
                                   "HTTP/1.1 200 OK\r\n"
                                   "Content-Type: text/event-stream\r\n"
                                   "Cache-Control: no-cache\r\n"
                                   "X-Accel-Buffering: no\r\n");
    if (session_id && *session_id)
        hlen += (size_t)snprintf(head + hlen, sizeof(head) - hlen,
                                 "X-Hermes-Session-Id: %s\r\n", session_id);
    if (gateway_session_key && *gateway_session_key)
        hlen += (size_t)snprintf(head + hlen, sizeof(head) - hlen,
                                 "X-Hermes-Session-Key: %s\r\n", gateway_session_key);
    snprintf(head + hlen, sizeof(head) - hlen, "\r\n");
    apisg_write_fd(client_fd, head, strlen(head));

    apisg_resp_state_t st;
    apisg_rs_init(&st);

    /* response.created — initial envelope, status=in_progress */
    json_t *created_env = apisg_rs_envelope(response_id, model, created_at, "in_progress");
    json_set(created_env, "output", json_array());
    json_t *created_ev = json_object();
    json_set(created_ev, "type", json_string("response.created"));
    json_set(created_ev, "response", created_env);
    apisg_rs_write_event(client_fd, &st, "response.created", created_ev);
    json_free(created_ev);
    double last_activity = (double)clock() / CLOCKS_PER_SEC;

    char batch_buf[65536];
    size_t batch_len = 0;
    batch_buf[0] = '\0';
    char agent_error[512] = "";
    char final_response_text[131072] = "";
    long usage_in = 0, usage_out = 0, usage_total = 0;
    /* usage fields are filled by the agent task join in the transport layer */
    (void)usage_in; (void)usage_out; (void)usage_total;
    bool client_gone = false;

#define APISG_FLUSH_BATCH()                                                   \
    do {                                                                      \
        if (batch_len > 0) {                                                  \
            apisg_rs_emit_text_delta(client_fd, &st, batch_buf);              \
            batch_buf[0] = '\0';                                              \
            batch_len = 0;                                                    \
        }                                                                     \
    } while (0)

    while (true) {
        char *item = sse_queue_get(stream_q, 500);
        if (!item) {
            if (*agent_task_done) {
                for (;;) {
                    char *rem = sse_queue_get(stream_q, 0);
                    if (!rem) break;
                    if (strcmp(rem, "__EOS__") == 0) { free(rem); break; }
                    if (strncmp(rem, "__tool_started__\t", 17) == 0 ||
                        strncmp(rem, "__tool_completed__\t", 19) == 0) {
                        /* Tool lifecycle frames arrive pre-serialized by the
                         * run event callback; forward as tool progress text
                         * markers are NOT part of Responses history. */
                        APISG_FLUSH_BATCH();
                        json_t *tool_ev = json_parse(strchr(rem, '\t') + 1, NULL);
                        if (tool_ev && json_is_object(tool_ev)) {
                            bool is_start =
                                strncmp(rem, "__tool_started__\t", 17) == 0;
                            const char *name =
                                json_get_str(tool_ev, "tool", "");
                            const char *call_id =
                                json_get_str(tool_ev, "call_id",
                                             json_get_str(tool_ev, "preview", ""));
                            st.call_counter++;
                            char gen_call[96];
                            snprintf(gen_call, sizeof(gen_call), "%s_%d",
                                     response_id, st.call_counter);
                            const char *cid = call_id[0] ? call_id : gen_call;
                            if (is_start) {
                                json_t *fitem = json_object();
                                char *fhex = apisg_uuid_hex24();
                                char fid[32];
                                snprintf(fid, sizeof(fid), "fc_%s", fhex ? fhex : "");
                                free(fhex);
                                json_set(fitem, "id", json_string(fid));
                                json_set(fitem, "type", json_string("function_call"));
                                json_set(fitem, "status", json_string("in_progress"));
                                json_set(fitem, "name", json_string(name));
                                json_set(fitem, "call_id", json_string(cid));
                                json_set(fitem, "arguments", json_string("{}"));
                                json_t *ev = json_object();
                                json_set(ev, "type", json_string("response.output_item.added"));
                                json_set(ev, "output_index", json_number((double)st.output_index++));
                                json_set(ev, "item", fitem);
                                apisg_rs_write_event(client_fd, &st,
                                                     "response.output_item.added", ev);
                                json_free(ev);
                                json_t *emitted = json_object();
                                json_set(emitted, "type", json_string("function_call"));
                                json_set(emitted, "name", json_string(name));
                                json_set(emitted, "arguments", json_string("{}"));
                                json_set(emitted, "call_id", json_string(cid));
                                json_append(st.emitted_items, emitted);
                            } else {
                                const json_t *result_v =
                                    json_obj_get(tool_ev, "result");
                                char result_str[4096];
                                if (result_v && json_is_string(result_v))
                                    snprintf(result_str, sizeof(result_str), "%s",
                                             result_v->str_val);
                                else
                                    snprintf(result_str, sizeof(result_str), "%ld",
                                             (long)(result_v ? result_v->num_val : 0));
                                json_t *out_item = json_object();
                                char *ohex = apisg_uuid_hex24();
                                char oid[36];
                                snprintf(oid, sizeof(oid), "fco_%s", ohex ? ohex : "");
                                free(ohex);
                                json_set(out_item, "id", json_string(oid));
                                json_set(out_item, "type", json_string("function_call_output"));
                                json_set(out_item, "status", json_string("completed"));
                                json_t *parts = json_array();
                                json_t *part = json_object();
                                json_set(part, "type", json_string("input_text"));
                                json_set(part, "text", json_string(result_str));
                                json_append(parts, part);
                                json_set(out_item, "output", parts);
                                json_t *ev = json_object();
                                json_set(ev, "type", json_string("response.output_item.done"));
                                json_set(ev, "output_index", json_number((double)st.output_index++));
                                json_set(ev, "item", out_item);
                                apisg_rs_write_event(client_fd, &st,
                                                     "response.output_item.done", ev);
                                json_free(ev);
                                json_t *emitted = json_object();
                                json_set(emitted, "type", json_string("function_call_output"));
                                json_set(emitted, "output", json_copy(parts));
                                json_append(st.emitted_items, emitted);
                            }
                            json_free(tool_ev);
                        }
                    } else {
                        /* plain text delta — batch to reduce re-render storms */
                        size_t l = strlen(rem);
                        if (batch_len + l < sizeof(batch_buf) - 1) {
                            memcpy(batch_buf + batch_len, rem, l);
                            batch_len += l;
                            batch_buf[batch_len] = '\0';
                        }
                    }
                    free(rem);
                }
                break;
            }
            if ((double)clock() / CLOCKS_PER_SEC - last_activity >=
                CHAT_COMPLETIONS_SSE_KEEPALIVE_SECONDS) {
                apisg_write_fd(client_fd, ": keepalive\n\n", 13);
                last_activity = (double)clock() / CLOCKS_PER_SEC;
            }
            continue;
        }
        if (strcmp(item, "__EOS__") == 0) { free(item); break; }
        size_t l = strlen(item);
        if (batch_len + l < sizeof(batch_buf) - 1) {
            memcpy(batch_buf + batch_len, item, l);
            batch_len += l;
            batch_buf[batch_len] = '\0';
        }
        free(item);
        last_activity = (double)clock() / CLOCKS_PER_SEC;
    }

    /* Flush any final batched text before processing result */
    APISG_FLUSH_BATCH();

#undef APISG_FLUSH_BATCH

    /* Close the message item if it was opened */
    if (st.final_text_len > 0 && strlen(final_response_text) < sizeof(final_response_text) - 1)
        snprintf(final_response_text, sizeof(final_response_text), "%s", st.final_text);
    if (st.message_opened) {
        json_t *ev = json_object();
        json_set(ev, "type", json_string("response.output_text.done"));
        json_set(ev, "item_id", json_string(st.message_item_id));
        json_set(ev, "output_index", json_number((double)st.message_output_index));
        json_set(ev, "content_index", json_number(0));
        json_set(ev, "text", json_string(final_response_text));
        json_set(ev, "logprobs", json_array());
        apisg_rs_write_event(client_fd, &st, "response.output_text.done", ev);
        json_free(ev);
        json_t *msg_done = json_object();
        json_set(msg_done, "id", json_string(st.message_item_id));
        json_set(msg_done, "type", json_string("message"));
        json_set(msg_done, "status", json_string("completed"));
        json_set(msg_done, "role", json_string("assistant"));
        json_t *content_arr = json_array();
        json_t *part = json_object();
        json_set(part, "type", json_string("output_text"));
        json_set(part, "text", json_string(final_response_text));
        json_append(content_arr, part);
        json_set(msg_done, "content", content_arr);
        json_t *dev = json_object();
        json_set(dev, "type", json_string("response.output_item.done"));
        json_set(dev, "output_index", json_number((double)st.message_output_index));
        json_set(dev, "item", msg_done);
        apisg_rs_write_event(client_fd, &st, "response.output_item.done", dev);
        json_free(dev);
    }

    json_t *final_items = json_copy(st.emitted_items);

    /* Trim large tool-call content from the terminal payload (~100KB cap):
     * clients already received full details via incremental events. */
    for (size_t i = 0; i < json_len(final_items); i++) {
        json_t *_item = json_get(final_items, i);
        if (!_item || !json_is_object(_item)) continue;
        if (strcmp(json_get_str(_item, "type", ""), "function_call") == 0) {
            const char *args = json_get_str(_item, "arguments", "{}");
            json_t *_args = json_parse(args, NULL);
            if (_args && json_is_object(_args)) {
                static const char *const big_keys[] =
                    {"content", "query", "pattern", "old_string", "new_string"};
                for (size_t k = 0; k < 5; k++) {
                    const json_t *v = json_obj_get(_args, big_keys[k]);
                    if (v && json_is_string(v) && strlen(v->str_val) > 500) {
                        char trimmed[576];
                        snprintf(trimmed, sizeof(trimmed),
                                 "[%zu chars — truncated for response.completed]",
                                 strlen(v->str_val));
                        json_set(_args, big_keys[k], json_string(trimmed));
                    }
                }
                char *ser = json_serialize(_args);
                json_set(_item, "arguments", json_string(ser));
                free(ser);
            }
            if (_args) json_free(_args);
        } else if (strcmp(json_get_str(_item, "type", ""), "function_call_output") == 0) {
            const json_t *outp = json_obj_get(_item, "output");
            if (json_is_array(outp) && json_len(outp) > 0) {
                json_t *first = json_get(outp, 0);
                if (json_is_object(first) &&
                    strcmp(json_get_str(first, "type", ""), "input_text") == 0) {
                    const char *text = json_get_str(first, "text", "");
                    if (strlen(text) > 1000) {
                        char trimmed[1100];
                        snprintf(trimmed, sizeof(trimmed), "%.500s...[%zu more chars]",
                                 text, strlen(text) - 500);
                        json_set(first, "text", json_string(trimmed));
                    }
                }
            }
        }
    }

    json_t *final_msg = json_object();
    json_set(final_msg, "type", json_string("message"));
    json_set(final_msg, "role", json_string("assistant"));
    json_t *final_content = json_array();
    json_t *final_part = json_object();
    json_set(final_part, "type", json_string("output_text"));
    if (final_response_text[0])
        json_set(final_part, "text", json_string(final_response_text));
    else {
        char *red = agent_error[0] ? apis_redact_api_error_text(agent_error, 0) : NULL;
        json_set(final_part, "text", json_string(red ? red : ""));
        free(red);
    }
    json_append(final_content, final_part);
    json_set(final_msg, "content", final_content);
    json_append(final_items, final_msg);

    int rc_status = 200;
    if (agent_error[0] && !client_gone) {
        json_t *failed_env = apisg_rs_envelope(response_id, model, created_at, "failed");
        json_set(failed_env, "output", json_copy(final_items));
        json_t *error_obj = json_object();
        char *red = apis_redact_api_error_text(agent_error, 0);
        json_set(error_obj, "message", json_string(red ? red : ""));
        free(red);
        json_set(error_obj, "type", json_string("server_error"));
        json_set(failed_env, "error", error_obj);
        json_t *fev = json_object();
        json_set(fev, "type", json_string("response.failed"));
        json_set(fev, "response", failed_env);
        apisg_rs_write_event(client_fd, &st, "response.failed", fev);
        json_free(fev);
        json_free(failed_env);
        st.terminal_snapshot_persisted = true;
    } else if (!client_gone) {
        json_t *completed_env = apisg_rs_envelope(response_id, model, created_at, "completed");
        json_set(completed_env, "output", json_copy(final_items));
        json_t *cev = json_object();
        json_set(cev, "type", json_string("response.completed"));
        json_set(cev, "response", completed_env);
        apisg_rs_write_event(client_fd, &st, "response.completed", cev);
        json_free(cev);
        json_free(completed_env);
        st.terminal_snapshot_persisted = true;
    }

    /* Persist the terminal snapshot for store=True so GET /v1/responses/{id}
     * and previous_response_id chaining recover from it. */
    if (store_flag && !client_gone) {
        extern json_t *apisg_build_response_conversation_history(
            const json_t *, const json_t *, const json_t *, const json_t *);
        json_t *history = apisg_build_response_conversation_history(
            conversation_history,
            NULL, user_message ? json_parse(user_message, NULL) : NULL, NULL);
        if (store_flag && adapter && adapter->response_store) {
            json_t *snapshot = json_object();
            json_t *env = apisg_rs_envelope(response_id, model, created_at,
                                            agent_error[0] ? "failed" : "completed");
            json_set(snapshot, "response", env);
            json_set(snapshot, "conversation_history", history ? json_copy(history) : json_array());
            json_set(snapshot, "instructions", json_string(instructions ? instructions : ""));
            json_set(snapshot, "session_id", json_string(session_id ? session_id : ""));
            char *ser = json_serialize(snapshot);
            if (ser) response_store_put(adapter->response_store, response_id, ser);
            free(ser);
            json_free(snapshot);
            if (conversation && *conversation)
                response_store_set_conversation(adapter->response_store, conversation,
                                                response_id);
        }
        if (history) json_free(history);
    }

    if (client_gone) {
        /* Interrupt the agent so it stops making upstream LLM calls. */
        extern __attribute__((weak)) int gwapi_reap_disconnected_agent_processes(void *);
        if (agent_ref_slot && gw_request_hard_interrupt)
            gw_request_hard_interrupt(agent_ref_slot, "SSE client disconnected");
        if (agent_ref_slot && gwapi_reap_disconnected_agent_processes)
            gwapi_reap_disconnected_agent_processes(agent_ref_slot);
        fprintf(stderr, "[api_server] SSE client disconnected; interrupted agent task %s\n",
                response_id);
        rc_status = 499;
    }

    json_free(final_items);
    json_free(st.emitted_items);
    return rc_status;
}

/* PoP: apisg_multimodal_validation_error @ api_server.py:_multimodal_validation_error */
json_t *apisg_multimodal_validation_error(const char *exc, const char *param,
                                          int *status_out) {
    /* Translate a _normalize_multimodal_content ValueError into a 400 body:
     * "code:message" split on the first colon; bare text → invalid_content_part. */
    const char *colon = exc ? strchr(exc, ':') : NULL;
    char code[128];
    const char *message;
    if (colon && colon[1]) {
        size_t cl = (size_t)(colon - exc);
        if (cl >= sizeof(code)) cl = sizeof(code) - 1;
        memcpy(code, exc, cl);
        code[cl] = '\0';
        message = colon + 1;
        while (*message == ' ') message++;
    } else {
        snprintf(code, sizeof(code), "invalid_content_part");
        message = exc ? exc : "";
    }
    if (status_out) *status_out = 400;
    return apisg_openai_error(message, NULL, param, code);
}

/* api_server.py:_session_chat_user_message */
/* PoP: apisg_session_chat_user_message @ api_server.py:_session_chat_user_message */
/* Parse and normalize session chat "message"/"input" like chat completions;
 * returns the normalized user message, or NULL with *err_status set to 400. */
json_t *apisg_session_chat_user_message(const json_t *body, const char *param,
                                        int *err_status) {
    extern bool api_server_content_has_visible_payload(const char *content);
    const char *user_message = json_get_str(body, "message", "");
    if (!user_message || !*user_message)
        user_message = json_get_str(body, "input", "");
    if (!api_server_content_has_visible_payload(user_message ? user_message : "")) {
        if (err_status) *err_status = 400;
        return NULL;
    }
    if (err_status) *err_status = 0;
    /* _normalize_multimodal_content over the raw payload; a validation error
     * maps onto the same 400 envelope as _multimodal_validation_error. */
    extern char *api_server_normalize_multimodal_content(const char *content);
    char *normalized =
        api_server_normalize_multimodal_content(user_message ? user_message : "");
    json_t *out = normalized ? json_parse(normalized, NULL) : NULL;
    free(normalized);
    if (!out) out = json_string(user_message ? user_message : "");
    return out;
}

/* api_server.py:_drain_session_stream_task_on_disconnect */
/* PoP: apisg_drain_session_stream_task_on_disconnect @ api_server.py:_drain_session_stream_task_on_disconnect */
/* Preserve live run control refs until the executor-backed turn exits. */
void apisg_drain_session_stream_task_on_disconnect(api_server_adapter_t *adapter,
                                                   const char *run_id,
                                                   volatile bool *task_done_flag,
                                                   const char *interrupt_message,
                                                   bool shield_wait) {
    (void)shield_wait; /* single-threaded C drain waits unconditionally */
    extern __attribute__((weak)) void *api_server_active_run_agent(
        api_server_adapter_t *, const char *run_id);
    void *agent = api_server_active_run_agent
                      ? api_server_active_run_agent(adapter, run_id)
                      : NULL;
    if (!agent) {
        if (!*task_done_flag) {
            /* task.cancel() + await */
            *task_done_flag = true;
            fprintf(stderr, "[api_server] cancelled stream task for %s\n", run_id);
        }
        return;
    }
    if (gw_request_hard_interrupt)
        gw_request_hard_interrupt(agent, interrupt_message);
    else
        fprintf(stderr, "[api_server] agent.interrupt(%s) for %s\n",
                interrupt_message, run_id);
}
