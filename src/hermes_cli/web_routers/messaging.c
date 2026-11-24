/*
 * messaging.c — C11 port of upstream hermes_cli/web_routers/messaging.py.
 *
 * Gates: w28_messaging_ede80b_p1 / _p2 / _p3 (one port file, three partitions).
 *
 * PoP (p1 — WhatsApp onboarding value shaping + pairing event machine):
 *   w28_messaging_ede80b_p1_normalize_mode
 *       @ hermes_cli/web_routers/messaging.py:_normalize_whatsapp_onboarding_mode
 *   w28_messaging_ede80b_p1_normalize_allowed_users
 *       @ hermes_cli/web_routers/messaging.py:_normalize_whatsapp_allowed_users
 *   w28_messaging_ede80b_p1_phone_from_identifier
 *       @ hermes_cli/web_routers/messaging.py:_whatsapp_phone_from_identifier
 *   w28_messaging_ede80b_p1_first_str
 *       @ hermes_cli/web_routers/messaging.py:_first_str
 *   w28_messaging_ede80b_p1_linked_account
 *       @ hermes_cli/web_routers/messaging.py:_whatsapp_linked_account_from_session
 *   w28_messaging_ede80b_p1_apply_pairing_event
 *       @ hermes_cli/web_routers/messaging.py:_apply_pairing_event
 *
 * PoP (p2 — Telegram onboarding):
 *   w28_messaging_ede80b_p2_normalize_user_id
 *       @ hermes_cli/web_routers/messaging.py:_normalize_telegram_user_id
 *   w28_messaging_ede80b_p2_normalize_allowed_ids
 *       @ hermes_cli/web_routers/messaging.py:apply_telegram_onboarding.<allowed ids>
 *   w28_messaging_ede80b_p2_parse_expiry_ts
 *       @ hermes_cli/web_routers/messaging.py:_parse_expiry_ts
 *   w28_messaging_ede80b_p2_ready_payload
 *       @ hermes_cli/web_routers/messaging.py:_telegram_ready_payload
 *   w28_messaging_ede80b_p2_start_payload
 *       @ hermes_cli/web_routers/messaging.py:start_telegram_onboarding.<response>
 *   w28_messaging_ede80b_p2_status_payload
 *       @ hermes_cli/web_routers/messaging.py:get_telegram_onboarding_status.<ladder>
 *
 * PoP (p3 — platform payload / status / posture):
 *   w28_messaging_ede80b_p3_env_info
 *       @ hermes_cli/web_routers/messaging.py:_messaging_env_info
 *   w28_messaging_ede80b_p3_count_csv
 *       @ hermes_cli/web_routers/messaging.py:_count_csv
 *   w28_messaging_ede80b_p3_platform_state
 *       @ hermes_cli/web_routers/messaging.py:_messaging_platform_payload
 *   w28_messaging_ede80b_p3_posture
 *       @ hermes_cli/web_routers/messaging.py:_messaging_platform_posture
 *   w28_messaging_ede80b_p3_multiplex_conflict
 *       @ hermes_cli/web_routers/messaging.py:_multiplex_port_binding_conflict
 *   w28_messaging_ede80b_p3_test_verdict
 *       @ hermes_cli/web_routers/messaging.py:test_messaging_platform.<verdict ladder>
 *
 * Seams kept out of C (a value is handed in instead of reached for):
 *   - the on-disk .env / config.yaml / gateway_state.json readers
 *     (load_env, load_config, read_runtime_status, resolve_gateway_liveness,
 *      retained_gateway_state) — the values they produce are fixture inputs;
 *   - the platform enablement arm (_platform_enablement): (enabled, configured,
 *     home_channel) arrive as inputs;
 *   - the env_vars rows of the platform payload: the credential preview
 *     (redacted_credential_preview -> mask_secret) is a display concern, so the
 *     already-shaped rows are passed through (fixtures keep them empty);
 *   - the Telegram/WhatsApp pairing service HTTP calls and the process table:
 *     only the request/response shaping and the state machine are ported.
 *
 * Every function is a pure function of its JSON inputs.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "noc_w28_messaging_ede80b_p1.h"
#include "noc_w28_messaging_ede80b_p2.h"
#include "noc_w28_messaging_ede80b_p3.h"

/* ── Python value helpers ─────────────────────────────────────────── */

/* Python truthiness of a JSON value. */
static int w28_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int w28_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static int w28_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static int w28_is_str(const json_t *v) { return v && v->type == JSON_STRING; }

/* Python ``repr`` of a float that came from a JSON number.  JSON has one number
 * type, so an integral value prints without a fractional part; that matches
 * Python's ``str(int)`` for the integer literals a fixture carries. */
static void w28_fmt_num(double v, char *buf, size_t cap) {
    if (v == (double)(long long)v && fabs(v) < 1e15) snprintf(buf, cap, "%.0f", v);
    else snprintf(buf, cap, "%.17g", v);
}

/* ``str(value or default)`` for the scalar shapes an env/body value carries.
 * Containers fall back to the default (no fixture uses one: Python's repr of a
 * list is not a value this port claims). */
static const char *w28_str_or(const json_t *v, const char *def, char *buf, size_t cap) {
    if (!w28_truthy(v)) return def;
    if (w28_is_str(v)) return v->str_val ? v->str_val : "";
    if (v->type == JSON_NUMBER) { w28_fmt_num(v->num_val, buf, cap); return buf; }
    if (v->type == JSON_BOOL) return "True";
    return def;
}

/* Python ``str(v).strip()`` for one scalar, allocated. */
static char *w28_strip_dup(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* Python ``str(value or "").strip()`` allocated. */
static char *w28_str_strip(const json_t *v) {
    char nbuf[64];
    return w28_strip_dup(w28_str_or(v, "", nbuf, sizeof nbuf));
}

static void w28_lower(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* dict.get(key, default): a present null is null, not the default. */
static json_t *w28_copy_or(const json_t *obj, const char *key, json_t *def /*owned*/) {
    const json_t *v = w28_is_obj(obj) ? json_obj_get(obj, key) : NULL;
    if (v) return json_copy(v);
    return def ? def : json_null();
}

/* A Python Optional[str] slot: empty string means None. */
static void w28_set_opt_str(json_t *o, const char *key, const char *s) {
    if (s && *s) json_set(o, key, json_string(s));
    else json_set(o, key, json_null());
}

static int w28_arr_has_str(const json_t *arr, const char *s) {
    size_t i, n = w28_is_arr(arr) ? json_len(arr) : 0;
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(arr, i);
        if (w28_is_str(e) && e->str_val && strcmp(e->str_val, s) == 0) return 1;
    }
    return 0;
}

static json_t *w28_ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(1));
    json_set(o, "value", value ? value : json_null());
    return o;
}

