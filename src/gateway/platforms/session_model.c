/*
 * session_model.c - real C11 port of upstream Hermes
 *   gateway/platforms/session_model.py pure helper(s).
 * PoP: w19_sessionmod_09f8f1_display_base_url @ gateway/platforms/session_model.py:_display_base_url
 * PoP: w19_sessionmod_09f8f1_resolve_settings @ gateway/platforms/session_model.py:_resolve_session_model_settings
 * The runner / session-store / channel collaborators are injected as plain inputs, since
 * the live code only reads them for these two answers.
 */
#include "noc_w19_sessionmod_09f8f1.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ---- Python truthiness ---------------------------------------------------- */
static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return json_len(v) > 0 ? 1 : 0;
        case JSON_OBJECT: return json_object_size(v) > 0 ? 1 : 0;
        default:          return 0;
    }
}

/* Python float repr: shortest round-tripping decimal form. */
static void py_float_repr(double v, char *buf, size_t cap) {
    if (v != v) { snprintf(buf, cap, "nan"); return; }
    if (v == (double)1 / 0.0) { snprintf(buf, cap, "inf"); return; }
    if (v == (double)-1 / 0.0) { snprintf(buf, cap, "-inf"); return; }
    if (v == (double)(long long)v && v < 1e16 && v > -1e16) {
        snprintf(buf, cap, "%lld", (long long)v);
        return;
    }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, v);
        if (strtod(buf, NULL) == v) return;
    }
}

/* str() of a JSON value, as the live f-string coercion uses it. */
static void py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL:   snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER: py_float_repr(v->num_val, buf, cap); return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        case JSON_ARRAY:  snprintf(buf, cap, "[...]"); return;
        case JSON_OBJECT: snprintf(buf, cap, "{...}"); return;
        default:          snprintf(buf, cap, "None"); return;
    }
}

/* str.strip() over ASCII whitespace. */
static void strip_ascii_inplace(char *s) {
    if (!s) return;
    size_t n = strlen(s), i = 0;
    while (i < n && isspace((unsigned char)s[i])) i++;
    size_t j = n;
    while (j > i && isspace((unsigned char)s[j - 1])) j--;
    memmove(s, s + i, j - i);
    s[j - i] = '\0';
}

/* ---- urlsplit subset ----------------------------------------------------- */

static int is_scheme_char(int c, int first) {
    if (first) return isalpha(c) ? 1 : 0;
    return (isalnum(c) || c == '+' || c == '-' || c == '.') ? 1 : 0;
}

/* ipaddress.IPv6Address acceptance for the bracketed-host check. */
static int valid_ipv6(const char *host) {
    if (!host || !*host) return 0;
    const char *p = host;
    int colons = 0, dcolon = 0, digits = 0, dot = 0;
    while (*p) {
        if (*p == '%') { /* zone id: non-empty tail */
            p++;
            if (!*p) return 0;
            while (*p) {
                if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_' && *p != '.') return 0;
                p++;
            }
            break;
        }
        if (*p == ':') {
            if (*(p + 1) == ':') {
                if (dcolon) return 0;
                dcolon = 1;
                p += 2;
                if (!*p) break;
                continue;
            }
            if (digits == 0) return 0;
            colons++;
            digits = 0;
            p++;
            continue;
        }
        if (*p == '.') { dot = 1; digits = 0; p++; continue; }
        if (!isxdigit((unsigned char)*p)) return 0;
        digits++;
        if (digits > 4 && !dot) return 0;
        p++;
    }
    if (digits > 0) colons++;
    return colons > 0 || dcolon;
}

