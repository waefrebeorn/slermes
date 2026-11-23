/*
 * anon_challenge.c — C port of upstream Hermes hermes_cli/anon_challenge.py.
 *
 * PoP: w12_anonchalle_ed340f_server_message @ hermes_cli/anon_challenge.py:server_message
 * PoP: w12_anonchalle_ed340f_same_origin @ hermes_cli/anon_challenge.py:_same_origin
 * PoP: w12_anonchalle_ed340f_number @ hermes_cli/anon_challenge.py:_number
 * PoP: w12_anonchalle_ed340f_parse @ hermes_cli/anon_challenge.py:parse_browser_challenge
 * PoP: w12_anonchalle_ed340f_constants @ hermes_cli/anon_challenge.py:<module constants>
 *
 * Only the parsing / validation leaf functions are pure; the presenting,
 * polling and state-machine functions depend on network, threads and env and
 * are the caller's responsibility (mirroring the live module's seams).
 */
#include "noc_w12_anonchalle_ed340f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHALLENGE_COPY "Nous needs to run a quick check before starting your free session."

struct json_t *w12_anonchalle_ed340f_server_message(const json_t *v) {
    if (!v || v->type != JSON_STRING) return json_string("");
    const char *s = v->str_val;
    char buf[8192];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n + 1 < sizeof(buf); p++) {
        unsigned char c = *p;
        /* str.isprintable(): ASCII space/visible are printable, controls/DEL are not.
         * Non-ASCII bytes are kept (printable for the fixture's Unicode letters). */
        if (c == ' ' || (c >= 0x21 && c <= 0x7e) || c >= 0x80) buf[n++] = (char)c;
        else buf[n++] = ' ';
    }
    buf[n] = '\0';
    /* " ".join(<collapsed>.split()): collapse whitespace runs, trim both ends. */
    char out[8192];
    size_t m = 0;
    int in_space = 1;                    /* pretend leading space -> trim leading */
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == ' ') {
            if (!in_space) { out[m++] = ' '; in_space = 1; }
        } else {
            out[m++] = buf[i]; in_space = 0;
        }
    }
    if (m > 0 && out[m - 1] == ' ') m--; /* trim trailing: NUL at the NEW length */
    out[m] = '\0';
    if (m == 0 || m > 300) return json_string("");
    return json_string(out);
}

/* Minimal urlparse sufficient for scheme/netloc same-origin checks. */
static void parse_url(const char *u, char *scheme, size_t sc_cap, char *netloc, size_t nl_cap) {
    scheme[0] = '\0'; netloc[0] = '\0';
    if (!u) return;
    const char *p = u;
    while (*p && *p != ':') p++;
    if (*p != ':' || p[1] != '/' || p[2] != '/') return;
    size_t sl = (size_t)(p - u);
    if (sl == 0 || sl >= sc_cap) return;
    for (size_t i = 0; i < sl; i++) if (!isalpha((unsigned char)u[i])) return;
    const char *n = p + 3;
    const char *e = n;
    while (*e && *e != '/' && *e != '?' && *e != '#') e++;
    size_t nl = (size_t)(e - n);
    if (nl == 0 || nl >= nl_cap) return;
    for (size_t i = 0; i < sl; i++) scheme[i] = (char)tolower((unsigned char)u[i]);
    scheme[sl] = '\0';
    memcpy(netloc, n, nl);
    netloc[nl] = '\0';
}