static json_t *w28_err(int status, const char *detail) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(0));
    json_set(o, "status", json_number((double)status));
    json_set(o, "detail", json_string(detail ? detail : ""));
    return o;
}

/* ── Gate p1: WhatsApp onboarding values + pairing event machine ──── */

json_t *w28_messaging_ede80b_p1_normalize_mode(const json_t *value) {
    char nbuf[64];
    char *m = w28_strip_dup(w28_str_or(value, "bot", nbuf, sizeof nbuf));
    json_t *out;
    if (!m) return w28_err(400, "WhatsApp mode must be 'bot' or 'self-chat'.");
    w28_lower(m);
    if (strcmp(m, "bot") != 0 && strcmp(m, "self-chat") != 0)
        out = w28_err(400, "WhatsApp mode must be 'bot' or 'self-chat'.");
    else
        out = w28_ok(json_string(m));
    free(m);
    return out;
}

/* part.strip() truthiness over a byte span. */
static int w28_span_has_nonws(const char *p, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (!isspace((unsigned char)p[i])) return 1;
    return 0;
}

json_t *w28_messaging_ede80b_p1_normalize_allowed_users(const json_t *value) {
    char nbuf[64];
    char *raw = w28_strip_dup(w28_str_or(value, "", nbuf, sizeof nbuf));
    json_t *out;
    char *acc;
    size_t len = 0;
    const char *p;
    if (!raw) return w28_err(500, "out of memory");
    acc = (char *)malloc(strlen(raw) + 1);
    if (!acc) { free(raw); return w28_err(500, "out of memory"); }
    p = raw;
    for (;;) {
        const char *comma = strchr(p, ',');
        size_t plen = comma ? (size_t)(comma - p) : strlen(p);
        size_t k;
        if (w28_span_has_nonws(p, plen)) {
            if (len) acc[len++] = ',';
            for (k = 0; k < plen; k++)
                if (p[k] != ' ') acc[len++] = p[k];   /* part.replace(" ", "") */
        }
        if (!comma) break;
        p = comma + 1;
    }
    acc[len] = '\0';
    out = w28_ok(json_string(acc));
    free(acc);
    free(raw);
    return out;
}

/* _whatsapp_phone_from_identifier as a string result (None -> null). */
static char *w28_phone_digits(const char *raw_in) {
    char *raw = w28_strip_dup(raw_in ? raw_in : "");
    char *digits;
    size_t n = 0, i = 0, end;
    if (!raw) return NULL;
    if (!*raw) { free(raw); return NULL; }
    /* raw.split("@",1)[0].split(":",1)[0] */
    end = strlen(raw);
    for (i = 0; i < end; i++)
        if (raw[i] == '@') { end = i; break; }
    for (i = 0; i < end; i++)
        if (raw[i] == ':') { end = i; break; }
    digits = (char *)malloc(end + 1);
    if (!digits) { free(raw); return NULL; }
    for (i = 0; i < end; i++)
        if (raw[i] >= '0' && raw[i] <= '9') digits[n++] = raw[i];
    digits[n] = '\0';
    free(raw);
    if (!n) { free(digits); return NULL; }
    return digits;
}

json_t *w28_messaging_ede80b_p1_phone_from_identifier(const json_t *value) {
    char nbuf[64];
    char *digits;
    json_t *out;
    const char *raw = w28_str_or(value, "", nbuf, sizeof nbuf);
    digits = w28_phone_digits(raw);
    if (!digits) return w28_ok(json_null());
    out = w28_ok(json_string(digits));
    free(digits);
    return out;
}

json_t *w28_messaging_ede80b_p1_first_str(const json_t *candidate, const json_t *keys) {
    size_t i, n;
    if (!w28_is_obj(candidate)) return w28_ok(json_null());
    n = w28_is_arr(keys) ? json_len(keys) : 0;
    for (i = 0; i < n; i++) {
        const json_t *k = json_get(keys, i);
        char *v;
        if (!w28_is_str(k)) continue;
        v = w28_str_strip(w28_is_obj(candidate) ? json_obj_get(candidate, k->str_val) : NULL);
        if (!v) return w28_err(500, "out of memory");
        if (*v) {
            json_t *out = w28_ok(json_string(v));
            free(v);
            return out;
        }
        free(v);
    }
    return w28_ok(json_null());
}

json_t *w28_messaging_ede80b_p1_linked_account(const json_t *payload) {
    static const char *const id_keys[] = {"id", "jid", "lid"};
    static const char *const name_keys[] = {"name", "verifiedName", "notify", "pushName"};
    const json_t *candidates[3];
    char *account_id = NULL, *account_name = NULL, *phone;
    size_t c, i;
    json_t *o;

    if (!w28_is_obj(payload)) payload = NULL;
    candidates[0] = payload ? json_obj_get(payload, "me") : NULL;
    candidates[1] = payload ? json_obj_get(payload, "account") : NULL;
    candidates[2] = payload;

    for (c = 0; c < 3 && !account_id; c++) {
        if (!w28_is_obj(candidates[c])) continue;
        for (i = 0; i < sizeof id_keys / sizeof id_keys[0]; i++) {
            char *v = w28_str_strip(json_obj_get(candidates[c], id_keys[i]));
            if (!v) continue;
            if (*v) { account_id = v; break; }
            free(v);
        }
    }
    for (c = 0; c < 3 && !account_name; c++) {
        if (!w28_is_obj(candidates[c])) continue;
        for (i = 0; i < sizeof name_keys / sizeof name_keys[0]; i++) {
            char *v = w28_str_strip(json_obj_get(candidates[c], name_keys[i]));
            if (!v) continue;
            if (*v) { account_name = v; break; }
            free(v);
        }
    }
    phone = w28_phone_digits(account_id ? account_id : "");

    o = json_object();
    w28_set_opt_str(o, "account_id", account_id);
    w28_set_opt_str(o, "account_name", account_name);
    w28_set_opt_str(o, "account_phone", phone);
    free(account_id);
    free(account_name);
    free(phone);
    return w28_ok(o);
}

