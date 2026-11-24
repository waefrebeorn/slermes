/*
 * hosted_room_peer_http.c — C11 home for upstream Hermes
 * tui_gateway/hosted_room_peer_http.py.
 *
 * Two gates share this one translation unit and split the module's PURE
 * surface (request/URL/header/body building, response parsing + error mapping,
 * retry/backoff math, scope/authority checks, predicates):
 *
 *   gate w31_hostedroom_50fa41_p1 — request building + error classification
 *   gate w31_hostedroom_50fa41_p2 — session/run state-machine decisions
 *
 * SEAMS (never performed here; their answers arrive in the fixture object):
 *   urllib.request.Request / open_credentialed_url  -> in["response"] carrier
 *   response.read1, Content-Length, settimeout      -> in["raw"], in["declared"]
 *   time.monotonic                                  -> in["now"] / deadline
 *   PeerRunsHTTPClient._request                     -> in["admit"] / in["fetched"]
 *   PeerRunsHTTPClient._admit_dispatch (POST)       -> in["admit"]
 *   PeerRunsHTTPClient._scoped_post                 -> in["result"]
 *   PeerRunsHTTPClient._post_run_action             -> in["action"]
 *   PeerRunsHTTPClient._poll_receipt                -> in["status"] (history/status)
 *   PeerRunsHTTPClient._session_id                  -> in["session"] (close)
 *   receipt store (hosted_rooms.*_remote_run_receipt)-> in["record"], "seam" marker
 *   HostedMemberDispatch.from_mapping validation    -> fixture dispatch is valid
 *   validate_room_link_url                          -> base_url arrives validated
 *   hmac.compare_digest (catalog digest)            -> in["catalog"] arrives built
 *
 * PoP (one per exported symbol):
 *   w31_hostedroom_50fa41_p1_valid_code          @ hosted_room_peer_http.py:_valid_code
 *   w31_hostedroom_50fa41_p1_response_error_code @ hosted_room_peer_http.py:_response_error_code
 *   w31_hostedroom_50fa41_p1_run_path            @ hosted_room_peer_http.py:_run_path
 *   w31_hostedroom_50fa41_p1_run_status          @ hosted_room_peer_http.py:_run_status
 *   w31_hostedroom_50fa41_p1_next_poll_delay     @ hosted_room_peer_http.py:_next_poll_delay
 *   w31_hostedroom_50fa41_p1_require_room_grant  @ hosted_room_peer_http.py:_require_room_grant
 *   w31_hostedroom_50fa41_p1_request             @ hosted_room_peer_http.py:PeerRunsHTTPClient._request
 *   w31_hostedroom_50fa41_p1_admission_body      @ hosted_room_peer_http.py:PeerRunsHTTPClient._admit_dispatch
 *   w31_hostedroom_50fa41_p1_invitation_body     @ hosted_room_peer_http.py:PeerRunsHTTPClient.issue_invitation
 *   w31_hostedroom_50fa41_p1_digest_check        @ hosted_room_peer_http.py:digest_reauthorization_error
 *   w31_hostedroom_50fa41_p1_pre_admission       @ hosted_room_peer_http.py:_is_proven_pre_admission_failure
 *   w31_hostedroom_50fa41_p1_http_error          @ hosted_room_peer_http.py:PeerRunsHTTPClient._raise_http_error
 *   w31_hostedroom_50fa41_p1_budget_error        @ hosted_room_peer_http.py:_read_body
 *   w31_hostedroom_50fa41_p2_api_checks          @ hosted_room_peer_http.py:PeerRunsHTTPClient.__init__
 *   w31_hostedroom_50fa41_p2_prepare             @ hosted_room_peer_http.py:PeerRunsHTTPClient.prepare
 *   w31_hostedroom_50fa41_p2_room_scope          @ hosted_room_peer_http.py:PeerRunsHTTPClient.bind_room_scope
 *   w31_hostedroom_50fa41_p2_accepted            @ hosted_room_peer_http.py:PeerRunsHTTPClient._accepted
 *   w31_hostedroom_50fa41_p2_receipt_record      @ hosted_room_peer_http.py:PeerRunsHTTPClient._save_receipt
 *   w31_hostedroom_50fa41_p2_session_id_choice   @ hosted_room_peer_http.py:PeerRunsHTTPClient._session_id
 *   w31_hostedroom_50fa41_p2_close_result        @ hosted_room_peer_http.py:PeerRunsHTTPClient.close_admission
 *   w31_hostedroom_50fa41_p2_recover_decision    @ hosted_room_peer_http.py:PeerRunsHTTPClient.recover_dispatch
 *   w31_hostedroom_50fa41_p2_poll_cache          @ hosted_room_peer_http.py:PeerRunsHTTPClient._poll_receipt
 *   w31_hostedroom_50fa41_p2_observation_receipt @ hosted_room_peer_http.py:PeerRunsHTTPClient._observation_receipt
 *   w31_hostedroom_50fa41_p2_history_view        @ hosted_room_peer_http.py:PeerRunsHTTPClient.history
 *   w31_hostedroom_50fa41_p2_status_view         @ hosted_room_peer_http.py:PeerRunsHTTPClient.status
 *   w31_hostedroom_50fa41_p2_approve_decision    @ hosted_room_peer_http.py:PeerRunsHTTPClient.approve_receipt
 *   w31_hostedroom_50fa41_p2_forget_decision     @ hosted_room_peer_http.py:PeerRunsHTTPClient.forget_receipt
 *   w31_hostedroom_50fa41_p2_stop_decision       @ hosted_room_peer_http.py:PeerRunsHTTPClient.stop_receipt
 *   w31_hostedroom_50fa41_p2_refresh_decision    @ hosted_room_peer_http.py:PeerRunsHTTPClient.refresh_receipt
 *   w31_hostedroom_50fa41_p2_refreshed_grant     @ hosted_room_peer_http.py:PeerRunsHTTPClient.refresh_grant
 */
#include "hermes_json.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w31_hostedroom_50fa41_p1.h"
#include "noc_w31_hostedroom_50fa41_p2.h"

/* ====================================================================== */
/* shared value helpers (static: not part of either gate's surface)        */
/* ====================================================================== */

static char *hrp_dup(const char *s)
{
    size_t n;
    char *p;
    if (!s) s = "";
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* Python truthiness of a JSON value. */
static int hrp_truthy(const json_t *v)
{
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    if (v->type == JSON_STRING) return (v->str_val && v->str_val[0]) ? 1 : 0;
    if (v->type == JSON_ARRAY) return json_len(v) > 0 ? 1 : 0;
    if (v->type == JSON_OBJECT) return json_object_size(v) > 0 ? 1 : 0;
    return 0;
}

/* Python str(value). Domain: str/int/bool/null (containers are not modelled). */
static char *hrp_pystr(const json_t *v)
{
    char buf[64];
    if (!v || v->type == JSON_NULL) return hrp_dup("None");
    switch (v->type) {
    case JSON_BOOL:
        return hrp_dup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
            return hrp_dup(buf);
        }
        snprintf(buf, sizeof buf, "%.17g", v->num_val);
        return hrp_dup(buf);
    case JSON_STRING:
        return hrp_dup(v->str_val ? v->str_val : "");
    default:
        return hrp_dup("");
    }
}

/* str(v or "") */
static char *hrp_or_empty_str(const json_t *v)
{
    return hrp_truthy(v) ? hrp_pystr(v) : hrp_dup("");
}

static double hrp_num(const json_t *o, const char *key, double def)
{
    json_t *v = json_obj_get(o, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static int hrp_present(const json_t *o, const char *key)
{
    return (o && o->type == JSON_OBJECT && json_obj_get(o, key) != NULL) ? 1 : 0;
}

static json_t *hrp_copy_of(const json_t *v)
{
    return v ? json_copy(v) : json_null();
}

/* Python int(v or 0). */
static double hrp_int_or0(const json_t *v)
{
    if (!hrp_truthy(v)) return 0.0;
    if (v->type == JSON_NUMBER) return (double)(long long)v->num_val;
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end && end != v->str_val && *end == '\0') return (double)(long long)d;
    }
    return 0.0;
}

/* urllib.parse.quote(s, safe="") over UTF-8 bytes. */
static char *hrp_quote(const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t n = strlen(s ? s : ""), i, k = 0;
    char *out = (char *)malloc(n * 3 + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)(s ? s[i] : 0);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-' || c == '~') {
            out[k++] = (char)c;
        } else {
            out[k++] = '%';
            out[k++] = hex[c >> 4];
            out[k++] = hex[c & 0x0f];
        }
    }
    out[k] = '\0';
    return out;
}

