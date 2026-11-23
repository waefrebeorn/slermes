/*
 * plugin_oauth.c — C port of upstream Hermes hermes_cli/plugin_oauth.py.
 *
 * PoP: w9_pluginoauth_2f3a26_valid_id @ hermes_cli/plugin_oauth.py:_ID_RE (^[a-z0-9][a-z0-9-]*$)
 * PoP: w9_pluginoauth_2f3a26_valid_env @ hermes_cli/plugin_oauth.py:_ENV_RE (^[A-Z][A-Z0-9_]*$)
 * PoP: w9_pluginoauth_2f3a26_valid_host @ hermes_cli/plugin_oauth.py:_HOST_RE
 * PoP: w9_pluginoauth_2f3a26_parent @ hermes_cli/plugin_oauth.py:_parent
 *
 * Faithful ports of the manifest validators and the registered-domain
 * approximation used by the consent warnings.
 */
#include "noc_w9_pluginoauth_2f3a26.h"
#include "hermes_json.h"
#include <stdio.h>
#include <string.h>

static int lower_alpha(unsigned char c) { return c >= 'a' && c <= 'z'; }
static int upper_alpha(unsigned char c) { return c >= 'A' && c <= 'Z'; }
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static int lower_alnum(unsigned char c) { return lower_alpha(c) || digit(c); }

int w9_pluginoauth_2f3a26_valid_id(const char *value) {
    if (!value || !value[0]) return 0;
    if (!lower_alnum((unsigned char)value[0])) return 0;
    for (const char *p = value + 1; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!lower_alnum(c) && c != '-') return 0;
    }
    return 1;
}

int w9_pluginoauth_2f3a26_valid_env(const char *value) {
    if (!value || !value[0]) return 0;
    if (!upper_alpha((unsigned char)value[0])) return 0;
    for (const char *p = value + 1; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!upper_alpha(c) && !digit(c) && c != '_') return 0;
    }
    return 1;
}

/* ^(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\.)+[a-z](?:[a-z0-9-]*[a-z0-9])?$ */
int w9_pluginoauth_2f3a26_valid_host(const char *value) {
    if (!value || !value[0]) return 0;
    const char *p = value;
    int labels = 0;
    for (;;) {
        const char *dot = strchr(p, '.');
        size_t len = dot ? (size_t)(dot - p) : strlen(p);
        if (len == 0) return 0;
        int last = !dot;
        unsigned char c0 = (unsigned char)p[0];
        if (last) {
            if (!lower_alpha(c0)) return 0; /* final label starts with a letter */
        } else if (!lower_alnum(c0)) {
            return 0;
        }
        if (len >= 2) {
            unsigned char cl = (unsigned char)p[len - 1];
            if (!lower_alnum(cl)) return 0; /* must end alphanumeric */
            for (size_t i = 1; i + 1 < len; i++) {
                unsigned char c = (unsigned char)p[i];
                if (!lower_alnum(c) && c != '-') return 0;
            }
        }
        labels++;
        if (last) break;
        p = dot + 1;
    }
    return labels >= 2; /* at least one dot is required */
}

/* labels = host.split("."); keep = 3 if len(labels)>2 and len(labels[-1])==2
 * and len(labels[-2])<=3 else 2; return ".".join(labels[-keep:]) */
struct json_t *w9_pluginoauth_2f3a26_parent(const char *host) {
    const char *h = host ? host : "";
    const char *labels[64];
    size_t lens[64];
    size_t count = 0;
    const char *p = h;
    for (;;) {
        const char *dot = strchr(p, '.');
        if (count < 64) {
            labels[count] = p;
            lens[count] = dot ? (size_t)(dot - p) : strlen(p);
        }
        count++;
        if (!dot) break;
        p = dot + 1;
    }
    if (count > 64) count = 64;
    size_t keep = (count > 2 && lens[count - 1] == 2 && lens[count - 2] <= 3) ? 3 : 2;
    if (keep > count) keep = count;

    char out[8192];
    size_t n = 0;
    for (size_t i = count - keep; i < count; i++) {
        if (i > count - keep) { if (n + 1 < sizeof(out)) out[n++] = '.'; }
        for (size_t j = 0; j < lens[i] && n + 1 < sizeof(out); j++) out[n++] = labels[i][j];
    }
    out[n] = '\0';
    return json_string(out);
}