/* _WHATSAPP_PAIRING_ERROR_TEXT (the only readable code the dashboard maps). */
static const char *w28_pairing_error_text(const char *code) {
    static const char *const passkey =
        "This WhatsApp account requires passkey verification, which dashboard setup cannot do. "
        "In a local terminal, run hermes whatsapp --passkey-browser followed by the path to your "
        "Chrome or Edge executable.";
    if (code && strcmp(code, "passkey_required") == 0) return passkey;
    return code;
}

json_t *w28_messaging_ede80b_p1_apply_pairing_event(const json_t *record, const json_t *payload) {
    json_t *rec = json_object();
    char *event;
    json_t *out;

    if (!w28_is_obj(record)) record = NULL;
    if (!w28_is_obj(payload)) payload = NULL;

    json_set(rec, "qr_payload", w28_copy_or(record, "qr_payload", NULL));
    json_set(rec, "status", w28_copy_or(record, "status", json_string("")));
    json_set(rec, "error", w28_copy_or(record, "error", NULL));
    json_set(rec, "account_id", w28_copy_or(record, "account_id", NULL));
    json_set(rec, "account_name", w28_copy_or(record, "account_name", NULL));
    json_set(rec, "account_phone", w28_copy_or(record, "account_phone", NULL));

    event = w28_str_strip(payload ? json_obj_get(payload, "event") : NULL);
    if (!event) { json_free(rec); return w28_err(500, "out of memory"); }

    if (strcmp(event, "qr") == 0) {
        char *qr = w28_str_strip(json_obj_get(payload, "qr"));
        if (qr && *qr) {
            json_set(rec, "qr_payload", json_string(qr));
            json_set(rec, "status", json_string("waiting"));
            json_set(rec, "error", json_null());
        }
        free(qr);
    } else if (strcmp(event, "connected") == 0) {
        const json_t *user = json_obj_get(payload, "user");
        if (w28_is_obj(user)) {
            char *id = w28_str_strip(json_obj_get(user, "id"));
            char *name = w28_str_strip(json_obj_get(user, "name"));
            char *phone = w28_phone_digits(id ? id : "");
            w28_set_opt_str(rec, "account_id", id);
            w28_set_opt_str(rec, "account_name", name);
            w28_set_opt_str(rec, "account_phone", phone);
            free(id);
            free(name);
            free(phone);
        }
        json_set(rec, "status", json_string("connected"));
        json_set(rec, "error", json_null());
    } else if (strcmp(event, "error") == 0) {
        char nbuf[64];
        const char *raw = w28_str_or(json_obj_get(payload, "error"),
                                     "WhatsApp pairing failed.", nbuf, sizeof nbuf);
        json_set(rec, "status", json_string("error"));
        json_set(rec, "error", json_string(w28_pairing_error_text(raw)));
    } else if (strcmp(event, "disconnected") == 0) {
        const json_t *st = json_obj_get(rec, "status");
        if (w28_is_str(st) && strcmp(st->str_val, "starting") == 0)
            json_set(rec, "status", json_string("waiting"));
    }

    free(event);
    out = w28_ok(rec);
    return out;
}

/* ── Gate p2: Telegram onboarding ─────────────────────────────────── */

json_t *w28_messaging_ede80b_p2_normalize_user_id(const json_t *value) {
    char *s = w28_str_strip(value);
    size_t i;
    int ok;
    json_t *out;
    if (!s) return w28_err(500, "out of memory");
    ok = *s != '\0';                       /* \d+ needs at least one digit */
    for (i = 0; ok && s[i]; i++)
        if (!(s[i] >= '0' && s[i] <= '9')) ok = 0;
    if (ok) out = w28_ok(json_string(s));
    else out = w28_ok(json_null());
    free(s);
    return out;
}

json_t *w28_messaging_ede80b_p2_normalize_allowed_ids(const json_t *ids) {
    size_t i, n = w28_is_arr(ids) ? json_len(ids) : 0;
    json_t *arr = json_array();
    json_t *out;

    for (i = 0; i < n; i++) {
        char *s = w28_str_strip(json_get(ids, i));
        size_t k, dup = 0, m;
        if (!s) { json_free(arr); return w28_err(500, "out of memory"); }
        if (!*s) {
            free(s);
            json_free(arr);
            return w28_err(400, "Allowed Telegram user IDs must be numeric.");
        }
        for (k = 0; s[k]; k++) {
            if (!(s[k] >= '0' && s[k] <= '9')) {
                free(s);
                json_free(arr);
                return w28_err(400, "Allowed Telegram user IDs must be numeric.");
            }
        }
        m = json_len(arr);
        for (k = 0; k < m; k++) {
            const json_t *e = json_get(arr, k);
            if (w28_is_str(e) && strcmp(e->str_val, s) == 0) { dup = 1; break; }
        }
        if (!dup) json_append(arr, json_string(s));
        free(s);
    }
    if (json_len(arr) == 0) {
        json_free(arr);
        return w28_err(400, "Add at least one allowed Telegram user ID.");
    }
    out = w28_ok(arr);
    return out;
}

/* Days since 1970-01-01 (Howard Hinnant's civil-days algorithm). */
static long long w28_days_from_civil(long long y, unsigned m, unsigned d) {
    long long era;
    unsigned yoe, doy, doe;
    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2 ? (unsigned)-3 : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097LL + (long long)doe - 719468LL;
}

static int w28_days_in_month(int y, int m) {
    static const int dim[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2) {
        int leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        return leap ? 29 : 28;
    }
    return (m >= 1 && m <= 12) ? dim[m - 1] : 0;
}

