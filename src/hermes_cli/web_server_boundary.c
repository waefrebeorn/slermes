/*
 * web_server_boundary.c — C port of upstream Hermes hermes_cli/web_server_boundary.py.
 *
 * PoP: w18_webserver_e8a789_public_url_hosts @ hermes_cli/web_server_boundary.py:public_url_hosts
 * PoP: w18_webserver_e8a789_host_header_hostname @ hermes_cli/web_server_boundary.py:_host_header_hostname
 * PoP: w18_webserver_e8a789_is_accepted_host @ hermes_cli/web_server_boundary.py:_is_accepted_host
 * PoP: w18_webserver_e8a789_should_require_auth @ hermes_cli/web_server_boundary.py:should_require_auth
 * PoP: w18_webserver_e8a789_public_hosts_engage_gate @ hermes_cli/web_server_boundary.py:public_hosts_engage_gate
 * PoP: w18_webserver_e8a789_loopback_origin @ hermes_cli/web_server_boundary.py:_loopback_origin
 * PoP: w18_webserver_e8a789_public_url_origins @ hermes_cli/web_server_boundary.py:public_url_origins
 */
#include "noc_w18_webserver_e8a789.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python _LOOPBACK_HOST_VALUES = {"localhost", "127.0.0.1", "::1"}. */
static int is_loopback_value(const char *h) {
    return h && (!strcmp(h, "localhost") || !strcmp(h, "127.0.0.1") || !strcmp(h, "::1"));
}