/* urlsplit + urlunsplit((scheme, netloc.rsplit("@")[-1], path, "", "")). */
char *w19_sessionmod_09f8f1_display_base_url(const char *value) {
    if (!value || !*value) return dupstr("");
    /* ASCII newlines and tabs are stripped from the URL before splitting. */
    size_t len = strlen(value);
    char *work = (char *)malloc(len + 1);
    if (!work) return dupstr("");
    size_t w = 0;
    for (size_t i = 0; i < len; i++) {
        char c = value[i];
        if (c == '\t' || c == '\r' || c == '\n') continue;
        work[w++] = c;
    }
    work[w] = '\0';
    /* Leading C0 controls and spaces are dropped; trailing bytes are kept. */
    size_t start = 0;
    while (work[start] && (unsigned char)work[start] <= 0x20) start++;
    char *url = work + start;

    /* scheme */
    size_t i = 0;
    if (!isalpha((unsigned char)url[0])) { free(work); return dupstr(""); }
    while (url[i] && url[i] != ':') {
        if (!is_scheme_char((unsigned char)url[i], i == 0)) { free(work); return dupstr(""); }
        i++;
    }
    if (url[i] != ':' || i == 0) { free(work); return dupstr(""); }
    char *scheme = (char *)malloc(i + 1);
    if (!scheme) { free(work); return dupstr(""); }
    for (size_t k = 0; k < i; k++) scheme[k] = (char)tolower((unsigned char)url[k]);
    scheme[i] = '\0';
    const char *rest = url + i + 1;
    if (rest[0] != '/' || rest[1] != '/') { free(scheme); free(work); return dupstr(""); }
    rest += 2;

    /* netloc runs to the first "/", "?" or "#" */
    size_t nlen = 0;
    while (rest[nlen] && rest[nlen] != '/' && rest[nlen] != '?' && rest[nlen] != '#') nlen++;
    char *netloc = (char *)malloc(nlen + 1);
    if (!netloc) { free(scheme); free(work); return dupstr(""); }
    memcpy(netloc, rest, nlen);
    netloc[nlen] = '\0';

    /* path: up to "?" or "#" */
    const char *p = rest + nlen;
    size_t plen = 0;
    while (p[plen] && p[plen] != '?' && p[plen] != '#') plen++;

    /* hostinfo: netloc.rsplit("@", 1)[-1] */
    char *hostinfo = strrchr(netloc, '@');
    hostinfo = hostinfo ? hostinfo + 1 : netloc;

    /* hostname + port, mirroring urlsplit._hostinfo */
    size_t hlen = 0;
    const char *port = NULL;
    size_t portlen = 0;
    if (hostinfo[0] == '[') {
        char *close = strchr(hostinfo + 1, ']');
        if (!close) { free(netloc); free(scheme); free(work); return dupstr(""); }
        hlen = (size_t)(close - (hostinfo + 1));
        char *tmp = (char *)malloc(hlen + 1);
        if (!tmp) { free(netloc); free(scheme); free(work); return dupstr(""); }
        memcpy(tmp, hostinfo + 1, hlen);
        tmp[hlen] = '\0';
        int ok = valid_ipv6(tmp);
        free(tmp);
        if (!ok) { free(netloc); free(scheme); free(work); return dupstr(""); }
        const char *after = close + 1;
        if (after[0] == ':') { port = after + 1; portlen = strlen(port); }
        else if (after[0] != '\0') { free(netloc); free(scheme); free(work); return dupstr(""); }
    } else {
        const char *colon = strchr(hostinfo, ':');
        hlen = colon ? (size_t)(colon - hostinfo) : strlen(hostinfo);
        if (colon) { port = colon + 1; portlen = strlen(port); }
    }
    if (hlen == 0) { free(netloc); free(scheme); free(work); return dupstr(""); }
    if (port && portlen > 0) {
        for (size_t k = 0; k < portlen; k++) {
            if (!isdigit((unsigned char)port[k])) {
                free(netloc); free(scheme); free(work); return dupstr("");
            }
        }
        unsigned long long pv = strtoull(port, NULL, 10);
        if (pv > 65535ULL) { free(netloc); free(scheme); free(work); return dupstr(""); }
    }

    /* urlunsplit((scheme, netloc_without_userinfo, path, "", "")) */
    size_t total = strlen(scheme) + 3 + strlen(hostinfo) + plen + 1;
    char *out = (char *)malloc(total);
    if (!out) { free(netloc); free(scheme); free(work); return dupstr(""); }
    strcpy(out, scheme);
    strcat(out, "://");
    strcat(out, hostinfo);
    size_t olen = strlen(out);
    memcpy(out + olen, p, plen);
    out[olen + plen] = '\0';
    free(netloc);
    free(scheme);
    free(work);
    return out;
}