static int w28_ndigits(const char *s, int n, int *out) {
    int i, v = 0;
    for (i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = v;
    return 1;
}

/* datetime.fromisoformat() for the ISO-8601 shapes a service expiry carries:
 * YYYY-MM-DD[(T| )HH:MM[:SS[.ffffff]]][Z|±HH:MM|±HHMM|±HH].  0 on failure. */
static int w28_iso_epoch(const char *s, double *out) {
    int y, mo, d, h = 0, mi = 0, se = 0, off = 0;
    double frac = 0.0;
    const char *p;
    long long secs;

    if (!s) return 0;
    if (!w28_ndigits(s, 4, &y)) return 0;
    if (s[4] != '-') return 0;
    if (!w28_ndigits(s + 5, 2, &mo)) return 0;
    if (s[7] != '-') return 0;
    if (!w28_ndigits(s + 8, 2, &d)) return 0;
    p = s + 10;

    if (*p == 'T' || *p == 't' || *p == ' ') {
        p++;
        if (!w28_ndigits(p, 2, &h)) return 0;
        if (p[2] != ':') return 0;
        if (!w28_ndigits(p + 3, 2, &mi)) return 0;
        p += 5;
        if (*p == ':') {
            p++;
            if (!w28_ndigits(p, 2, &se)) return 0;
            p += 2;
            if (*p == '.') {
                int digits = 0;
                double scale = 0.1;
                p++;
                if (!(*p >= '0' && *p <= '9')) return 0;
                while (*p >= '0' && *p <= '9') {
                    if (digits < 6) { frac += (*p - '0') * scale; scale /= 10.0; digits++; }
                    p++;   /* fromisoformat truncates past microseconds */
                }
            }
        }
    }
    if (*p == 'Z' || *p == 'z') {
        p++;
    } else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1, oh = 0, om = 0;
        p++;
        if (!w28_ndigits(p, 2, &oh)) return 0;
        p += 2;
        if (*p == ':') p++;
        if (*p >= '0' && *p <= '9') {
            if (!w28_ndigits(p, 2, &om)) return 0;
            p += 2;
        }
        off = sign * (oh * 3600 + om * 60);
    }
    if (*p != '\0') return 0;

    if (mo < 1 || mo > 12) return 0;
    if (d < 1 || d > w28_days_in_month(y, mo)) return 0;
    if (h > 23 || mi > 59 || se > 59) return 0;

    secs = w28_days_from_civil(y, (unsigned)mo, (unsigned)d) * 86400LL
         + (long long)h * 3600 + (long long)mi * 60 + se - off;
    *out = (double)secs + frac;
    return 1;
}

json_t *w28_messaging_ede80b_p2_parse_expiry_ts(const json_t *value) {
    double epoch;
    if (!w28_is_str(value)) return w28_ok(json_null());   /* fallback seam */
    if (!w28_iso_epoch(value->str_val ? value->str_val : "", &epoch))
        return w28_ok(json_null());                        /* fallback seam */
    return w28_ok(json_number(epoch));
}

json_t *w28_messaging_ede80b_p2_ready_payload(const json_t *record) {
    json_t *o = json_object();
    if (!w28_is_obj(record)) record = NULL;
    json_set(o, "status", json_string("ready"));
    json_set(o, "bot_username", w28_copy_or(record, "bot_username", NULL));
    json_set(o, "owner_user_id", w28_copy_or(record, "owner_user_id", NULL));
    json_set(o, "expires_at", w28_copy_or(record, "expires_at", NULL));
    return w28_ok(o);
}

json_t *w28_messaging_ede80b_p2_start_payload(const json_t *payload) {
    static const char *const incomplete =
        "Telegram setup service returned an incomplete response.";
    char *pairing_id, *poll_token, *expires_at, *deep_link, *qr;
    json_t *o;

    if (!w28_is_obj(payload)) payload = NULL;
    pairing_id = w28_str_strip(payload ? json_obj_get(payload, "pairing_id") : NULL);
    poll_token = w28_str_strip(payload ? json_obj_get(payload, "poll_token") : NULL);
    expires_at = w28_str_strip(payload ? json_obj_get(payload, "expires_at") : NULL);
    deep_link = w28_str_strip(payload ? json_obj_get(payload, "deep_link") : NULL);

    if (!pairing_id || !poll_token || !expires_at || !deep_link) {
        free(pairing_id); free(poll_token); free(expires_at); free(deep_link);
        return w28_err(502, incomplete);
    }
    if (!*pairing_id || !*poll_token || !*expires_at || !*deep_link) {
        free(pairing_id); free(poll_token); free(expires_at); free(deep_link);
        return w28_err(502, incomplete);
    }

    /* str(payload.get("qr_payload") or deep_link).strip() */
    o = json_object();
    json_set(o, "pairing_id", json_string(pairing_id));
    {
        char *suggested = w28_str_strip(payload ? json_obj_get(payload, "suggested_username") : NULL);
        json_set(o, "suggested_username", json_string(suggested ? suggested : ""));
        free(suggested);
    }
    json_set(o, "deep_link", json_string(deep_link));
    {
        const json_t *qr_v = payload ? json_obj_get(payload, "qr_payload") : NULL;
        char nbuf[64];
        qr = w28_strip_dup(w28_truthy(qr_v) ? w28_str_or(qr_v, deep_link, nbuf, sizeof nbuf)
                                            : deep_link);
        json_set(o, "qr_payload", json_string(qr ? qr : ""));
        free(qr);
    }
    json_set(o, "expires_at", json_string(expires_at));

    free(pairing_id); free(poll_token); free(expires_at); free(deep_link);
    return w28_ok(o);
}

/* _telegram_onboarding_error_message for the two codes this ladder reaches. */
static const char *w28_telegram_status_message(const char *code, const char *fallback) {
    if (code && strcmp(code, "expired") == 0) return "Telegram setup expired. Start a new setup.";
    if (code && strcmp(code, "claimed") == 0)
        return "Telegram setup was already claimed. Start a new setup.";
    return fallback;
}

