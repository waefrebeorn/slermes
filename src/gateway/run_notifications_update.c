/*
 * run_notifications_update.c - C home for gateway/run_notifications_update.py.
 * Faithful port of the pure update-watcher helpers: log tails, marker profile/age,
 * byte-offset output reads, marker paths and the exit-code read. The watcher loop
 * itself is asyncio + adapter I/O and stays in Python.
 * PoP: w19_runnotific_6abf1f_output_tail @ gateway/run_notifications_update.py:_update_output_tail
 * PoP: w19_runnotific_6abf1f_marker_profile @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._marker_profile
 * PoP: w19_runnotific_6abf1f_marker_age @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._marker_age_seconds
 * PoP: w19_runnotific_6abf1f_read_since @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._read_update_output_since
 * PoP: w19_runnotific_6abf1f_update_paths @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._update_paths
 * PoP: w19_runnotific_6abf1f_update_exit_code @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._update_exit_code
 */
#include "noc_w19_runnotific_6abf1f.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELLIPSIS "\xE2\x80\xA6"      /* U+2026, what Python writes before a cut tail */
#define REPLACEMENT "\xEF\xBF\xBD"   /* U+FFFD, decode(errors="replace") */

static char *dupstr(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s) + 1;
    p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

static int py_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *py_strip_dup(const char *s) {
    const char *a;
    const char *b;
    size_t n;
    char *out;
    if (!s) return dupstr("");
    a = s;
    while (*a && py_ws((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && py_ws((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* Number of UTF-8 characters (bytes that are not continuation bytes). */
static size_t utf8_chars(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    for (; p && *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset of character index `idx` (or strlen when idx >= char count). */
static size_t utf8_byte_at(const char *s, size_t idx) {
    size_t chars = 0;
    size_t i = 0;
    for (; s[i]; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            if (chars == idx) return i;
            chars++;
        }
    }
    return i;
}

/* ---- _update_output_tail(output, limit) ---- */
json_t *w19_runnotific_6abf1f_output_tail(const char *output, long long limit) {
    const char *s = output ? output : "";
    size_t n = utf8_chars(s);
    size_t start_char, start_byte, len;
    char *buf;
    json_t *node;
    if ((long long)n <= limit) return json_string(s);
    if (limit > 0) {
        start_char = ((size_t)limit > n) ? 0 : n - (size_t)limit;
    } else if (limit == 0) {
        start_char = 0;  /* output[-0:] is output[0:]: the whole string */
    } else {
        start_char = (size_t)(-limit);
        if (start_char > n) start_char = n;
    }
    start_byte = utf8_byte_at(s, start_char);
    len = strlen(s) - start_byte;
    buf = (char *)malloc(strlen(ELLIPSIS) + len + 1);
    if (!buf) return json_null();
    strcpy(buf, ELLIPSIS);
    memcpy(buf + strlen(ELLIPSIS), s + start_byte, len);
    buf[strlen(ELLIPSIS) + len] = '\0';
    node = json_string(buf);
    free(buf);
    return node;
}

/* gateway.session.profile_from_session_key_namespace */
static const char *pfskn(const char *ns) {
    if (!ns) return "default";
    if (!strcmp(ns, "main")) return "default";
    if (!strcmp(ns, "main~")) return "main";
    return ns;
}

/* ---- _marker_profile(data) ---- */
json_t *w19_runnotific_6abf1f_marker_profile(json_t *data) {
    json_t *v;
    char *profile;
    if (!data || data->type != JSON_OBJECT) return json_null();
    v = json_obj_get(data, "profile");
    if (v && v->type == JSON_STRING) {
        profile = py_strip_dup(v->str_val);
    } else if (v && v->type == JSON_NUMBER) {
        char num[64];
        snprintf(num, sizeof(num), "%g", v->num_val);
        profile = py_strip_dup(num);
    } else {
        profile = dupstr("");
    }
    if (profile && profile[0]) {
        json_t *node = json_string(profile);
        free(profile);
        return node;
    }
    free(profile);

    v = json_obj_get(data, "session_key");
    if (v && v->type == JSON_STRING) {
        /* split(":") - need >= 5 parts with an agent lane that is not main/empty. */
        const char *s = v->str_val;
        const char *p = s;
        const char *parts[6];
        size_t lens[6];
        size_t count = 0;
        while (1) {
            const char *colon = strchr(p, ':');
            size_t len = colon ? (size_t)(colon - p) : strlen(p);
            if (count < 6) { parts[count] = p; lens[count] = len; }
            count++;
            if (!colon || count >= 6) break;
            p = colon + 1;
        }
        if (count >= 5 && lens[0] == 5 && !strncmp(parts[0], "agent", 5)) {
            /* parts[1] not in ("main", "") */
            int is_main = (lens[1] == 4 && !strncmp(parts[1], "main", 4));
            int is_empty = (lens[1] == 0);
            if (!is_main && !is_empty) {
                char *ns = (char *)malloc(lens[1] + 1);
                json_t *node;
                if (!ns) return json_null();
                memcpy(ns, parts[1], lens[1]);
                ns[lens[1]] = '\0';
                node = json_string(pfskn(ns));
                free(ns);
                return node;
            }
        }
    }
    return json_null();
}

/* days since 1970-01-01 for a proleptic Gregorian date */
static long long days_from_civil(long long y, int m, int d) {
    long long era, yoe, doy, doe;
    y -= (m <= 2);
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153LL * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

/* Parse an ISO-8601 stamp into microseconds since the epoch. 1 on success. */
static int iso_parse_us(const char *s, long long *out_us, int *aware) {
    long long y = 0, day = 0, hour = 0, minute = 0, second = 0, frac_us = 0;
    long long off = 0;
    int digits, i = 0;
    *aware = 0;
    while (s[i] && py_ws((unsigned char)s[i])) i++;
    for (digits = 0; digits < 4; digits++, i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        y = y * 10 + (s[i] - '0');
    }
    if (s[i] != '-') return 0;
    i++;
    for (digits = 0; digits < 2; digits++, i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        day = day * 10 + (s[i] - '0');
    }
    if (s[i] != '-') return 0;
    i++;
    {
        long long d = 0;
        for (digits = 0; digits < 2; digits++, i++) {
            if (s[i] < '0' || s[i] > '9') return 0;
            d = d * 10 + (s[i] - '0');
        }
        day = day * 100 + d;  /* MMDD */
    }
    if (s[i] != 'T' && s[i] != 't' && s[i] != ' ') return 0;
    i++;
    for (digits = 0; digits < 2; digits++, i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        hour = hour * 10 + (s[i] - '0');
    }
    if (s[i] != ':') return 0;
    i++;
    for (digits = 0; digits < 2; digits++, i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        minute = minute * 10 + (s[i] - '0');
    }
    if (s[i] == ':') {
        i++;
        for (digits = 0; digits < 2; digits++, i++) {
            if (s[i] < '0' || s[i] > '9') return 0;
            second = second * 10 + (s[i] - '0');
        }
        if (s[i] == '.' || s[i] == ',') {
            long long scale = 100000;
            i++;
            if (s[i] < '0' || s[i] > '9') return 0;
            while (s[i] >= '0' && s[i] <= '9') {
                if (scale > 0) frac_us += (s[i] - '0') * scale;
                scale /= 10;
                i++;
            }
        }
    }
    if (s[i] == 'Z' || s[i] == 'z') {
        *aware = 1;
        i++;
    } else if (s[i] == '+' || s[i] == '-') {
        int sign = (s[i] == '-') ? -1 : 1;
        long long oh = 0, om = 0;
        i++;
        for (digits = 0; digits < 2; digits++, i++) {
            if (s[i] < '0' || s[i] > '9') return 0;
            oh = oh * 10 + (s[i] - '0');
        }
        if (s[i] == ':') i++;
        if (s[i] >= '0' && s[i] <= '9') {
            for (digits = 0; digits < 2; digits++, i++) {
                if (s[i] < '0' || s[i] > '9') return 0;
                om = om * 10 + (s[i] - '0');
            }
        }
        off = sign * (oh * 3600 + om * 60);
        *aware = 1;
    }
    while (s[i] && py_ws((unsigned char)s[i])) i++;
    if (s[i] != '\0') return 0;
    *out_us = ((days_from_civil(y, (int)(day / 100), (int)(day % 100)) * 86400LL
                + hour * 3600 + minute * 60 + second) * 1000000LL + frac_us) - off * 1000000LL;
    return 1;
}

/* ---- _marker_age_seconds(data) ---- */
json_t *w19_runnotific_6abf1f_marker_age(json_t *data, const char *now_iso) {
    json_t *v;
    char *raw;
    long long stamp_us, now_us;
    int aware_stamp, aware_now;
    if (!data || data->type != JSON_OBJECT) return json_null();
    v = json_obj_get(data, "timestamp");
    if (!v || v->type != JSON_STRING) return json_null();
    raw = py_strip_dup(v->str_val);
    if (!raw || !raw[0]) { free(raw); return json_null(); }
    if (!iso_parse_us(raw, &stamp_us, &aware_stamp)) { free(raw); return json_null(); }
    free(raw);
    if (!now_iso || !*now_iso) return json_null();
    if (!iso_parse_us(now_iso, &now_us, &aware_now)) return json_null();
    {
        long long diff_us = now_us - stamp_us;
        return json_number((double)diff_us / 1000000.0);
    }
}

/* Decode UTF-8 with errors="replace": invalid sequences become U+FFFD. */
static char *utf8_replace(const char *data, size_t len) {
    char *out = (char *)malloc(len * 3 + 1);
    size_t i = 0, o = 0;
    if (!out) return NULL;
    while (i < len) {
        unsigned char c = (unsigned char)data[i];
        size_t need;
        if (c < 0x80) { out[o++] = (char)c; i++; continue; }
        if ((c & 0xE0) == 0xC0) need = 1;
        else if ((c & 0xF0) == 0xE0) need = 2;
        else if ((c & 0xF8) == 0xF0) need = 3;
        else {
            memcpy(out + o, REPLACEMENT, 3);
            o += 3;
            i++;
            continue;
        }
        if (i + need >= len) {
            memcpy(out + o, REPLACEMENT, 3);
            o += 3;
            i++;
            continue;
        }
        {
            size_t k;
            int ok = 1;
            for (k = 1; k <= need; k++)
                if (((unsigned char)data[i + k] & 0xC0) != 0x80) { ok = 0; break; }
            if (!ok) {
                memcpy(out + o, REPLACEMENT, 3);
                o += 3;
                i++;
                continue;
            }
            memcpy(out + o, data + i, need + 1);
            o += need + 1;
            i += need + 1;
        }
    }
    out[o] = '\0';
    return out;
}

/* ---- _read_update_output_since(path, offset) ---- */
json_t *w19_runnotific_6abf1f_read_since(const char *data, long long offset) {
    const char *s = data ? data : "";
    size_t len = strlen(s);
    json_t *out = json_object();
    if (offset < 0) offset = 0;
    if ((unsigned long long)offset >= (unsigned long long)len) {
        json_set(out, "text", json_string(""));
        json_set(out, "offset", json_number((double)len));
        return out;
    }
    {
        char *decoded = utf8_replace(s + offset, len - (size_t)offset);
        json_set(out, "text", decoded ? json_string(decoded) : json_string(""));
        free(decoded);
    }
    json_set(out, "offset", json_number((double)len));
    return out;
}

/* str(Path(home) / name) as Windows renders it: backslash separators, no trailing one. */
static char *win_path_join(const char *home, const char *name) {
    char *norm;
    size_t i, o = 0, hl;
    char *out;
    if (!home || !home[0] || !strcmp(home, ".")) return dupstr(name);
    hl = strlen(home);
    norm = (char *)malloc(hl + 1);
    if (!norm) return dupstr(name);
    for (i = 0; i < hl; i++) {
        char c = home[i];
        if (c == '/') c = '\\';
        if (c == '\\' && o > 0 && norm[o - 1] == '\\') continue;
        norm[o++] = c;
    }
    while (o > 0 && norm[o - 1] == '\\') o--;
    norm[o] = '\0';
    if (!norm[0]) { free(norm); return dupstr(name); }
    out = (char *)malloc(o + strlen(name) + 2);
    if (!out) { free(norm); return dupstr(name); }
    memcpy(out, norm, o);
    out[o] = '\\';
    strcpy(out + o + 1, name);
    free(norm);
    return out;
}

/* ---- _update_paths() ---- */
json_t *w19_runnotific_6abf1f_update_paths(const char *hermes_home) {
    static const char *const names[] = {"pending", "claimed", "output", "exit_code",
                                        "prompt", "response"};
    static const char *const files[] = {".update_pending.json", ".update_pending.claimed.json",
                                        ".update_output.txt", ".update_exit_code",
                                        ".update_prompt.json", ".update_response"};
    json_t *out = json_object();
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char *p = win_path_join(hermes_home ? hermes_home : "", files[i]);
        json_set(out, names[i], json_string(p ? p : ""));
        free(p);
    }
    return out;
}

/* ---- _update_exit_code(paths) ---- */
json_t *w19_runnotific_6abf1f_update_exit_code(const char *text) {
    char *trimmed = py_strip_dup(text ? text : "");
    const char *s;
    long long acc = 0;
    int neg = 0, digits = 0;
    json_t *node;
    if (!trimmed) return json_null();
    s = trimmed[0] ? trimmed : "1";
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    while (*s >= '0' && *s <= '9') { acc = acc * 10 + (*s - '0'); digits++; s++; }
    if (!digits || *s != '\0') { free(trimmed); return json_null(); }
    node = json_number((double)(neg ? -acc : acc));
    free(trimmed);
    return node;
}