/* ASCII str.strip() */
static char *hrp_strip(const char *s)
{
    size_t n, a = 0, b;
    char *out;
    if (!s) return hrp_dup("");
    n = strlen(s);
    while (a < n && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\f' || s[a] == '\v'))
        a++;
    b = n;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' ||
                     s[b - 1] == '\r' || s[b - 1] == '\f' || s[b - 1] == '\v'))
        b--;
    out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

/* UTF-8 code point count (Python len). */
static size_t hrp_cp_len(const char *s)
{
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

/* First limit code points of s (Python s[:limit]). */
static char *hrp_cp_prefix(const char *s, size_t limit)
{
    size_t i = 0, count = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    char *out;
    while (p[i]) {
        if ((p[i] & 0xC0) != 0x80) {
            if (count == limit) break;
            count++;
        }
        i++;
    }
    out = (char *)malloc(i + 1);
    if (!out) return NULL;
    memcpy(out, p, i);
    out[i] = '\0';
    return out;
}

/* ====================================================================== */
/* PeerRunsHTTPError projection                                            */
/* ====================================================================== */

static int hrp_reauth_code(const char *code)
{
    if (!code) return 0;
    return !strcmp(code, "invalid_room_grant") || !strcmp(code, "room_reauthorization_required") ||
           !strcmp(code, "room_execution_policy_changed") ||
           !strcmp(code, "room_capability_catalog_changed");
}

static json_t *hrp_error_obj(const char *message, int retryable, int ambiguous,
                             int not_admitted, int has_status, double status,
                             const char *error_code)
{
    json_t *e = json_object();
    int needs = 0;
    json_set(e, "message", json_string(message ? message : ""));
    json_set(e, "retryable", json_bool(retryable ? true : false));
    json_set(e, "ambiguous", json_bool(ambiguous ? true : false));
    json_set(e, "not_admitted", json_bool(not_admitted ? true : false));
    json_set(e, "status_code", has_status ? json_number(status) : json_null());
    json_set(e, "error_code", error_code ? json_string(error_code) : json_null());
    if (has_status && (status == 401 || status == 403) && hrp_reauth_code(error_code)) needs = 1;
    json_set(e, "needs_reauthorization", json_bool(needs ? true : false));
    return e;
}

/* Normalise an input error spec ({"message","retryable",...}) to full form. */
static json_t *hrp_error_from_spec(const json_t *spec)
{
    const char *msg = json_get_str(spec, "message", "");
    json_t *st = json_obj_get(spec, "status_code");
    int has = (st && st->type == JSON_NUMBER) ? 1 : 0;
    return hrp_error_obj(msg, json_get_bool(spec, "retryable", false),
                         json_get_bool(spec, "ambiguous", false),
                         json_get_bool(spec, "not_admitted", false), has,
                         has ? st->num_val : 0.0,
                         json_get_str(spec, "error_code", NULL));
}

static json_t *hrp_ok(json_t *value)
{
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", value ? value : json_null());
    return o;
}

static json_t *hrp_fail(json_t *error)
{
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", error ? error : json_null());
    return o;
}

static json_t *hrp_fail_msg(const char *message)
{
    return hrp_fail(hrp_error_obj(message, 0, 0, 0, 0, 0.0, NULL));
}

/* ====================================================================== */
/* error-code extraction                                                   */
/* ====================================================================== */

#define HRP_CODE_MAX 64

static int hrp_code_ok(const char *s)
{
    size_t n, i;
    if (!s) return 0;
    n = strlen(s);
    if (n < 1 || n > HRP_CODE_MAX) return 0;
    if (!(s[0] >= 'a' && s[0] <= 'z')) return 0;
    for (i = 1; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

/* str(error.get("message") or "").lower() */
static char *hrp_message_lower(const json_t *error)
{
    char *s = hrp_or_empty_str(json_obj_get(error, "message")), *p;
    for (p = s; p && *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return s;
}

static char *hrp_response_error_code(const char *detail)
{
    json_t *payload, *error;
    char *code = NULL;
    if (!detail) return NULL;
    payload = json_parse(detail, NULL);
    if (!payload) return NULL;
    if (payload->type != JSON_OBJECT) {
        json_free(payload);
        return NULL;
    }
    error = json_obj_get(payload, "error");
    if (error && error->type == JSON_OBJECT && hrp_present(error, "code") &&
        json_obj_get(error, "code")->type == JSON_STRING) {
        const char *raw = json_obj_get(error, "code")->str_val;
        if (hrp_code_ok(raw)) {
            code = hrp_dup(raw);
            if (code && !strcmp(code, "invalid_room_dispatch")) {
                char *msg = hrp_message_lower(error);
                if (msg) {
                    if (strstr(msg, "room grant")) {
                        free(code);
                        code = hrp_dup("invalid_room_grant");
                    } else if (strstr(msg, "capability catalog changed")) {
                        free(code);
                        code = hrp_dup("room_capability_catalog_changed");
                    } else if (strstr(msg, "execution policy changed")) {
                        free(code);
                        code = hrp_dup("room_execution_policy_changed");
                    }
                }
                free(msg);
            }
        }
    } else {
        const char *raw = json_get_str(payload, "code", NULL);
        if (hrp_code_ok(raw)) code = hrp_dup(raw);
    }
    json_free(payload);
    return code;
}

json_t *w31_hostedroom_50fa41_p1_valid_code(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_valid_code */
    json_t *code = json_obj_get(in, "code");
    if (code && code->type == JSON_STRING && hrp_code_ok(code->str_val))
        return json_string(code->str_val);
    return json_null();
}

json_t *w31_hostedroom_50fa41_p1_response_error_code(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_response_error_code */
    json_t *detail = json_obj_get(in, "detail");
    char *code;
    json_t *out;
    if (!detail || detail->type != JSON_STRING) return json_null();
    code = hrp_response_error_code(detail->str_val);
    out = code ? json_string(code) : json_null();
    free(code);
    return out;
}

/* ====================================================================== */
/* run path / run status                                                   */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p1_run_path(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_run_path */
    char *rid = hrp_pystr(json_obj_get(in, "run_id"));
    char *q = hrp_quote(rid);
    json_t *suffix = json_obj_get(in, "suffix");
    size_t cap = strlen(q) + 32, len, i;
    char *out = (char *)malloc(cap);
    len = (size_t)snprintf(out, cap, "/v1/runs/%s", q);
    if (suffix && suffix->type == JSON_ARRAY) {
        for (i = 0; i < json_len(suffix); i++) {
            char *part = hrp_pystr(json_get(suffix, i));
            size_t need = len + strlen(part) + 2;
            if (need > cap) {
                char *grown = (char *)realloc(out, need);
                if (!grown) {
                    free(part);
                    break;
                }
                out = grown;
                cap = need;
            }
            out[len++] = '/';
            memcpy(out + len, part, strlen(part));
            len += strlen(part);
            out[len] = '\0';
            free(part);
        }
    }
    free(rid);
    free(q);
    {
        json_t *res = json_string(out);
        free(out);
        return res;
    }
}

static const char *HRP_RUN_STATUS_KEYS[] = {
    "run_id", "status", "output", "error", "approval", "last_event", "tool",
    "progress_at", "execution_generation"};
static const char *HRP_APPROVAL_KEYS[] = {"command", "description", "choices", "edit", "timeout_s"};

static json_t *hrp_run_status(const json_t *full)
{
    json_t *status = json_object();
    size_t i;
    if (!full || full->type != JSON_OBJECT) return status;
    for (i = 0; i < sizeof(HRP_RUN_STATUS_KEYS) / sizeof(HRP_RUN_STATUS_KEYS[0]); i++) {
        json_t *v = json_obj_get(full, HRP_RUN_STATUS_KEYS[i]);
        if (v) json_set(status, HRP_RUN_STATUS_KEYS[i], json_copy(v));
    }
    if (hrp_present(full, "pending_controls")) {
        json_t *pc = json_obj_get(full, "pending_controls");
        json_t *prompt = NULL;
        if (pc && pc->type == JSON_ARRAY) {
            for (i = 0; i < json_len(pc); i++) {
                json_t *p = json_get(pc, i);
                const char *kind;
                if (!p || p->type != JSON_OBJECT) continue;
                kind = json_get_str(p, "kind", NULL);
                if (kind && !strcmp(kind, "approval")) {
                    prompt = p;
                    break;
                }
            }
        }
        if (!prompt) {
            json_set(status, "approval", json_null());
        } else {
            json_t *approval = json_object();
            json_set(approval, "request_id", hrp_copy_of(json_obj_get(prompt, "prompt_id")));
            for (i = 0; i < sizeof(HRP_APPROVAL_KEYS) / sizeof(HRP_APPROVAL_KEYS[0]); i++) {
                json_t *v = json_obj_get(prompt, HRP_APPROVAL_KEYS[i]);
                if (v) json_set(approval, HRP_APPROVAL_KEYS[i], json_copy(v));
            }
            json_set(status, "approval", approval);
        }
    }
    return status;
}

json_t *w31_hostedroom_50fa41_p1_run_status(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_run_status */
    return hrp_run_status(json_obj_get(in, "full"));
}

/* ====================================================================== */
/* poll backoff                                                            */
/* ====================================================================== */

static double hrp_next_delay(const json_t *cached, double mn, double mx)
{
    double prev = cached ? hrp_num(cached, "delay", 0.0) : mn / 2.0;
    double v = prev * 2.0;
    if (v < mn) v = mn;
    if (v > mx) v = mx;
    return v;
}

json_t *w31_hostedroom_50fa41_p1_next_poll_delay(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_next_poll_delay */
    json_t *cached = json_obj_get(in, "cached");
    if (cached && cached->type == JSON_NULL) cached = NULL;
    return json_number(hrp_next_delay(cached, hrp_num(in, "poll_min_seconds", 0.1),
                                      hrp_num(in, "poll_max_seconds", 2.0)));
}

/* ====================================================================== */
/* grant + digest authority checks                                         */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p1_require_room_grant(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_require_room_grant */
    char *value = hrp_or_empty_str(json_obj_get(in, "grant"));
    json_t *out;
    if (!value || !value[0] || !strcmp(value, "compat") || !strcmp(value, "compatibility-only")) {
        out = hrp_fail_msg("a scoped room grant is required");
    } else {
        out = hrp_ok(json_string(value));
    }
    free(value);
    return out;
}

json_t *w31_hostedroom_50fa41_p1_digest_check(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:digest_reauthorization_error */
    json_t *catalog = json_obj_get(in, "catalog");
    json_t *policy = catalog ? json_obj_get(catalog, "execution_policy") : NULL;
    const char *policy_digest = policy ? json_get_str(policy, "policy_digest", NULL) : NULL;
    const char *catalog_digest = catalog ? json_get_str(catalog, "catalog_digest", NULL) : NULL;
    json_t *expected_ep = json_obj_get(in, "execution_policy_digest");
    json_t *expected_cap = json_obj_get(in, "capability_digest");
    int ep_has = expected_ep && expected_ep->type != JSON_NULL;
    int cap_has = expected_cap && expected_cap->type != JSON_NULL;
    if (ep_has) {
        const char *want = (expected_ep->type == JSON_STRING) ? expected_ep->str_val : NULL;
        int same = (want && policy_digest) ? !strcmp(want, policy_digest)
                                           : (!want && !policy_digest);
        if (!same)
            return hrp_error_obj("peer room execution policy needs reauthorization", 0, 0, 1, 1,
                                 403.0, "room_execution_policy_changed");
    }
    if (cap_has) {
        const char *want = (expected_cap->type == JSON_STRING) ? expected_cap->str_val : NULL;
        int same = (want && catalog_digest) ? !strcmp(want, catalog_digest)
                                            : (!want && !catalog_digest);
        if (!same)
            return hrp_error_obj("peer room capabilities need reauthorization", 0, 0, 1, 1, 403.0,
                                 "room_capability_catalog_changed");
    }
    return json_null();
}

/* ====================================================================== */
/* HTTP error classification                                               */
/* ====================================================================== */

static const char *hrp_reauth_message(const char *code)
{
    if (!code) return NULL;
    if (!strcmp(code, "invalid_room_grant") || !strcmp(code, "room_reauthorization_required"))
        return "peer room authorization needs renewal";
    if (!strcmp(code, "room_execution_policy_changed"))
        return "peer room execution policy needs reauthorization";
    if (!strcmp(code, "room_capability_catalog_changed"))
        return "peer room capabilities need reauthorization";
    return NULL;
}

static json_t *hrp_http_error(const char *method, const char *path, double code,
                              const char *detail)
{
    int admission = (!strcmp(method, "POST") && !strcmp(path, "/v1/runs")) ? 1 : 0;
    char *cut = hrp_cp_prefix(detail ? detail : "", 500);
    char *ec = hrp_response_error_code(cut);
    int conflict = admission && code == 409.0 && (!ec || !strcmp(ec, "idempotency_key_conflict"));
    int retryable = (code == 408.0 || code == 425.0 || code == 429.0 || code >= 500.0) ? 1 : 0;
    int ambiguous = (!strcmp(method, "POST") && (code >= 500.0 || conflict)) ? 1 : 0;
    int not_admitted = (admission && code >= 400.0 && code < 500.0 && !conflict) ? 1 : 0;
    int renewal = ((code == 401.0 || code == 403.0) && ec &&
                   (!strcmp(ec, "invalid_room_grant") ||
                    !strcmp(ec, "room_reauthorization_required")))
                      ? 1
                      : 0;
    int drift = (code == 403.0 && ec &&
                 (!strcmp(ec, "room_execution_policy_changed") ||
                  !strcmp(ec, "room_capability_catalog_changed")))
                    ? 1
                    : 0;
    const char *mapped = (renewal || drift) ? hrp_reauth_message(ec) : NULL;
    json_t *err;
    if (!mapped) {
        char msg[512];
        snprintf(msg, sizeof msg, "peer rejected %s %s with HTTP %d", method, path, (int)code);
        err = hrp_error_obj(msg, retryable, ambiguous, not_admitted, 1, code, ec);
    } else {
        err = hrp_error_obj(mapped, retryable, ambiguous, not_admitted, 1, code, ec);
    }
    free(cut);
    free(ec);
    return err;
}

json_t *w31_hostedroom_50fa41_p1_http_error(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._raise_http_error */
    const char *method = json_get_str(in, "method", "GET");
    const char *path = json_get_str(in, "path", "");
    json_t *detail = json_obj_get(in, "detail");
    const char *text = (detail && detail->type == JSON_STRING) ? detail->str_val : "";
    return hrp_http_error(method, path, hrp_num(in, "status", 0.0), text);
}

/* ====================================================================== */
/* pre-admission failure predicate                                         */
/* ====================================================================== */

/* Membership is decided by errno NAME, exactly like upstream's
 * `getattr(errno, name)` filter -- the numeric value is platform-local. */
static int hrp_not_admitted_errno(const char *name)
{
    if (!name) return 0;
    return !strcmp(name, "ECONNREFUSED") || !strcmp(name, "ENETDOWN") ||
           !strcmp(name, "ENETUNREACH") || !strcmp(name, "EHOSTDOWN") ||
           !strcmp(name, "EHOSTUNREACH");
}

static json_t *hrp_pre_admission(int ambiguous, int gaierror, int oserror,
                                 const char *errno_name)
{
    int proven = gaierror || (oserror && hrp_not_admitted_errno(errno_name));
    int not_admitted = ambiguous && proven;
    return hrp_error_obj("peer RoomLink endpoint is unreachable", 1,
                         ambiguous && !not_admitted, not_admitted, 0, 0.0, NULL);
}

json_t *w31_hostedroom_50fa41_p1_pre_admission(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_is_proven_pre_admission_failure */
    return hrp_pre_admission(json_get_bool(in, "ambiguous", false),
                             json_get_bool(in, "gaierror", false),
                             json_get_bool(in, "oserror", false),
                             json_get_str(in, "errno_name", NULL));
}

/* ====================================================================== */
/* budget overrun mapping (_read_body)                                     */
/* ====================================================================== */

static json_t *hrp_budget_error(const char *kind, int time_budget, int ambiguous)
{
    char msg[256];
    snprintf(msg, sizeof msg, "peer%s response exceeded the RoomLink %s", kind ? kind : "",
             time_budget ? "time budget" : "size limit");
    return hrp_error_obj(msg, time_budget, ambiguous, 0, 0, 0.0, NULL);
}

json_t *w31_hostedroom_50fa41_p1_budget_error(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:_read_body */
    const char *kind = json_get_str(in, "kind", "");
    int time_budget = !strcmp(json_get_str(in, "budget", "size"), "time");
    return hrp_fail(hrp_budget_error(kind, time_budget, json_get_bool(in, "ambiguous", false)));
}

/* ====================================================================== */
/* _request: URL + headers + body build, bounded read, JSON parse          */
/* ====================================================================== */

static json_t *hrp_request(const json_t *in)
{
    const char *base = json_get_str(in, "base_url", "");
    const char *path = json_get_str(in, "path", "");
    const char *method = json_get_str(in, "method", "GET");
    const char *api_key = json_get_str(in, "api_key", "");
    json_t *grant = json_obj_get(in, "room_grant");
    json_t *body = json_obj_get(in, "body");
    json_t *extra = json_obj_get(in, "headers");
    json_t *resp = json_obj_get(in, "response");
    json_t *echo = json_object();
    json_t *out = json_object();
    char *url, *auth, *data = NULL;
    json_t *hdrs = json_object();
    size_t i;
    int ambiguous = !strcmp(method, "POST");

    url = (char *)malloc(strlen(base) + strlen(path) + 1);
    sprintf(url, "%s%s", base, path);
    if (body && body->type != JSON_NULL) data = json_serialize(body);
    if (hrp_truthy(grant)) {
        char *g = hrp_pystr(grant);
        auth = (char *)malloc(strlen(g) + 12);
        sprintf(auth, "HermesRoom %s", g);
        free(g);
    } else {
        auth = (char *)malloc(strlen(api_key) + 8);
        sprintf(auth, "Bearer %s", api_key);
    }
    json_set(hdrs, "Authorization", json_string(auth));
    json_set(hdrs, "Content-Type", json_string("application/json"));
    json_set(hdrs, "User-Agent", json_string("Hermes-RoomLink/1.0"));
    if (extra && extra->type == JSON_OBJECT) {
        for (i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            if (k) json_set(hdrs, k, json_copy(json_object_get_at(extra, i)));
        }
    }
    json_set(echo, "url", json_string(url));
    json_set(echo, "method", json_string(method));
    json_set(echo, "authorization", hrp_copy_of(json_obj_get(hdrs, "Authorization")));
    json_set(echo, "content_type", hrp_copy_of(json_obj_get(hdrs, "Content-Type")));
    json_set(echo, "user_agent", hrp_copy_of(json_obj_get(hdrs, "User-Agent")));
    json_set(echo, "headers", json_copy(hdrs));
    json_set(echo, "data", data ? json_string(data) : json_null());
    json_set(echo, "ambiguous", json_bool(ambiguous ? true : false));
    json_set(out, "request", echo);

    if (resp && resp->type == JSON_OBJECT &&
        !strcmp(json_get_str(resp, "kind", "ok"), "http")) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error",
                 hrp_http_error(method, path, hrp_num(resp, "status", 0.0),
                                json_get_str(resp, "detail", "")));
    } else if (resp && resp->type == JSON_OBJECT &&
               !strcmp(json_get_str(resp, "kind", "ok"), "unreachable")) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "error", hrp_pre_admission(ambiguous, json_get_bool(resp, "gaierror", false),
                                                 json_get_bool(resp, "oserror", false),
                                                 json_get_str(resp, "errno_name", NULL)));
    } else {
        json_t *declared = resp ? json_obj_get(resp, "declared") : NULL;
        const char *raw = resp ? json_get_str(resp, "raw", "") : "";
        if (declared && declared->type == JSON_NUMBER &&
            declared->num_val > (double)W31_HRP_MAX_PEER_RESPONSE_BYTES) {
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", hrp_budget_error("", 0, ambiguous));
        } else if (strlen(raw) > (size_t)W31_HRP_MAX_PEER_RESPONSE_BYTES) {
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", hrp_budget_error("", 0, ambiguous));
        } else {
            json_t *payload = json_parse(raw, NULL);
            if (!payload) {
                json_set(out, "ok", json_bool(false));
                json_set(out, "error",
                         hrp_error_obj("peer returned non-JSON data", ambiguous, ambiguous, 0, 0,
                                       0.0, NULL));
            } else if (payload->type != JSON_OBJECT) {
                json_set(out, "ok", json_bool(false));
                json_set(out, "error",
                         hrp_error_obj("peer returned a non-object response", ambiguous, ambiguous,
                                       0, 0, 0.0, NULL));
                json_free(payload);
            } else {
                json_set(out, "ok", json_bool(true));
                json_set(out, "value", payload);
            }
        }
    }
    free(url);
    free(auth);
    free(data);
    json_free(hdrs);
    return out;
}