json_t *w28_messaging_ede80b_p2_status_payload(const json_t *record, const json_t *payload) {
    static const char *const incomplete =
        "Telegram setup service returned an incomplete response.";
    char *status;
    json_t *out;

    if (!w28_is_obj(record)) record = NULL;
    if (!w28_is_obj(payload)) payload = NULL;

    if (record && w28_truthy(json_obj_get(record, "bot_token")))
        return w28_messaging_ede80b_p2_ready_payload(record);

    status = w28_str_strip(payload ? json_obj_get(payload, "status") : NULL);
    if (!status) return w28_err(500, "out of memory");

    if (strcmp(status, "waiting") == 0) {
        json_t *o = json_object();
        char *exp = record ? w28_str_strip(json_obj_get(record, "expires_at")) : NULL;
        json_set(o, "status", json_string("waiting"));
        json_set(o, "expires_at", json_string(exp ? exp : ""));
        free(exp);
        out = w28_ok(o);
    } else if (strcmp(status, "ready") == 0) {
        char *token = w28_str_strip(json_obj_get(payload, "token"));
        if (!token || !*token) {
            free(token);
            free(status);
            return w28_err(502, incomplete);
        }
        {
            json_t *next = json_object();
            char *name = w28_str_strip(json_obj_get(payload, "bot_username"));
            json_t *norm = w28_messaging_ede80b_p2_normalize_user_id(
                json_obj_get(payload, "owner_user_id"));
            json_set(next, "bot_token", json_string(token));
            w28_set_opt_str(next, "bot_username", name);
            json_set(next, "owner_user_id", json_copy(json_obj_get(norm, "value")));
            json_set(next, "expires_at", w28_copy_or(record, "expires_at", json_string("")));
            out = w28_messaging_ede80b_p2_ready_payload(next);
            json_free(next);
            json_free(norm);
            free(name);
        }
        free(token);
    } else if (strcmp(status, "expired") == 0 || strcmp(status, "claimed") == 0) {
        out = w28_err(410, w28_telegram_status_message(
                               status, "Telegram setup is no longer available. Start a new setup."));
    } else {
        out = w28_err(502, "Telegram setup service returned an unknown status.");
    }
    free(status);
    return out;
}

/* ── Gate p3: platform payload / status / posture ─────────────────── */

/* re.compile(r"_ALLOWED(?:_[A-Z]+)?$").search(key) */
static int w28_allowlist_key(const char *key) {
    size_t n = strlen(key), i;
    for (i = 0; i + 8 <= n; i++) {
        size_t j;
        if (memcmp(key + i, "_ALLOWED", 8) != 0) continue;
        j = i + 8;
        if (j == n) return 1;
        if (key[j] != '_') continue;
        j++;
        if (j >= n) continue;
        while (j < n && key[j] >= 'A' && key[j] <= 'Z') j++;
        if (j == n) return 1;
    }
    return 0;
}

json_t *w28_messaging_ede80b_p3_env_info(const json_t *key, const json_t *info) {
    const char *k = w28_is_str(key) ? (key->str_val ? key->str_val : "") : "";
    const json_t *i = w28_is_obj(info) ? info : NULL;
    json_t *o = json_object();
    int is_password = w28_truthy(i ? json_obj_get(i, "password") : NULL);

    json_set(o, "description", w28_copy_or(i, "description", json_string("")));
    json_set(o, "prompt", w28_copy_or(i, "prompt", json_string(k)));
    json_set(o, "help", w28_copy_or(i, "help", json_string("")));
    json_set(o, "url", w28_copy_or(i, "url", NULL));
    json_set(o, "is_password", json_bool(is_password));
    json_set(o, "is_list", json_bool(!is_password && w28_allowlist_key(k)));
    json_set(o, "advanced", w28_copy_or(i, "advanced", json_bool(0)));
    return w28_ok(o);
}

json_t *w28_messaging_ede80b_p3_count_csv(const json_t *value) {
    char *raw;
    size_t n = 0;
    const char *p;
    if (!w28_truthy(value)) return w28_ok(json_number(0.0));
    if (!w28_is_str(value)) return w28_ok(json_number(0.0));
    raw = (char *)(value->str_val ? value->str_val : "");
    p = raw;
    for (;;) {
        const char *comma = strchr(p, ',');
        size_t plen = comma ? (size_t)(comma - p) : strlen(p);
        if (w28_span_has_nonws(p, plen)) n++;
        if (!comma) break;
        p = comma + 1;
    }
    return w28_ok(json_number((double)n));
}

/* env_value(key): the profile's own .env, else (unscoped) the process env. */
static const char *w28_env_value(const json_t *env_on_disk, const char *key, int scoped) {
    const json_t *v = w28_is_obj(env_on_disk) ? json_obj_get(env_on_disk, key) : NULL;
    if (w28_truthy(v)) return w28_is_str(v) && v->str_val ? v->str_val : "";
    if (scoped) return "";
    {
        const char *e = getenv(key);
        return e ? e : "";
    }
}