int w12_anonchalle_ed340f_same_origin(const char *url, const char *portal_base_url) {
    char ts[32], tn[1024], ps[32], pn[1024];
    parse_url(url, ts, sizeof(ts), tn, sizeof(tn));
    parse_url(portal_base_url, ps, sizeof(ps), pn, sizeof(pn));
    if (strcmp(ts, "http") && strcmp(ts, "https")) return 0;
    if (strcmp(ts, ps)) return 0;
    if (!tn[0] || !pn[0]) return 0;
    for (char *a = tn, *b = pn; *a; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return 1;
}

static double numval(const json_t *v, double def) {
    if (!v || v->type == JSON_BOOL || v->type != JSON_NUMBER) return def;
    if (!isfinite(v->num_val)) return def;
    return v->num_val;
}

struct json_t *w12_anonchalle_ed340f_number(const json_t *value, double default_value) {
    return json_number(numval(value, default_value));
}

static int printable_str(const char *s) {
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (!((c >= 0x20 && c <= 0x7e) || c >= 0x80)) return 0;
    }
    return 1;
}

struct json_t *w12_anonchalle_ed340f_parse(const json_t *payload, const char *portal_base_url) {
    if (!json_is_object(payload)) return json_null();
    json_t *entries = json_obj_get(payload, "challenges");
    if (!json_is_array(entries)) return json_null();

    char msgbuf[8192];
    const char *msg = CHALLENGE_COPY;
    json_t *m = w12_anonchalle_ed340f_server_message(json_obj_get(payload, "message"));
    if (m && m->type == JSON_STRING && m->str_val[0]) {
        strncpy(msgbuf, m->str_val, sizeof(msgbuf) - 1);
        msgbuf[sizeof(msgbuf) - 1] = '\0';
        msg = msgbuf;                    /* copy OUT before freeing m */
    }
    json_free(m);

    for (size_t i = 0; i < json_len(entries); i++) {
        json_t *entry = json_get(entries, i);
        if (!json_is_object(entry)) continue;
        json_t *t = json_obj_get(entry, "type");
        if (!t || t->type != JSON_STRING || strcmp(t->str_val, "browser")) continue;
        json_t *u = json_obj_get(entry, "url");
        if (!u || u->type != JSON_STRING || !printable_str(u->str_val)) continue;
        if (!w12_anonchalle_ed340f_same_origin(u->str_val, portal_base_url)) continue;

        int required = 1;
        json_t *req = json_obj_get(entry, "required");
        if (req && req->type == JSON_BOOL && req->bool_val == false) required = 0;

        double ex = numval(json_obj_get(entry, "expires_in"), 600.0);
        if (ex < 30.0) ex = 30.0;
        if (ex > 900.0) ex = 900.0;
        double iv = numval(json_obj_get(entry, "interval"), 2.0);
        if (iv < 1.0) iv = 1.0;
        if (iv > 5.0) iv = 5.0;

        json_t *res = json_object();
        json_set(res, "type", json_string("browser"));
        json_set(res, "url", json_string(u->str_val));
        json_set(res, "required", json_bool(required ? true : false));
        json_set(res, "expires_in", json_number((double)(long long)ex));
        json_set(res, "message", json_string(msg));
        json_set(res, "attempt", json_number(0.0));
        return res;
    }
    return json_null();
}

struct json_t *w12_anonchalle_ed340f_constants(void) {
    json_t *o = json_object();
    json_set(o, "BROWSER_CAPABILITY", json_string("browser-v1"));
    json_set(o, "CHALLENGE_EVENT", json_string("free_tier.challenge"));
    json_set(o, "CHALLENGE_COPY", json_string(CHALLENGE_COPY));
    json_set(o, "CHALLENGE_WAIT_SECONDS", json_number(120.0));
    json_set(o, "_POLL_MIN_SECONDS", json_number(1.0));
    json_set(o, "_POLL_MAX_SECONDS", json_number(5.0));
    json_set(o, "_EXPIRES_MIN_SECONDS", json_number(30.0));
    json_set(o, "_EXPIRES_MAX_SECONDS", json_number(900.0));
    json_set(o, "_EXPIRES_DEFAULT_SECONDS", json_number(600.0));
    json_set(o, "_MESSAGE_MAX_CHARS", json_number(300.0));
    json_set(o, "_GAVE_UP_COOLDOWN_SECONDS", json_number(30.0));
    return o;
}