json_t *w31_hostedroom_50fa41_p1_request(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._request */
    return hrp_request(in);
}

/* ====================================================================== */
/* dispatch projection + admission body                                    */
/* ====================================================================== */

static const char *HRP_DISPATCH_FIELDS[] = {
    "protocol_version", "room_id", "home_install_id", "authority_gateway_id",
    "authority_epoch", "member_id", "target_install_id", "target_profile", "task_id",
    "execution_generation", "source_event_seq", "cancellation_scope_id", "prompt",
    "prompt_digest", "capability_digest", "execution_policy_digest", "trace_id"};
#define HRP_DISPATCH_N ((int)(sizeof(HRP_DISPATCH_FIELDS) / sizeof(HRP_DISPATCH_FIELDS[0])))

static json_t *hrp_dispatch_mapping(const json_t *d)
{
    json_t *o = json_object();
    int i;
    for (i = 0; i < HRP_DISPATCH_N; i++)
        json_set(o, HRP_DISPATCH_FIELDS[i], hrp_copy_of(json_obj_get(d, HRP_DISPATCH_FIELDS[i])));
    return o;
}

json_t *w31_hostedroom_50fa41_p1_admission_body(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._admit_dispatch */
    json_t *d = json_obj_get(in, "dispatch");
    json_t *body = json_object();
    json_t *out = json_object();
    char *task_id = hrp_pystr(json_obj_get(d, "task_id"));
    char *gen = hrp_pystr(json_obj_get(d, "execution_generation"));
    char *key = (char *)malloc(strlen(task_id) + strlen(gen) + 8);
    json_set(body, "input", hrp_copy_of(json_obj_get(d, "prompt")));
    json_set(body, "hosted_room_dispatch", hrp_dispatch_mapping(d));
    sprintf(key, "room:%s:%s", task_id, gen);
    json_set(out, "body", body);
    json_set(out, "idempotency_key", json_string(key));
    free(task_id);
    free(gen);
    free(key);
    return out;
}