json_t *w28_messaging_ede80b_p3_platform_state(const json_t *in) {
    const json_t *entry = w28_is_obj(in) ? json_obj_get(in, "entry") : NULL;
    const json_t *env_on_disk = w28_is_obj(in) ? json_obj_get(in, "env_on_disk") : NULL;
    const json_t *runtime = w28_is_obj(in) ? json_obj_get(in, "runtime") : NULL;
    const json_t *home_channel_in = w28_is_obj(in) ? json_obj_get(in, "home_channel") : NULL;
    const json_t *mirrors = w28_is_obj(in) ? json_obj_get(in, "mirrors") : NULL;
    const json_t *relay_disabled = w28_is_obj(in) ? json_obj_get(in, "relay_disabled") : NULL;
    const json_t *relay_fronted = w28_is_obj(in) ? json_obj_get(in, "relay_fronted") : NULL;
    const json_t *env_rows = w28_is_obj(in) ? json_obj_get(in, "env_vars") : NULL;
    int scoped = w28_is_obj(in) && w28_truthy(json_obj_get(in, "scoped"));
    int gateway_running = w28_is_obj(in) && w28_truthy(json_obj_get(in, "gateway_running"));
    int enabled = w28_is_obj(in) && w28_truthy(json_obj_get(in, "enabled"));
    int configured = w28_is_obj(in) && w28_truthy(json_obj_get(in, "configured"));
    int relay_exclusive_view = w28_is_obj(in) && w28_truthy(json_obj_get(in, "relay_exclusive"));
    const char *platform_id, *retained;
    const json_t *rt, *rps, *rp;
    const json_t *state_v;
    json_t *state_out, *o, *home_channel_out, *ec_v, *em_v;
    int mirrored_from, fronted, disabled_by_relay;

    if (!w28_is_obj(entry)) entry = NULL;
    platform_id = (entry && w28_is_str(json_obj_get(entry, "id")))
                      ? json_obj_get(entry, "id")->str_val : "";

    rt = w28_is_obj(runtime) ? runtime : NULL;
    rps = rt ? json_obj_get(rt, "platforms") : NULL;
    rp = (w28_is_obj(rps) && *platform_id) ? json_obj_get(rps, platform_id) : NULL;
    if (!w28_is_obj(rp)) rp = NULL;
    if (!gateway_running) rp = NULL;   /* a dead writer's verdict is history */

    mirrored_from = rp && w28_truthy(json_obj_get(rp, "mirrored_from"));
    if (gateway_running && mirrored_from && w28_arr_has_str(mirrors, platform_id)) {
        enabled = 1;
        configured = 1;
    } else if (mirrored_from && !(enabled && configured)) {
        rp = NULL;   /* the default's Relay is not this profile's channel */
    }

    state_v = rp ? json_obj_get(rp, "state") : NULL;
    if (!enabled) state_out = json_string("disabled");
    else if (!configured) state_out = json_string("not_configured");
    else if (gateway_running && !w28_truthy(state_v)) state_out = json_string("pending_restart");
    else if (!gateway_running && !w28_truthy(state_v)) {
        retained = w28_is_obj(in) && w28_is_str(json_obj_get(in, "retained_state"))
                       ? json_obj_get(in, "retained_state")->str_val : "";
        state_out = json_string(retained && strcmp(retained, "startup_failed") == 0
                                    ? "startup_failed" : "gateway_stopped");
    } else {
        state_out = state_v ? json_copy(state_v) : json_null();
    }

    ec_v = rp ? json_obj_get(rp, "error_code") : NULL;
    em_v = rp ? json_obj_get(rp, "error_message") : NULL;
    if (w28_is_str(state_out) && strcmp(state_out->str_val, "startup_failed") == 0) {
        const json_t *exit_reason = rt ? json_obj_get(rt, "exit_reason") : NULL;
        ec_v = w28_truthy(ec_v) ? json_copy(ec_v) : json_string("startup_failed");
        em_v = w28_truthy(em_v) ? json_copy(em_v)
                                : (exit_reason ? json_copy(exit_reason) : json_null());
    } else {
        ec_v = ec_v ? json_copy(ec_v) : json_null();
        em_v = em_v ? json_copy(em_v) : json_null();
    }

    fronted = w28_arr_has_str(relay_fronted, platform_id);
    disabled_by_relay = !enabled && w28_arr_has_str(relay_disabled, platform_id);

    home_channel_out = w28_is_obj(home_channel_in) ? json_copy(home_channel_in) : json_null();

    o = json_object();
    json_set(o, "id", json_string(platform_id));
    json_set(o, "name", w28_copy_or(entry, "name", json_null()));
    json_set(o, "description", w28_copy_or(entry, "description", json_null()));
    json_set(o, "docs_url", w28_copy_or(entry, "docs_url", json_null()));
    json_set(o, "enabled", json_bool(enabled));
    json_set(o, "configured", json_bool(configured));
    json_set(o, "gateway_running", json_bool(gateway_running));
    json_set(o, "state", state_out);
    json_set(o, "error_code", ec_v);
    json_set(o, "error_message", em_v);
    json_set(o, "updated_at", w28_copy_or(rp, "updated_at", NULL));
    json_set(o, "home_channel", home_channel_out);
    json_set(o, "env_vars", env_rows ? json_copy(env_rows) : json_array());
    json_set(o, "disabled_by_relay", json_bool(disabled_by_relay));
    json_set(o, "relay_fronted", json_bool(fronted));
    json_set(o, "relay_exclusive", json_bool(fronted && relay_exclusive_view));
    json_set(o, "ingress_url",
             w28_copy_or(gateway_running ? rp : NULL, "ingress_url", NULL));

    if (strcmp(platform_id, "whatsapp") == 0) {
        char *mode = w28_strip_dup(w28_env_value(env_on_disk, "WHATSAPP_MODE", scoped));
        char *users = w28_strip_dup(w28_env_value(env_on_disk, "WHATSAPP_ALLOWED_USERS", scoped));
        json_t *ws = json_object();
        json_set(ws, "mode", json_string(
            (mode && (strcmp(mode, "bot") == 0 || strcmp(mode, "self-chat") == 0)) ? mode : ""));
        json_set(ws, "allowed_users_set", json_bool(users && *users));
        json_set(ws, "home_channel_set", json_bool(w28_is_obj(home_channel_in)));
        json_set(o, "whatsapp_setup", ws);
        free(mode);
        free(users);
    }
    return w28_ok(o);
}

/* The ordered allowlist scan of _messaging_platform_posture: {name: value} with
 * allowed_env first and the gateway/telegram vars after it, deduped by name. */