static void lower_ascii(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

/* Mimic urllib.parse.urlparse(url).hostname / .port for http(s) URLs.
 * Returns 1 when an authority was parsed. *port_ok is 0 when accessing
 * .port would raise ValueError, *has_port is 0 when there was no port. */
static int parse_authority(const char *url, char *scheme, size_t schemecap,
                           char *host, size_t hostcap, int *port, int *has_port, int *port_ok) {
    *has_port = 0; *port_ok = 1; *port = -1;
    if (scheme && schemecap) scheme[0] = '\0';
    if (host && hostcap) host[0] = '\0';
    if (!url) return 0;
    const char *sep = strstr(url, "://");
    if (!sep || sep == url) return 0;              /* no scheme-based authority */
    if (!isalpha((unsigned char)url[0])) return 0;  /* invalid scheme */
    size_t sl = (size_t)(sep - url);
    if (sl && sl < schemecap) {
        memcpy(scheme, url, sl);
        scheme[sl] = '\0';
        lower_ascii(scheme);
    }
    const char *a = sep + 3;
    const char *end = a + strcspn(a, "/?#");
    size_t alen = (size_t)(end - a);
    if (alen == 0 || alen >= 2048) return 0;
    char auth[2048];
    memcpy(auth, a, alen);
    auth[alen] = '\0';
    char *hp = auth;
    char *at = strrchr(auth, '@');
    if (at) hp = at + 1;
    const char *hostpart = hp;
    const char *portstr = NULL;
    if (*hostpart == '[') {
        const char *close = strchr(hostpart, ']');
        if (!close) return 0;                       /* urlparse raises ValueError */
        size_t hl = (size_t)(close - hostpart - 1);
        if (hl == 0 || hl >= hostcap) return 0;
        memcpy(host, hostpart + 1, hl);
        host[hl] = '\0';
        const char *suffix = close + 1;
        if (*suffix == ':') portstr = suffix + 1;
    } else {
        int colons = 0;
        for (const char *p = hostpart; *p; p++) if (*p == ':') colons++;
        if (colons > 1) return 0;
        const char *colon = strchr(hostpart, ':');
        if (colon) {
            size_t hl = (size_t)(colon - hostpart);
            if (hl == 0 || hl >= hostcap) return 0;
            memcpy(host, hostpart, hl);
            host[hl] = '\0';
            portstr = colon + 1;
        } else {
            size_t hl = strlen(hostpart);
            if (hl == 0 || hl >= hostcap) return 0;
            memcpy(host, hostpart, hl);
            host[hl] = '\0';
        }
    }
    lower_ascii(host);
    if (portstr) {
        *has_port = 1;
        if (!all_digits(portstr) || strlen(portstr) > 7) {
            *port_ok = 0;
        } else {
            long v = strtol(portstr, NULL, 10);
            if (v < 0 || v > 65535) *port_ok = 0;
            else *port = (int)v;
        }
    }
    return 1;
}

json_t *w18_webserver_e8a789_public_url_hosts(const char *public_url) {
    json_t *out = json_array();
    if (!public_url || !*public_url) return out;
    char scheme[32], host[512];
    int port = -1, has_port = 0, port_ok = 1;
    if (!parse_authority(public_url, scheme, sizeof(scheme), host, sizeof(host),
                         &port, &has_port, &port_ok))
        return out;
    (void)scheme; (void)port; (void)has_port; (void)port_ok; /* .hostname only */
    if (*host) json_append(out, json_string(host));
    return out;
}

/* Static scratch for the hostname result; caller copies immediately. */
static char hh_buf[1024];
const char *w18_webserver_e8a789_host_header_hostname(const char *host_header) {
    hh_buf[0] = '\0';
    const char *value = host_header ? host_header : "";
    while (*value && isspace((unsigned char)*value)) value++;
    const char *e = value + strlen(value);
    while (e > value && isspace((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - value);
    if (n == 0) return hh_buf;
    char v[2048];
    if (n >= sizeof(v)) return hh_buf;
    memcpy(v, value, n);
    v[n] = '\0';
    if (strstr(v, "://")) return hh_buf;
    if (strpbrk(v, "\"'<> \n\r\t/?#@")) return hh_buf;
    if (v[0] == '[') {
        char *close = strchr(v, ']');
        if (!close) return hh_buf;
        size_t hl = (size_t)(close - v - 1);
        if (hl == 0 || hl >= sizeof(hh_buf)) return hh_buf;
        memcpy(hh_buf, v + 1, hl);
        hh_buf[hl] = '\0';
        if (!strchr(hh_buf, ':')) { hh_buf[0] = '\0'; return hh_buf; }
        const char *suffix = close + 1;
        if (*suffix) {
            if (*suffix != ':' || !all_digits(suffix + 1)) { hh_buf[0] = '\0'; return hh_buf; }
        }
        lower_ascii(hh_buf);
        return hh_buf;
    }
    int colons = 0;
    for (const char *p = v; *p; p++) if (*p == ':') colons++;
    if (colons > 1) return hh_buf;
    if (colons == 1) {
        char *colon = strchr(v, ':');
        size_t hl = (size_t)(colon - v);
        if (hl == 0 || !all_digits(colon + 1) || hl >= sizeof(hh_buf)) return hh_buf;
        memcpy(hh_buf, v, hl);
        hh_buf[hl] = '\0';
        lower_ascii(hh_buf);
        return hh_buf;
    }
    if (strlen(v) >= sizeof(hh_buf)) return hh_buf;
    strcpy(hh_buf, v);
    lower_ascii(hh_buf);
    return hh_buf;
}

static int trusted_has(const json_t *arr, const char *h) {
    if (!json_is_array(arr) || !h) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && !strcmp(e->str_val, h)) return 1;
    }
    return 0;
}

int w18_webserver_e8a789_is_accepted_host(const char *host_header, const char *bound_host,
                                          const json_t *trusted_public_hosts) {
    const char *host_only = w18_webserver_e8a789_host_header_hostname(host_header);
    if (!host_only || !*host_only) return 0;
    if (trusted_has(trusted_public_hosts, host_only)) return 1;
    const char *bh = bound_host ? bound_host : "";
    if (!strcmp(bh, "0.0.0.0") || !strcmp(bh, "::")) return 1;
    char bound_lc[512];
    if (strlen(bh) >= sizeof(bound_lc)) return 0;
    strcpy(bound_lc, bh);
    lower_ascii(bound_lc);
    if (is_loopback_value(bound_lc)) return is_loopback_value(host_only);
    return !strcmp(host_only, bound_lc);
}

int w18_webserver_e8a789_should_require_auth(const char *host, int allow_public) {
    (void)allow_public; /* deliberately ignored */
    return !is_loopback_value(host);
}

int w18_webserver_e8a789_public_hosts_engage_gate(const json_t *trusted_public_hosts) {
    if (!json_is_array(trusted_public_hosts)) return 0;
    for (size_t i = 0; i < json_len(trusted_public_hosts); i++) {
        json_t *e = json_get(trusted_public_hosts, i);
        if (e && e->type == JSON_STRING && !is_loopback_value(e->str_val)) return 1;
    }
    return 0;
}

json_t *w18_webserver_e8a789_loopback_origin(const char *origin) {
    char scheme[32], host[512];
    int port = -1, has_port = 0, port_ok = 1;
    if (!parse_authority(origin, scheme, sizeof(scheme), host, sizeof(host),
                         &port, &has_port, &port_ok))
        return json_null();
    if (strcmp(scheme, "http") && strcmp(scheme, "https")) return json_null();
    if (!is_loopback_value(host)) return json_null();
    if (has_port && !port_ok) return json_null();
    int final_port = port;
    if (!has_port) final_port = !strcmp(scheme, "https") ? 443 : 80;
    json_t *out = json_array();
    json_append(out, json_string(scheme));
    json_append(out, json_string(host));
    json_append(out, json_number((double)final_port));
    return out;
}

json_t *w18_webserver_e8a789_public_url_origins(const char *public_url) {
    json_t *out = json_array();
    if (!public_url || !*public_url) return out;
    json_t *o = w18_webserver_e8a789_loopback_origin(public_url);
    if (o && o->type == JSON_ARRAY) json_append(out, o);
    else json_free(o);
    return out;
}