/* ====================================================================== */
/* invitation body                                                         */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p1_invitation_body(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.issue_invitation */
    json_t *body;
    if (!hrp_truthy(json_obj_get(in, "api_key")))
        return hrp_fail_msg("issuing an invitation requires the target gateway API key");
    body = json_object();
    json_set(body, "room_id", hrp_copy_of(json_obj_get(in, "room_id")));
    json_set(body, "home_install_id", hrp_copy_of(json_obj_get(in, "home_install_id")));
    json_set(body, "authority_gateway_id", hrp_copy_of(json_obj_get(in, "authority_gateway_id")));
    json_set(body, "authority_epoch", hrp_copy_of(json_obj_get(in, "authority_epoch")));
    json_set(body, "member_id", hrp_copy_of(json_obj_get(in, "member_id")));
    json_set(body, "grant_id", hrp_copy_of(json_obj_get(in, "grant_id")));
    {
        json_t *ttl = json_obj_get(in, "ttl_seconds");
        json_set(body, "ttl_seconds", ttl ? json_copy(ttl) : json_number(3600));
    }
    if (hrp_present(in, "status_ttl_seconds") &&
        json_obj_get(in, "status_ttl_seconds")->type != JSON_NULL)
        json_set(body, "status_ttl_seconds", json_copy(json_obj_get(in, "status_ttl_seconds")));
    return hrp_ok(body);
}

/* ====================================================================== */
/* SHA-256 (prepare's logical session identity)                            */
/* ====================================================================== */