json_t *w28_messaging_ede80b_p3_posture(const json_t *in) {
    const json_t *entry = w28_is_obj(in) ? json_obj_get(in, "entry") : NULL;
    const json_t *env = w28_is_obj(in) ? json_obj_get(in, "env") : NULL;
    const char *platform_id = (w28_is_obj(in) && w28_is_str(json_obj_get(in, "platform_id")))
                                  ? json_obj_get(in, "platform_id")->str_val : "";
    const char *name = (entry && w28_is_str(json_obj_get(entry, "name")))
                           ? json_obj_get(entry, "name")->str_val : "";
    const char *allowed_env = (w28_is_obj(in) && w28_is_str(json_obj_get(in, "allowed_env")))
                                  ? json_obj_get(in, "allowed_env")->str_val : "";
    const char *allow_all_env = (w28_is_obj(in) && w28_is_str(json_obj_get(in, "allow_all_env")))
                                    ? json_obj_get(in, "allow_all_env")->str_val : "";
    const char *dm_policy = (w28_is_obj(in) && w28_is_str(json_obj_get(in, "dm_policy")))
                                ? json_obj_get(in, "dm_policy")->str_val : "";
    double approved = w28_is_obj(in) ? json_get_num(in, "approved", 0.0) : 0.0;
    double pending = w28_is_obj(in) ? json_get_num(in, "pending", 0.0) : 0.0;
    const char *names[4];
    const char *values[4];
    size_t count = 0, i;
    const char *allow_all_value;
    char low[16];
    json_t *o;

    (void)env;
    if (!allowed_env || !*allowed_env) return w28_ok(json_null());

    allow_all_value = w28_env_value(env, allow_all_env, 1);
    snprintf(low, sizeof low, "%s", allow_all_value);
    w28_lower(low);
    if (*allow_all_env && (strcmp(low, "true") == 0 || strcmp(low, "1") == 0 ||
                           strcmp(low, "yes") == 0)) {
        char *msg = (char *)malloc(strlen(name) + strlen(allow_all_env) + 64);
        o = json_object();
        if (!msg) return w28_err(500, "out of memory");
        sprintf(msg, "%s is open: %s is set.", name, allow_all_env);
        json_set(o, "posture", json_string("open"));
        json_set(o, "message", json_string(msg));
        free(msg);
        return w28_ok(o);
    }
    {
        const char *g = w28_env_value(env, "GATEWAY_ALLOW_ALL_USERS", 1);
        snprintf(low, sizeof low, "%s", g);
        w28_lower(low);
        if (strcmp(low, "true") == 0 || strcmp(low, "1") == 0 || strcmp(low, "yes") == 0) {
            o = json_object();
            json_set(o, "posture", json_string("open"));
            json_set(o, "message", json_string("Gateway is open: GATEWAY_ALLOW_ALL_USERS is set."));
            return w28_ok(o);
        }
    }

    names[count] = allowed_env;
    values[count] = w28_env_value(env, allowed_env, 1);
    count++;
    names[count] = "GATEWAY_ALLOWED_USERS";
    values[count] = w28_env_value(env, "GATEWAY_ALLOWED_USERS", 1);
    count++;
    if (strcmp(platform_id, "telegram") == 0) {
        names[count] = "TELEGRAM_GROUP_ALLOWED_USERS";
        values[count] = w28_env_value(env, "TELEGRAM_GROUP_ALLOWED_USERS", 1);
        count++;
        names[count] = "TELEGRAM_GROUP_ALLOWED_CHATS";
        values[count] = w28_env_value(env, "TELEGRAM_GROUP_ALLOWED_CHATS", 1);
        count++;
    }

    for (i = 0; i < count; i++) {
        char *trimmed = w28_strip_dup(values[i] ? values[i] : "");
        if (!trimmed) return w28_err(500, "out of memory");
        if (*trimmed) {
            /* _count_csv over the value */
            size_t n = 0;
            const char *p = values[i];
            for (;;) {
                const char *comma = strchr(p, ',');
                size_t plen = comma ? (size_t)(comma - p) : strlen(p);
                if (w28_span_has_nonws(p, plen)) n++;
                if (!comma) break;
                p = comma + 1;
            }
            {
                char *msg = (char *)malloc(strlen(names[i]) + 64);
                o = json_object();
                if (!msg) { free(trimmed); return w28_err(500, "out of memory"); }
                sprintf(msg, "Connected. %zu user%s allowlisted via %s.", n,
                        n == 1 ? "" : "s", names[i]);
                json_set(o, "posture", json_string("allowlist"));
                json_set(o, "message", json_string(msg));
                free(msg);
                free(trimmed);
                return w28_ok(o);
            }
        }
        free(trimmed);
    }

    if (dm_policy && strcmp(dm_policy, "allowlist") == 0) {
        o = json_object();
        json_set(o, "posture", json_string("allowlist"));
        json_set(o, "message",
                 json_string("Connected. Access restricted by the configured allow_from policy."));
        return w28_ok(o);
    }
    if (approved != 0.0) {
        char *msg = (char *)malloc(96);
        o = json_object();
        if (!msg) return w28_err(500, "out of memory");
        sprintf(msg, "Connected. %.0f user%s paired.", approved, approved == 1.0 ? "" : "s");
        json_set(o, "posture", json_string("pairing"));
        json_set(o, "message", json_string(msg));
        free(msg);
        return w28_ok(o);
    }
    {
        const char *tail = " is connected, but no users are authorized yet \xe2\x80\x94 set ";
        const char *mid = ", approve a pairing request, or enable pairing for new users.";
        char *base = (char *)malloc(strlen(name) + strlen(tail) + strlen(allowed_env) +
                                    strlen(mid) + 128);
        size_t used;
        o = json_object();
        if (!base) return w28_err(500, "out of memory");
        used = (size_t)sprintf(base, "%s%s%s%s", name, tail, allowed_env, mid);
        if (pending != 0.0)
            sprintf(base + used, " %.0f pairing request%s pending approval.", pending,
                    pending == 1.0 ? "" : "s");
        json_set(o, "posture", json_string("deny_all"));
        json_set(o, "message", json_string(base));
        free(base);
        return w28_ok(o);
    }
}