/* ---- _resolve_session_model_settings ------------------------------------- */

json_t *w19_sessionmod_09f8f1_resolve_settings(json_t *model_cfg, const char *key,
                                               json_t *override, json_t *store_override,
                                               int has_channel, const char *channel_model,
                                               const char *channel_provider) {
    char model[1024] = "", provider[1024] = "", base_url[1024] = "";
    json_t *eff = override;
    if (!py_truthy(eff)) eff = py_truthy(store_override) ? store_override : NULL;

    /* model_cfg = cfg.get("model") or {} ; non-dict -> {"default": model_cfg} */
    json_t *mc = py_truthy(model_cfg) ? model_cfg : NULL;
    json_t *fallback = NULL; /* holds {"default": scalar} when a non-dict leaked in */
    if (mc && mc->type != JSON_OBJECT) {
        fallback = json_object();
        json_set(fallback, "default", json_copy(mc));
        mc = fallback;
    }

    if (mc && mc->type == JSON_OBJECT) {
        json_t *d = json_obj_get(mc, "default");
        json_t *m = json_obj_get(mc, "model");
        json_t *pr = json_obj_get(mc, "provider");
        json_t *b = json_obj_get(mc, "base_url");
        /* defaults["model"] = model_cfg.get("default") or model_cfg.get("model") or "" */
        if (py_truthy(d)) py_str(d, model, sizeof(model));
        else if (py_truthy(m)) py_str(m, model, sizeof(model));
        if (py_truthy(pr)) py_str(pr, provider, sizeof(provider));
        if (py_truthy(b)) py_str(b, base_url, sizeof(base_url));
    }
    if (!provider[0]) snprintf(provider, sizeof(provider), "openrouter");

    if (has_channel) {
        if (channel_model && *channel_model) snprintf(model, sizeof(model), "%s", channel_model);
        if (channel_provider && *channel_provider) snprintf(provider, sizeof(provider), "%s", channel_provider);
    }

    const char *names[3] = {"model", "provider", "base_url"};
    char *defaults[3] = {model, provider, base_url};

    json_t *out = json_object();
    json_set(out, "session_key", json_string(key ? key : ""));
    for (int i = 0; i < 3; i++) {
        char final_s[1024];
        json_t *ov = (eff && eff->type == JSON_OBJECT) ? json_obj_get(eff, names[i]) : NULL;
        if (py_truthy(ov)) py_str(ov, final_s, sizeof(final_s));
        else snprintf(final_s, sizeof(final_s), "%s", defaults[i]);
        strip_ascii_inplace(final_s);
        json_set(out, names[i], json_string(final_s));
    }
    if (fallback) json_free(fallback);
    return out;
}

json_t *w19_sessionmod_09f8f1_settings_display(json_t *settings) {
    json_t *out = json_object();
    if (!settings || settings->type != JSON_OBJECT) return out;
    const char *base = json_get_str(settings, "base_url", "");
    char *display = w19_sessionmod_09f8f1_display_base_url(base);
    for (size_t i = 0; i < json_object_size(settings); i++) {
        const char *k = json_object_get_key_at(settings, i);
        if (!k) continue;
        if (!strcmp(k, "base_url")) json_set(out, k, json_string(display ? display : ""));
        else json_set(out, k, json_copy(json_object_get_at(settings, i)));
    }
    if (!json_obj_get(out, "base_url")) json_set(out, "base_url", json_string(display ? display : ""));
    free(display);
    return out;
}