static uint32_t hrp_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void hrp_sha256(const unsigned char *data, size_t len, unsigned char out[32])
{
    static const uint32_t K[64] = {
        0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
        0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
        0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
        0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
        0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
        0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
        0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
        0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
        0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
        0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
        0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};
    uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                     0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    uint64_t bits = (uint64_t)len * 8u;
    size_t padded = ((len + 8) / 64 + 1) * 64, off;
    unsigned char *buf = (unsigned char *)calloc(1, padded);
    uint32_t w[64];
    int i;
    if (!buf) return;
    memcpy(buf, data, len);
    buf[len] = 0x80;
    for (i = 0; i < 8; i++) buf[padded - 1 - (size_t)i] = (unsigned char)(bits >> (8 * i));
    for (off = 0; off < padded; off += 64) {
        uint32_t a, b, c, d, e, f, g, hh;
        for (i = 0; i < 16; i++)
            w[i] = ((uint32_t)buf[off + 4 * i] << 24) |
                   ((uint32_t)buf[off + 4 * i + 1] << 16) |
                   ((uint32_t)buf[off + 4 * i + 2] << 8) | (uint32_t)buf[off + 4 * i + 3];
        for (i = 16; i < 64; i++) {
            uint32_t s0 = hrp_rotr(w[i - 15], 7) ^ hrp_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = hrp_rotr(w[i - 2], 17) ^ hrp_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        a = h[0]; b = h[1]; c = h[2]; d = h[3];
        e = h[4]; f = h[5]; g = h[6]; hh = h[7];
        for (i = 0; i < 64; i++) {
            uint32_t S1 = hrp_rotr(e, 6) ^ hrp_rotr(e, 11) ^ hrp_rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = hrp_rotr(a, 2) ^ hrp_rotr(a, 13) ^ hrp_rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(buf);
    for (i = 0; i < 8; i++) {
        out[4 * i] = (unsigned char)(h[i] >> 24);
        out[4 * i + 1] = (unsigned char)(h[i] >> 16);
        out[4 * i + 2] = (unsigned char)(h[i] >> 8);
        out[4 * i + 3] = (unsigned char)h[i];
    }
}

static char *hrp_sha256_hex(const unsigned char *data, size_t len)
{
    unsigned char digest[32];
    char *hex = (char *)malloc(65);
    int i;
    if (!hex) return NULL;
    hrp_sha256(data, len, digest);
    for (i = 0; i < 32; i++) snprintf(hex + 2 * i, 3, "%02x", digest[i]);
    hex[64] = '\0';
    return hex;
}

/* ====================================================================== */
/* p2: constructor value guards                                            */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_api_checks(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.__init__ */
    json_t *key = json_obj_get(in, "api_key");
    const char *error = NULL;
    json_t *o;
    if (hrp_truthy(key) && key->type == JSON_STRING && hrp_cp_len(key->str_val) < 16)
        error = "peer API key is missing or too short";
    else if (hrp_num(in, "poll_min_seconds", 0.1) <= 0.0 ||
             hrp_num(in, "poll_max_seconds", 2.0) < hrp_num(in, "poll_min_seconds", 0.1))
        error = "peer polling bounds are invalid";
    o = json_object();
    json_set(o, "error", error ? json_string(error) : json_null());
    return o;
}

/* ====================================================================== */
/* p2: prepare                                                             */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_prepare(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.prepare */
    const char *source = json_get_str(in, "source", "");
    char *grant, *room_id, *profile, *material, *hex, *session;
    json_t *expected, *value;

    if (strcmp(source, "bot_room") != 0) return hrp_fail_msg("peer room source must be bot_room");
    grant = hrp_or_empty_str(json_obj_get(in, "grant"));
    if (!grant[0] || !strcmp(grant, "compat") || !strcmp(grant, "compatibility-only")) {
        free(grant);
        return hrp_fail_msg("a scoped room grant is required");
    }
    free(grant);

    room_id = hrp_pystr(json_obj_get(in, "room_id"));
    profile = hrp_pystr(json_obj_get(in, "profile"));
    material = (char *)malloc(strlen(room_id) + strlen(profile) + 2);
    memcpy(material, room_id, strlen(room_id));
    material[strlen(room_id)] = '\0';
    memcpy(material + strlen(room_id) + 1, profile, strlen(profile) + 1);
    hex = hrp_sha256_hex((const unsigned char *)material, strlen(room_id) + 1 + strlen(profile));
    session = (char *)malloc(64);
    snprintf(session, 64, "roomlink_%.32s", hex ? hex : "");

    expected = json_obj_get(in, "expected_session_id");
    {
        char *want = hrp_pystr(expected);
        int mismatch = hrp_truthy(expected) && strcmp(want, session) != 0;
        free(want);
        if (mismatch) {
            free(room_id);
            free(profile);
            free(material);
            free(hex);
            free(session);
            return hrp_fail_msg("peer room session identity changed");
        }
    }
    value = json_object();
    json_set(value, "session_id", json_string(session));
    {
        char *title = (char *)malloc(strlen(room_id) + 8);
        sprintf(title, "Group: %s", room_id);
        json_set(value, "title", json_string(title));
        free(title);
    }
    json_set(value, "source", json_string(source));
    free(room_id);
    free(profile);
    free(material);
    free(hex);
    free(session);
    return hrp_ok(value);
}

/* ====================================================================== */
/* p2: room scope                                                          */
/* ====================================================================== */

static const char *HRP_SCOPE_FIELDS[] = {
    "room_id", "home_install_id", "authority_gateway_id", "authority_epoch",
    "member_id", "target_install_id", "target_profile"};

json_t *w31_hostedroom_50fa41_p2_room_scope(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.bind_room_scope */
    json_t *scope = json_obj_get(in, "scope");
    char *names[6];
    double epoch = hrp_int_or0(json_obj_get(scope, "authority_epoch"));
    json_t *value;
    int i, complete = 1;

    names[0] = hrp_or_empty_str(json_obj_get(scope, "room_id"));
    names[1] = hrp_or_empty_str(json_obj_get(scope, "home_install_id"));
    names[2] = hrp_or_empty_str(json_obj_get(scope, "authority_gateway_id"));
    names[3] = hrp_or_empty_str(json_obj_get(scope, "member_id"));
    names[4] = hrp_or_empty_str(json_obj_get(scope, "target_install_id"));
    names[5] = hrp_or_empty_str(json_obj_get(scope, "target_profile"));
    for (i = 0; i < 6; i++)
        if (!names[i] || !names[i][0]) complete = 0;
    if (!complete) {
        for (i = 0; i < 6; i++) free(names[i]);
        return hrp_fail_msg("peer room receipt scope is incomplete");
    }
    if (epoch < 1.0) {
        for (i = 0; i < 6; i++) free(names[i]);
        return hrp_fail_msg("peer room receipt authority epoch is invalid");
    }
    value = json_object();
    json_set(value, "room_id", json_string(names[0]));
    json_set(value, "home_install_id", json_string(names[1]));
    json_set(value, "authority_gateway_id", json_string(names[2]));
    json_set(value, "authority_epoch", json_number(epoch));
    json_set(value, "member_id", json_string(names[3]));
    json_set(value, "target_install_id", json_string(names[4]));
    json_set(value, "target_profile", json_string(names[5]));
    for (i = 0; i < 6; i++) free(names[i]);
    return hrp_ok(value);
}

/* ====================================================================== */
/* p2: accepted + receipt record                                           */
/* ====================================================================== */

static json_t *hrp_accepted(const json_t *d, const char *run_id, const char *session_id,
                            int replayed)
{
    json_t *o = json_object();
    json_set(o, "status", json_string("accepted"));
    json_set(o, "task_id", hrp_copy_of(json_obj_get(d, "task_id")));
    json_set(o, "execution_generation", hrp_copy_of(json_obj_get(d, "execution_generation")));
    json_set(o, "run_id", json_string(run_id ? run_id : ""));
    json_set(o, "session_id", json_string(session_id ? session_id : ""));
    json_set(o, "replayed", json_bool(replayed ? true : false));
    return o;
}

json_t *w31_hostedroom_50fa41_p2_accepted(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._accepted */
    return hrp_accepted(json_obj_get(in, "dispatch"), json_get_str(in, "run_id", ""),
                        json_get_str(in, "session_id", ""),
                        json_get_bool(in, "replayed", false));
}

json_t *w31_hostedroom_50fa41_p2_receipt_record(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._save_receipt */
    json_t *d = json_obj_get(in, "dispatch");
    json_t *o = json_object();
    int i;
    json_set(o, "run_id", hrp_copy_of(json_obj_get(in, "run_id")));
    json_set(o, "session_id", hrp_copy_of(json_obj_get(in, "session_id")));
    for (i = 0; i < 7; i++)
        json_set(o, HRP_SCOPE_FIELDS[i], hrp_copy_of(json_obj_get(d, HRP_SCOPE_FIELDS[i])));
    json_set(o, "task_id", hrp_copy_of(json_obj_get(d, "task_id")));
    json_set(o, "execution_generation", hrp_copy_of(json_obj_get(d, "execution_generation")));
    return o;
}

/* ====================================================================== */
/* p2: session id resolution                                               */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_session_id_choice(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._session_id */
    json_t *existing = json_obj_get(in, "existing");
    json_t *prepared = json_obj_get(in, "prepared");
    char *sid;
    json_t *out;
    if (existing && existing->type == JSON_OBJECT && hrp_present(existing, "session_id")) {
        sid = hrp_pystr(json_obj_get(existing, "session_id"));
        out = hrp_ok(json_string(sid));
        free(sid);
        return out;
    }
    if (!prepared || prepared->type != JSON_OBJECT)
        return hrp_fail_msg("peer room session is unavailable");
    {
        json_t *first = json_obj_get(prepared, "session_id");
        json_t *second = json_obj_get(prepared, "id");
        sid = hrp_truthy(first) ? hrp_pystr(first)
                                : (hrp_truthy(second) ? hrp_pystr(second) : hrp_dup(""));
    }
    out = hrp_ok(json_string(sid));
    free(sid);
    return out;
}

/* ====================================================================== */
/* p2: close_admission outcome                                             */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_close_result(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.close_admission */
    json_t *d = json_obj_get(in, "dispatch");
    json_t *existing = json_obj_get(in, "existing");
    json_t *result = json_obj_get(in, "result");
    const char *session = json_get_str(in, "session", "");
    if (existing && existing->type == JSON_OBJECT) {
        char *run_id = hrp_pystr(json_obj_get(existing, "run_id"));
        char *sid = hrp_pystr(json_obj_get(existing, "session_id"));
        json_t *value = hrp_accepted(d, run_id, sid, 1);
        free(run_id);
        free(sid);
        return hrp_ok(value);
    }
    if (result && result->type == JSON_OBJECT) {
        json_t *closed = json_obj_get(result, "closed");
        if (closed && closed->type == JSON_BOOL && closed->bool_val) {
            json_t *v = json_object();
            json_set(v, "closed", json_bool(true));
            return hrp_ok(v);
        }
    }
    {
        char *run_id = hrp_or_empty_str(json_obj_get(result, "run_id"));
        json_t *value;
        if (!run_id[0]) {
            free(run_id);
            return hrp_fail_msg("peer answered a run close with no run");
        }
        value = hrp_accepted(d, run_id, session, 1);
        free(run_id);
        return hrp_ok(value);
    }
}

/* ====================================================================== */
/* p2: recover_dispatch decision                                           */
/* ====================================================================== */

static int hrp_scope_conflicts(const json_t *receipt, const json_t *d)
{
    int i;
    for (i = 0; i < 7; i++) {
        json_t *want = json_obj_get(d, HRP_SCOPE_FIELDS[i]);
        json_t *have = json_obj_get(receipt, HRP_SCOPE_FIELDS[i]);
        char *a = hrp_pystr(have), *b = hrp_pystr(want);
        int same = (strcmp(a, b) == 0);
        free(a);
        free(b);
        if (!same) return 1;
    }
    return 0;
}

json_t *w31_hostedroom_50fa41_p2_recover_decision(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.recover_dispatch */
    json_t *d = json_obj_get(in, "dispatch");
    json_t *existing = json_obj_get(in, "existing");
    json_t *backoff = json_obj_get(in, "backoff");
    json_t *admit = json_obj_get(in, "admit");
    double now = hrp_num(in, "now", 0.0);
    double mn = hrp_num(in, "poll_min_seconds", 0.1);
    double mx = hrp_num(in, "poll_max_seconds", 2.0);
    json_t *out = json_object();
    json_set(out, "accepted", json_null());
    json_set(out, "backoff", json_null());
    json_set(out, "error", json_null());
    json_set(out, "replayed", json_null());

    if (existing && existing->type == JSON_OBJECT) {
        if (hrp_scope_conflicts(existing, d)) {
            json_set(out, "action", json_string("conflict"));
            json_set(out, "error",
                     hrp_error_obj("peer run receipt conflicts with the recovered dispatch", 0, 0, 0,
                                   0, 0.0, NULL));
            return out;
        }
        {
            char *run_id = hrp_pystr(json_obj_get(existing, "run_id"));
            char *sid = hrp_pystr(json_obj_get(existing, "session_id"));
            json_set(out, "action", json_string("replayed"));
            json_set(out, "accepted", hrp_accepted(d, run_id, sid, 1));
            free(run_id);
            free(sid);
        }
        return out;
    }
    if (backoff && backoff->type == JSON_OBJECT &&
        now < hrp_num(backoff, "next_attempt_at", 0.0)) {
        json_set(out, "action", json_string("backing_off"));
        json_set(out, "error",
                 hrp_error_obj("peer admission recovery is backing off", 1, 1, 0, 0, 0.0, NULL));
        return out;
    }
    if (admit && admit->type == JSON_OBJECT && json_get_bool(admit, "ok", false)) {
        json_set(out, "action", json_string("admitted"));
        json_set(out, "replayed", json_bool(json_get_bool(admit, "replayed", false)));
        return out;
    }
    {
        json_t *spec = (admit && admit->type == JSON_OBJECT) ? json_obj_get(admit, "error") : NULL;
        json_t *err = (spec && spec->type == JSON_OBJECT)
                          ? hrp_error_from_spec(spec)
                          : hrp_error_obj("", 0, 0, 0, 0, 0.0, NULL);
        json_set(out, "action", json_string("failed"));
        json_set(out, "error", err);
        if (json_get_bool(err, "retryable", false) || json_get_bool(err, "ambiguous", false)) {
            json_t *nb = json_object();
            double delay = hrp_next_delay(backoff && backoff->type == JSON_OBJECT ? backoff : NULL,
                                          mn, mx);
            json_set(nb, "delay", json_number(delay));
            json_set(nb, "next_attempt_at", json_number(now + delay));
            json_set(out, "backoff", nb);
        }
        return out;
    }
}

/* ====================================================================== */
/* p2: _poll_receipt decision + cache                                      */
/* ====================================================================== */

static int hrp_terminal_state(const char *s)
{
    return s && (!strcmp(s, "completed") || !strcmp(s, "failed") ||
                 !strcmp(s, "interrupted") || !strcmp(s, "cancelled"));
}

static int hrp_known_state(const char *s)
{
    return hrp_terminal_state(s) || (s && (!strcmp(s, "queued") || !strcmp(s, "running") ||
                                           !strcmp(s, "waiting_for_approval") ||
                                           !strcmp(s, "stopping")));
}

static int hrp_active_state(const char *s)
{
    return s && (!strcmp(s, "queued") || !strcmp(s, "running") ||
                 !strcmp(s, "waiting_for_approval") || !strcmp(s, "stopping"));
}

/* cache slot: {"status": <prev>, ["error": <err>], delay, next_poll_at, approvals}
 * A missing previous status is Python's empty dict, not null. */
static json_t *hrp_cache_slot(const json_t *status, const json_t *err, const json_t *entry)
{
    json_t *slot = json_object();
    json_set(slot, "status", status ? json_copy(status) : json_object());
    if (err) json_set(slot, "error", json_copy(err));
    json_set(slot, "delay", json_copy(json_obj_get(entry, "delay")));
    json_set(slot, "next_poll_at", json_copy(json_obj_get(entry, "next_poll_at")));
    json_set(slot, "approvals", json_copy(json_obj_get(entry, "approvals")));
    return slot;
}

json_t *w31_hostedroom_50fa41_p2_poll_cache(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._poll_receipt */
    json_t *cached = json_obj_get(in, "cached");
    json_t *record = json_obj_get(in, "record");
    json_t *fetched = json_obj_get(in, "fetched");
    double now = hrp_num(in, "now", 0.0);
    double mn = hrp_num(in, "poll_min_seconds", 0.1);
    double mx = hrp_num(in, "poll_max_seconds", 2.0);
    char *run_id = hrp_pystr(json_obj_get(record, "run_id"));
    json_t *out = json_object();
    double delay;
    json_t *entry, *approvals = json_object();
    int resolved = 0;

    json_set(out, "status", json_null());
    json_set(out, "cache", json_null());
    json_set(out, "terminal_key", json_null());
    json_set(out, "error", json_null());
    json_set(out, "resolved", json_bool(false));

    if (cached && cached->type == JSON_NULL) cached = NULL;
    if (cached && cached->type == JSON_OBJECT) {
        json_t *prev = json_obj_get(cached, "status");
        const char *state = (prev && prev->type == JSON_OBJECT)
                                ? json_get_str(prev, "status", NULL)
                                : NULL;
        if (hrp_terminal_state(state)) {
            json_set(out, "action", json_string("cached_terminal"));
            json_set(out, "status", hrp_copy_of(prev));
            free(run_id);
            return out;
        }
        if (now < hrp_num(cached, "next_poll_at", 0.0)) {
            json_t *err = json_obj_get(cached, "error");
            if (err && err->type == JSON_OBJECT) {
                json_set(out, "action", json_string("cached_raise"));
                json_set(out, "error", hrp_error_from_spec(err));
            } else {
                json_set(out, "action", json_string("cached_wait"));
                json_set(out, "status", hrp_copy_of(prev));
            }
            free(run_id);
            return out;
        }
        {
            json_t *ap = json_obj_get(cached, "approvals");
            if (ap && ap->type == JSON_OBJECT && hrp_truthy(ap)) {
                json_free(approvals);
                approvals = json_copy(ap);
            }
        }
    }
    delay = hrp_next_delay(cached, mn, mx);
    entry = json_object();
    json_set(entry, "delay", json_number(delay));
    json_set(entry, "next_poll_at", json_number(now + delay));
    json_set(entry, "approvals", approvals);

    if (!(fetched && fetched->type == JSON_OBJECT && json_get_bool(fetched, "ok", false))) {
        json_t *spec = (fetched && fetched->type == JSON_OBJECT)
                           ? json_obj_get(fetched, "error")
                           : NULL;
        json_t *err = (spec && spec->type == JSON_OBJECT)
                          ? hrp_error_from_spec(spec)
                          : hrp_error_obj("", 0, 0, 0, 0, 0.0, NULL);
        json_t *prev = cached ? json_obj_get(cached, "status") : NULL;
        json_t *cache = json_object();
        json_set(cache, run_id, hrp_cache_slot(prev, err, entry));
        json_set(out, "action", json_string("failed"));
        json_set(out, "error", err);
        json_set(out, "cache", cache);
        json_free(entry);
        free(run_id);
        return out;
    }
    {
        json_t *full = json_copy(json_obj_get(fetched, "full"));
        json_t *status;
        if (!full || full->type != JSON_OBJECT) {
            if (full) json_free(full);
            full = json_object();
        }
        if (!strcmp(json_get_str(full, "status", ""), "unknown")) {
            char *fid = hrp_or_empty_str(json_obj_get(full, "run_id"));
            if (!strcmp(fid, run_id)) {
                json_t *frozen = json_object();
                json_set(frozen, "run_id", hrp_copy_of(json_obj_get(full, "run_id")));
                json_set(frozen, "status", json_string("interrupted"));
                json_set(frozen, "error", json_string("member gateway restarted"));
                json_free(full);
                full = frozen;
                resolved = 1;
            }
            free(fid);
        }
        status = hrp_run_status(full);
        json_free(full);
        {
            char *sid = hrp_or_empty_str(json_obj_get(status, "run_id"));
            const char *state = json_get_str(status, "status", NULL);
            if (strcmp(sid, run_id) != 0 || !hrp_known_state(state)) {
                json_t *err = hrp_error_obj("peer returned a mismatched run status", 0, 0, 0, 0,
                                            0.0, NULL);
                json_t *prev = cached ? json_obj_get(cached, "status") : NULL;
                json_t *cache = json_object();
                json_set(cache, run_id, hrp_cache_slot(prev, err, entry));
                json_set(out, "action", json_string("failed"));
                json_set(out, "error", err);
                json_set(out, "cache", cache);
                json_free(entry);
                json_free(status);
                free(sid);
                free(run_id);
                return out;
            }
            free(sid);
        }
        {
            json_t *approval = json_obj_get(status, "approval");
            if (approval && approval->type == JSON_OBJECT) {
                const char *rid = json_get_str(approval, "request_id", NULL);
                json_t *aps = json_obj_get(entry, "approvals");
                if (rid && aps && aps->type == JSON_OBJECT && !hrp_present(aps, rid)) {
                    json_t *ident = json_object();
                    json_set(ident, "request_id", json_string(rid));
                    if (hrp_present(status, "execution_generation"))
                        json_set(ident, "execution_generation",
                                 hrp_copy_of(json_obj_get(status, "execution_generation")));
                    json_set(aps, rid, ident);
                }
            }
        }
        {
            json_t *cache = json_object();
            json_set(cache, run_id, hrp_cache_slot(status, NULL, entry));
            json_set(out, "cache", cache);
            json_set(out, "status", json_copy(status));
            if (hrp_terminal_state(json_get_str(status, "status", NULL))) {
                json_t *key = json_array();
                json_append(key, hrp_copy_of(json_obj_get(record, "task_id")));
                json_append(key,
                            json_number(hrp_int_or0(json_obj_get(record, "execution_generation"))));
                json_set(out, "terminal_key", key);
            }
        }
        json_set(out, "action", json_string("fetched"));
        json_set(out, "resolved", json_bool(resolved ? true : false));
        json_free(status);
        json_free(entry);
        free(run_id);
        return out;
    }
}

/* ====================================================================== */
/* p2: observation receipt                                                 */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_observation_receipt(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient._observation_receipt */
    json_t *explicit_ = json_obj_get(in, "explicit");
    json_t *record = json_obj_get(in, "record");
    json_t *query = json_obj_get(in, "query");
    int has_key = 0;

    if (explicit_ && explicit_->type == JSON_OBJECT) {
        json_t *tid = json_obj_get(explicit_, "task_id");
        json_t *gen = json_obj_get(explicit_, "execution_generation");
        int has_tid = tid && tid->type != JSON_NULL;
        int has_gen = gen && gen->type != JSON_NULL;
        if (has_tid || has_gen) {
            char *t = hrp_or_empty_str(tid);
            double g = hrp_int_or0(gen);
            int bad = (!t[0] || g < 1.0);
            free(t);
            if (bad) return hrp_fail_msg("peer observation identity is invalid");
            has_key = 1;
        }
    }
    if (!has_key) {
        json_t *key = json_obj_get(in, "key");
        if (!key || key->type == JSON_NULL) {
            json_t *o = json_object();
            json_set(o, "record", json_null());
            return o;
        }
    }
    if (!record || record->type != JSON_OBJECT) {
        json_t *o = json_object();
        json_set(o, "record", json_null());
        return o;
    }
    {
        const char *room = json_get_str(record, "room_id", NULL);
        const char *profile = json_get_str(record, "target_profile", NULL);
        const char *session = json_get_str(record, "session_id", NULL);
        int same = room && profile && session &&
                   !strcmp(room, json_get_str(query, "room_id", "")) &&
                   !strcmp(profile, json_get_str(query, "profile", "")) &&
                   !strcmp(session, json_get_str(query, "session_id", ""));
        if (!same) return hrp_fail_msg("peer observation receipt changed scope");
    }
    {
        json_t *o = json_object();
        json_set(o, "record", json_copy(record));
        return o;
    }
}

/* ====================================================================== */
/* p2: history + status projections                                        */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_history_view(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.history */
    json_t *receipt = json_obj_get(in, "receipt");
    json_t *status = json_obj_get(in, "status");
    json_t *out = json_array();
    const char *state;
    if (!receipt || receipt->type != JSON_OBJECT) return out;
    state = json_get_str(status, "status", "");
    if (strcmp(state, "completed") && strcmp(state, "failed") && strcmp(state, "interrupted"))
        return out;
    {
        json_t *msg = json_object();
        json_t *output = json_obj_get(status, "output");
        json_t *error = json_obj_get(status, "error");
        char *content = hrp_truthy(output) ? hrp_pystr(output)
                                           : (hrp_truthy(error) ? hrp_pystr(error) : hrp_dup(""));
        char *run_id = hrp_pystr(json_obj_get(status, "run_id"));
        char *mid = (char *)malloc(strlen(run_id) + 12);
        sprintf(mid, "peer-run:%s", run_id);
        json_set(msg, "role", json_string("assistant"));
        json_set(msg, "task_id", hrp_copy_of(json_obj_get(receipt, "task_id")));
        json_set(msg, "execution_generation",
                 hrp_copy_of(json_obj_get(receipt, "execution_generation")));
        json_set(msg, "status", json_string(!strcmp(state, "completed") ? "settled" : "failed"));
        json_set(msg, "message_id", json_string(mid));
        json_set(msg, "content", json_string(content));
        json_append(out, msg);
        free(content);
        free(run_id);
        free(mid);
    }
    return out;
}

json_t *w31_hostedroom_50fa41_p2_status_view(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.status */
    json_t *receipt = json_obj_get(in, "receipt");
    json_t *status = json_obj_get(in, "status");
    json_t *out;
    if (!receipt || receipt->type != JSON_OBJECT) {
        out = json_object();
        json_set(out, "active", json_bool(false));
        json_set(out, "task_id", json_null());
        return out;
    }
    out = json_object();
    json_set(out, "active",
             json_bool(hrp_active_state(json_get_str(status, "status", NULL)) ? true : false));
    json_set(out, "task_id", hrp_copy_of(json_obj_get(receipt, "task_id")));
    json_set(out, "execution_generation",
             hrp_copy_of(json_obj_get(receipt, "execution_generation")));
    json_set(out, "run_execution_generation",
             hrp_copy_of(json_obj_get(status, "execution_generation")));
    json_set(out, "status", hrp_copy_of(json_obj_get(status, "status")));
    json_set(out, "run_id", hrp_copy_of(json_obj_get(status, "run_id")));
    json_set(out, "approval", hrp_copy_of(json_obj_get(status, "approval")));
    if (hrp_present(status, "tool")) json_set(out, "tool", json_copy(json_obj_get(status, "tool")));
    if (hrp_present(status, "progress_at"))
        json_set(out, "progress_at", json_copy(json_obj_get(status, "progress_at")));
    return out;
}

/* ====================================================================== */
/* p2: approve_receipt decision                                            */
/* ====================================================================== */

static int hrp_stale_approval_code(const char *code)
{
    return code && (!strcmp(code, "stale_generation") || !strcmp(code, "approval_not_pending") ||
                    !strcmp(code, "approval_not_active"));
}

json_t *w31_hostedroom_50fa41_p2_approve_decision(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.approve_receipt */
    json_t *record = json_obj_get(in, "record");
    json_t *cached = json_obj_get(in, "cached");
    json_t *poll = json_obj_get(in, "status");
    json_t *action = json_obj_get(in, "action");
    json_t *out = json_object();
    char *raw_id, *request_id;
    json_t *identity = NULL;
    json_t *body = NULL;

    json_set(out, "body", json_null());
    json_set(out, "identity", json_null());
    json_set(out, "result", json_null());
    json_set(out, "error", json_null());

    if (!record || record->type != JSON_OBJECT) {
        json_set(out, "action", json_string("none"));
        return out;
    }
    raw_id = hrp_or_empty_str(json_obj_get(in, "request_id"));
    request_id = hrp_strip(raw_id);
    free(raw_id);
    if (!request_id[0]) {
        json_set(out, "action", json_string("raise"));
        json_set(out, "error",
                 hrp_error_obj("an exact approval request_id is required", 0, 0, 0, 0, 0.0, NULL));
        free(request_id);
        return out;
    }
    {
        json_t *approvals = cached ? json_obj_get(cached, "approvals") : NULL;
        if (approvals && approvals->type == JSON_OBJECT && hrp_present(approvals, request_id))
            identity = json_copy(json_obj_get(approvals, request_id));
    }
    if (!identity) {
        if (!poll || poll->type != JSON_OBJECT) {
            json_set(out, "action", json_string("raise"));
            json_set(out, "error",
                     hrp_error_obj("peer approval status unavailable", 0, 0, 0, 0, 0.0, NULL));
            free(request_id);
            return out;
        }
        if (hrp_present(poll, "execution_generation")) {
            json_t *approval = json_obj_get(poll, "approval");
            const char *pending = (approval && approval->type == JSON_OBJECT)
                                      ? json_get_str(approval, "request_id", NULL)
                                      : NULL;
            if (!pending || strcmp(pending, request_id) != 0) {
                json_set(out, "action", json_string("raise"));
                json_set(out, "error",
                         hrp_error_obj("room approval is no longer pending", 0, 0, 0, 0, 0.0,
                                       NULL));
                free(request_id);
                return out;
            }
            identity = json_object();
            json_set(identity, "request_id", json_string(request_id));
            json_set(identity, "execution_generation",
                     hrp_copy_of(json_obj_get(poll, "execution_generation")));
        } else {
            identity = json_object();
            json_set(identity, "request_id", json_string(request_id));
        }
    }
    body = json_object();
    {
        size_t i;
        json_set(body, "choice", hrp_copy_of(json_obj_get(in, "choice")));
        for (i = 0; i < json_object_size(identity); i++) {
            const char *k = json_object_get_key_at(identity, i);
            if (k) json_set(body, k, json_copy(json_object_get_at(identity, i)));
        }
    }
    if (action && action->type == JSON_OBJECT && json_get_bool(action, "ok", false)) {
        json_set(out, "action", json_string("post"));
        json_set(out, "identity", json_copy(identity));
        json_set(out, "body", body ? body : json_null());
        body = NULL;
        json_set(out, "result", hrp_copy_of(json_obj_get(action, "result")));
    } else {
        json_t *spec = (action && action->type == JSON_OBJECT) ? json_obj_get(action, "error")
                                                               : NULL;
        json_t *err = (spec && spec->type == JSON_OBJECT)
                          ? hrp_error_from_spec(spec)
                          : hrp_error_obj("", 0, 0, 0, 0, 0.0, NULL);
        json_set(out, "action", json_string("raise"));
        if (hrp_stale_approval_code(json_get_str(err, "error_code", NULL))) {
            json_t *st = json_obj_get(err, "status_code");
            int has = (st && st->type == JSON_NUMBER) ? 1 : 0;
            json_set(out, "error",
                     hrp_error_obj("room approval is no longer pending", 0, 0, 0, has,
                                   has ? st->num_val : 0.0,
                                   json_get_str(err, "error_code", NULL)));
            json_free(err);
        } else {
            json_set(out, "error", err);
        }
    }
    json_free(identity);
    if (body) json_free(body);
    free(request_id);
    return out;
}

/* ====================================================================== */
/* p2: forget / stop / refresh decisions                                   */
/* ====================================================================== */

json_t *w31_hostedroom_50fa41_p2_forget_decision(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.forget_receipt */
    json_t *record = json_obj_get(in, "record");
    char *want = hrp_pystr(json_obj_get(in, "run_id"));
    json_t *out = json_object();
    int dropped = 0;
    if (record && record->type == JSON_OBJECT) {
        char *have = hrp_or_empty_str(json_obj_get(record, "run_id"));
        dropped = !strcmp(have, want);
        free(have);
    }
    json_set(out, "dropped", json_bool(dropped ? true : false));
    json_set(out, "return",
             json_get_bool(in, "has_store", false) ? json_string("seam") : json_bool(true));
    free(want);
    return out;
}

json_t *w31_hostedroom_50fa41_p2_stop_decision(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.stop_receipt */
    json_t *record = json_obj_get(in, "record");
    json_t *result = json_obj_get(in, "result");
    json_t *err_spec = json_obj_get(in, "error");
    json_t *out = json_object();
    json_set(out, "result", json_null());
    json_set(out, "error", json_null());
    json_set(out, "terminal", json_bool(false));
    if (!record || record->type != JSON_OBJECT) {
        json_set(out, "action", json_string("none"));
        return out;
    }
    if (err_spec && err_spec->type == JSON_OBJECT) {
        json_t *err = hrp_error_from_spec(err_spec);
        if (strcmp(json_get_str(err, "error_code", ""), "unknown_execution") != 0) {
            json_set(out, "action", json_string("raise"));
            json_set(out, "error", err);
            return out;
        }
        json_free(err);
        json_set(out, "action", json_string("repoll"));
        return out;
    }
    json_set(out, "action", json_string("returned"));
    json_set(out, "result", hrp_copy_of(result));
    json_set(out, "terminal",
             json_bool(hrp_terminal_state(json_get_str(result, "status", NULL)) ? true : false));
    return out;
}

json_t *w31_hostedroom_50fa41_p2_refresh_decision(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.refresh_receipt */
    json_t *record = json_obj_get(in, "record");
    json_t *err_spec = json_obj_get(in, "error");
    json_t *out = json_object();
    json_set(out, "error", json_null());
    json_set(out, "run_id", json_null());
    if (!record || record->type != JSON_OBJECT) {
        json_set(out, "action", json_string("none"));
        return out;
    }
    if (!err_spec || err_spec->type != JSON_OBJECT) {
        json_set(out, "action", json_string("ok"));
        return out;
    }
    {
        json_t *err = hrp_error_from_spec(err_spec);
        json_t *st = json_obj_get(err, "status_code");
        int is404 = (st && st->type == JSON_NUMBER && st->num_val == 404.0);
        const char *code = json_get_str(err, "error_code", NULL);
        if (is404 && code && !strcmp(code, "run_not_found")) {
            json_set(out, "action", json_string("forget"));
            json_set(out, "run_id", hrp_copy_of(json_obj_get(record, "run_id")));
            json_free(err);
        } else {
            json_set(out, "action", json_string("raise"));
            json_set(out, "error", err);
        }
    }
    return out;
}

json_t *w31_hostedroom_50fa41_p2_refreshed_grant(const json_t *in)
{
    /* PoP: hosted_room_peer_http.py:PeerRunsHTTPClient.refresh_grant */
    char *replacement = hrp_or_empty_str(json_obj_get(json_obj_get(in, "refreshed"), "grant"));
    json_t *out;
    if (!replacement[0]) {
        out = hrp_fail_msg("peer returned no refreshed room grant");
    } else {
        out = hrp_ok(json_string(replacement));
    }
    free(replacement);
    return out;
}