json_t *w28_messaging_ede80b_p3_multiplex_conflict(const json_t *in) {
    const json_t *mirrors = w28_is_obj(in) ? json_obj_get(in, "mirrors") : NULL;
    const json_t *active = w28_is_obj(in) ? json_obj_get(in, "active_profile") : NULL;
    const char *platform_id = (w28_is_obj(in) && w28_is_str(json_obj_get(in, "platform_id")))
                                  ? json_obj_get(in, "platform_id")->str_val : "";
    char *requested;
    const char *target;
    int multiplexes = w28_is_obj(in) && w28_truthy(json_obj_get(in, "multiplexes"));
    char low[64];
    char *msg;

    if (!w28_arr_has_str(mirrors, platform_id)) return w28_ok(json_null());
    requested = w28_str_strip(w28_is_obj(in) ? json_obj_get(in, "requested_profile") : NULL);
    if (!requested) return w28_err(500, "out of memory");
    snprintf(low, sizeof low, "%s", requested);
    w28_lower(low);
    if (!*requested || strcmp(low, "current") == 0)
        target = (active && w28_is_str(active) && active->str_val) ? active->str_val : "";
    else
        target = requested;
    if (!target || !*target || strcmp(target, "default") == 0 || strcmp(target, "custom") == 0) {
        free(requested);
        return w28_ok(json_null());
    }
    if (!multiplexes) { free(requested); return w28_ok(json_null()); }

    msg = (char *)malloc(strlen(platform_id) + strlen(target) * 3 + 512);
    if (!msg) { free(requested); return w28_err(500, "out of memory"); }
    sprintf(msg,
            "Cannot enable '%s' on profile '%s': gateway.multiplex_profiles is on and the "
            "default profile's listener already serves it for every profile at /p/%s/. "
            "Configure it on the default profile instead (disabling or clearing it here is "
            "still allowed).",
            platform_id, target, target);
    {
        json_t *out = w28_ok(json_string(msg));
        free(msg);
        free(requested);
        return out;
    }
}

json_t *w28_messaging_ede80b_p3_test_verdict(const json_t *in) {
    const json_t *entry = w28_is_obj(in) ? json_obj_get(in, "entry") : NULL;
    const json_t *payload = w28_is_obj(in) ? json_obj_get(in, "payload") : NULL;
    const json_t *posture = w28_is_obj(in) ? json_obj_get(in, "posture") : NULL;
    const char *name = (entry && w28_is_str(json_obj_get(entry, "name")))
                           ? json_obj_get(entry, "name")->str_val : "";
    const json_t *state_v = payload ? json_obj_get(payload, "state") : NULL;
    json_t *o;

    if (!w28_is_obj(payload)) payload = NULL;

    if (!w28_truthy(payload ? json_obj_get(payload, "enabled") : NULL)) {
        char *msg = (char *)malloc(strlen(name) + 96);
        o = json_object();
        if (!msg) return w28_err(500, "out of memory");
        sprintf(msg, "%s is disabled. Enable it, then restart the gateway.", name);
        json_set(o, "ok", json_bool(0));
        json_set(o, "state", state_v ? json_copy(state_v) : json_null());
        json_set(o, "message", json_string(msg));
        free(msg);
        return w28_ok(o);
    }
    if (!w28_truthy(json_obj_get(payload, "configured"))) {
        const json_t *rows = json_obj_get(payload, "env_vars");
        size_t i, n = w28_is_arr(rows) ? json_len(rows) : 0;
        char *msg = NULL;
        size_t len = 0, cap = 64;
        int any = 0;
        msg = (char *)malloc(cap);
        if (!msg) return w28_err(500, "out of memory");
        msg[0] = '\0';
        for (i = 0; i < n; i++) {
            const json_t *row = json_get(rows, i);
            const json_t *key;
            size_t need;
            if (!w28_is_obj(row)) continue;
            if (!w28_truthy(json_obj_get(row, "required"))) continue;
            key = json_obj_get(row, "key");
            if (!w28_is_str(key)) continue;
            if (w28_truthy(json_obj_get(row, "is_set"))) continue;
            need = len + strlen(key->str_val) + 3;
            if (need > cap) {
                char *grown = (char *)realloc(msg, need + 128);
                if (!grown) { free(msg); return w28_err(500, "out of memory"); }
                msg = grown;
                cap = need + 128;
            }
            if (any) { msg[len++] = ','; msg[len++] = ' '; }
            len += (size_t)sprintf(msg + len, "%s", key->str_val);
            any = 1;
        }
        o = json_object();
        json_set(o, "ok", json_bool(0));
        json_set(o, "state", state_v ? json_copy(state_v) : json_null());
        if (any) {
            char *full = (char *)malloc(len + 32);
            if (!full) { free(msg); json_free(o); return w28_err(500, "out of memory"); }
            sprintf(full, "Missing required setup: %s", msg);
            json_set(o, "message", json_string(full));
            free(full);
        } else {
            json_set(o, "message", json_string("Platform setup is incomplete."));
        }
        free(msg);
        return w28_ok(o);
    }
    if (!w28_truthy(json_obj_get(payload, "gateway_running"))) {
        o = json_object();
        json_set(o, "ok", json_bool(0));
        json_set(o, "state", state_v ? json_copy(state_v) : json_null());
        json_set(o, "message",
                 json_string("Gateway is not running. Restart the gateway to connect this platform."));
        return w28_ok(o);
    }
    if (w28_is_str(state_v) && strcmp(state_v->str_val, "connected") == 0) {
        const json_t *pmsg = w28_is_obj(posture) ? json_obj_get(posture, "message") : NULL;
        const json_t *ppos = w28_is_obj(posture) ? json_obj_get(posture, "posture") : NULL;
        if (!w28_is_obj(posture)) {
            char *msg = (char *)malloc(strlen(name) + 32);
            o = json_object();
            if (!msg) return w28_err(500, "out of memory");
            sprintf(msg, "%s is connected.", name);
            json_set(o, "ok", json_bool(1));
            json_set(o, "state", json_copy(state_v));
            json_set(o, "message", json_string(msg));
            free(msg);
            return w28_ok(o);
        }
        o = json_object();
        json_set(o, "ok", json_bool(1));
        json_set(o, "state", json_copy(state_v));
        json_set(o, "message", pmsg ? json_copy(pmsg) : json_null());
        json_set(o, "auth_posture", ppos ? json_copy(ppos) : json_null());
        return w28_ok(o);
    }
    {
        const json_t *em = json_obj_get(payload, "error_message");
        o = json_object();
        json_set(o, "ok", json_bool(0));
        json_set(o, "state", state_v ? json_copy(state_v) : json_null());
        if (w28_truthy(em))
            json_set(o, "message", json_copy(em));
        else
            json_set(o, "message", json_string(
                "Setup looks complete, but the gateway has not reported a connection yet. "
                "Restart the gateway."));
        return w28_ok(o);
    }
}
