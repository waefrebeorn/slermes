#define _POSIX_C_SOURCE 200809L
/* loops_pure.c — Faithful port of the pure, deterministic helpers in
 * hermes_cli/loops.py (the /loop recurring-wakeup command).
 *
 * Ports the side-effect-free surface: interval parsing/formatting, loop-arg
 * parsing, the LOOP_COMPLETE marker detection, and the config-backed
 * cadence getters (modeled with an optional config JSON: NULL uses defaults,
 * exactly like Python's _loops_config() returning {} on any error).
 *
 * PoP (Python -> C):
 *   parse_interval_token       -> loops_parse_interval_token
 *   parse_loop_args            -> loops_parse_loop_args
 *   format_interval            -> loops_format_interval
 *   response_signals_complete  -> loops_response_signals_complete
 *   min_interval_seconds       -> loops_min_interval_seconds
 *   max_ticks_default          -> loops_max_ticks_default
 *   self_paced_floor_seconds   -> loops_self_paced_floor_seconds
 *   self_paced_ceiling_seconds -> loops_self_paced_ceiling_seconds
 *
 * Callers receive freshly malloc'd strings (free with free()) or NULL.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <regex.h>

#define LOOPS_DEFAULT_MIN_INTERVAL_SECONDS   30
#define LOOPS_DEFAULT_MAX_TICKS              100
#define LOOPS_DEFAULT_SELF_PACED_FLOOR_SECONDS 60
#define LOOPS_DEFAULT_SELF_PACED_CEILING_SECONDS (15 * 60)

/* ─── parse_interval_token ──────────────────────────────────────── */
/* ^(?=\d)(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$  (case-insensitive) */
int loops_parse_interval_token(const char *token) {
    if (!token) return -1;
    while (*token && isspace((unsigned char)*token)) token++;
    /* must start with a digit */
    if (!*token || !isdigit((unsigned char)*token)) return -1;
    long h = 0, m = 0, s = 0;
    const char *p = token;
    /* parse compound units (h/m/s) in order */
    while (*p) {
        if (!isdigit((unsigned char)*p)) return -1;
        long val = 0;
        while (isdigit((unsigned char)*p)) { val = val * 10 + (*p - '0'); p++; }
        char unit = *p;
        if (unit == '\0') {
            /* trailing digits with no unit -> not an interval */
            return -1;
        }
        if (unit == 'h' || unit == 'H') h = val;
        else if (unit == 'm' || unit == 'M') m = val;
        else if (unit == 's' || unit == 'S') s = val;
        else return -1;
        p++;
        while (*p && isspace((unsigned char)*p)) p++;
    }
    long total = h * 3600 + m * 60 + s;
    return (total > 0) ? (int)total : -1;
}

/* ─── format_interval ───────────────────────────────────────────── */
char *loops_format_interval(double seconds) {
    if (seconds < 0) seconds = 0;
    long secs = (long)(seconds + 0.5);
    long h = secs / 3600;
    long rem = secs % 3600;
    long mm = rem / 60;
    long ss = rem % 60;
    char buf[64];
    buf[0] = '\0';
    if (h) { char t[16]; snprintf(t, sizeof(t), "%ldh", h); strcat(buf, t); }
    if (mm) { char t[16]; snprintf(t, sizeof(t), "%ldm", mm); strcat(buf, t); }
    if (ss || buf[0] == '\0') { char t[16]; snprintf(t, sizeof(t), "%lds", ss); strcat(buf, t); }
    return strdup(buf);
}

/* ─── response_signals_complete ─────────────────────────────────── */
/* (?im)^\s*LOOP_COMPLETE\s*[.!]?\s*$ */
int loops_response_signals_complete(const char *response) {
    if (!response) return 0;
    /* case-insensitive, multiline: a line that is only LOOP_COMPLETE */
    regex_t re;
    if (regcomp(&re, "(?i)^[[:space:]]*LOOP_COMPLETE[[:space:]]*[.!]?[[:space:]]*$", REG_EXTENDED | REG_NEWLINE | REG_ICASE) != 0)
        return 0;
    int found = (regexec(&re, response, 0, NULL, 0) == 0);
    regfree(&re);
    return found;
}

/* ─── config helpers (optional config JSON: NULL -> defaults) ───── */
static const char *cfg_str(json_t *cfg, const char *key) {
    json_t *v = json_object_get(cfg, key);
    if (v && v->type == JSON_STRING) return v->str_val;
    return NULL;
}
static long cfg_int(json_t *cfg, const char *key, long dflt) {
    json_t *v = json_object_get(cfg, key);
    if (!v) return dflt;
    if (v->type == JSON_NUMBER) return (long)v->num_val;
    if (v->type == JSON_STRING) {
        char *end; long r = strtol(v->str_val, &end, 10);
        if (*end == '\0') return r;
    }
    return dflt;
}

int loops_min_interval_seconds(const char *config_json) {
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    json_t *loops = cfg ? json_object_get(cfg, "loops") : NULL;
    if (!loops || loops->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return LOOPS_DEFAULT_MIN_INTERVAL_SECONDS;
    }
    long v = cfg_int(loops, "min_interval_seconds", LOOPS_DEFAULT_MIN_INTERVAL_SECONDS);
    if (v < 5) v = 5;
    if (cfg) json_free(cfg);
    return (int)v;
}
int loops_max_ticks_default(const char *config_json) {
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    json_t *loops = cfg ? json_object_get(cfg, "loops") : NULL;
    if (!loops || loops->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return LOOPS_DEFAULT_MAX_TICKS;
    }
    long v = cfg_int(loops, "max_ticks", LOOPS_DEFAULT_MAX_TICKS);
    if (v < 0) v = 0;
    if (cfg) json_free(cfg);
    return (int)v;
}
int loops_self_paced_floor_seconds(const char *config_json) {
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    json_t *loops = cfg ? json_object_get(cfg, "loops") : NULL;
    if (!loops || loops->type != JSON_OBJECT) {
        if (cfg) json_free(cfg);
        return LOOPS_DEFAULT_SELF_PACED_FLOOR_SECONDS;
    }
    long v = cfg_int(loops, "self_paced_floor_seconds", LOOPS_DEFAULT_SELF_PACED_FLOOR_SECONDS);
    if (v < 10) v = 10;
    if (cfg) json_free(cfg);
    return (int)v;
}
int loops_self_paced_ceiling_seconds(const char *config_json) {
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    json_t *loops = cfg ? json_object_get(cfg, "loops") : NULL;
    int floor;
    if (!loops || loops->type != JSON_OBJECT) {
        floor = LOOPS_DEFAULT_SELF_PACED_FLOOR_SECONDS;
        if (cfg) json_free(cfg);
        return LOOPS_DEFAULT_SELF_PACED_CEILING_SECONDS;
    }
    floor = loops_self_paced_floor_seconds(config_json);
    long v = cfg_int(loops, "self_paced_ceiling_seconds", LOOPS_DEFAULT_SELF_PACED_CEILING_SECONDS);
    if (v < floor) v = floor;
    if (cfg) json_free(cfg);
    return (int)v;
}

/* JSON-string escape for embedding into a serialized object. */
static void json_escape(const char *s, char *out, size_t outsz) {
    size_t o = 0;
    out[0] = '\0';
    if (!s) return;
    for (size_t i = 0; s[i] && o + 1 < outsz; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  if (o+2 < outsz){out[o++]='\\';out[o++]='"';} break;
            case '\\': if (o+2 < outsz){out[o++]='\\';out[o++]='\\';} break;
            case '\n': if (o+2 < outsz){out[o++]='\\';out[o++]='n';} break;
            case '\r': if (o+2 < outsz){out[o++]='\\';out[o++]='r';} break;
            case '\t': if (o+2 < outsz){out[o++]='\\';out[o++]='t';} break;
            default:
                if (c < 0x20) {
                    if (o+6 < outsz) { snprintf(out+o, outsz-o, "\\u%04x", c); o += 6; }
                } else {
                    out[o++] = (char)c;
                }
        }
    }
    out[o] = '\0';
}

/* ─── parse_loop_args ───────────────────────────────────────────── */
/* Returns a JSON object string:
 *   {"interval_seconds": N|null, "prompt": "...", "times": N,
 *    "until": "...", "error": "..."|null} */
char *loops_parse_loop_args(const char *text) {
    if (!text) text = "";
    while (*text && isspace((unsigned char)*text)) text++;
    char *raw_dup = strdup(text);
    char *raw = raw_dup;
    /* strip trailing whitespace */
    size_t rl = strlen(raw);
    while (rl > 0 && isspace((unsigned char)raw[rl - 1])) raw[--rl] = '\0';

    long times = 0;
    char until[4096]; until[0] = '\0';

    /* --times N */
    char *times_re = NULL;
    {
        regex_t rt;
        if (regcomp(&rt, "[[:space:]]--times[[:space:]]+([^[:space:]]+)", REG_EXTENDED) == 0) {
            regmatch_t m[2];
            if (regexec(&rt, raw, 2, m, 0) == 0) {
                size_t len = (size_t)(m[1].rm_eo - m[1].rm_so);
                char *tv = (char *)malloc(len + 1);
                memcpy(tv, raw + m[1].rm_so, len); tv[len] = '\0';
                char *end; long tvv = strtol(tv, &end, 10);
                if (*end != '\0' || tvv < 1) {
                    free(tv);
                    regfree(&rt);
                    /* error */
                    char *out = strdup("{\"interval_seconds\":null,\"prompt\":\"\",\"times\":0,\"until\":\"\",\"error\":\"--times expects a positive integer\"}");
                    free(raw_dup);
                    return out;
                }
                times = tvv;
                free(tv);
                /* remove match from raw */
                size_t pre = (size_t)m[0].rm_so;
                size_t post = (size_t)m[0].rm_eo;
                size_t newlen = pre + (strlen(raw) - post) + 1;
                char *nr = (char *)malloc(newlen);
                memcpy(nr, raw, pre);
                strcpy(nr + pre, raw + post);
                size_t n2 = strlen(nr);
                while (n2 > 0 && isspace((unsigned char)nr[n2 - 1])) nr[--n2] = '\0';
                nr[n2] = '\0';
                free(raw);
                raw = nr;
                times_re = raw; /* mark */
            }
            regfree(&rt);
        }
    }
    (void)times_re;

    /* --until ... (to end of line) */
    {
        regex_t ru;
        if (regcomp(&ru, "[[:space:]]--until[[:space:]]+(.+)$", REG_EXTENDED | REG_NEWLINE) == 0) {
            regmatch_t m[2];
            if (regexec(&ru, raw, 2, m, 0) == 0) {
                size_t len = (size_t)(m[1].rm_eo - m[1].rm_so);
                size_t cap = sizeof(until);
                if (len >= cap) len = cap - 1;
                memcpy(until, raw + m[1].rm_so, len); until[len] = '\0';
                size_t u2 = strlen(until);
                while (u2 > 0 && isspace((unsigned char)until[u2 - 1])) until[--u2] = '\0';
                size_t pre = (size_t)m[0].rm_so;
                size_t post = (size_t)m[0].rm_eo;
                size_t newlen = pre + (strlen(raw) - post) + 1;
                char *nr = (char *)malloc(newlen);
                memcpy(nr, raw, pre);
                strcpy(nr + pre, raw + post);
                size_t n2 = strlen(nr);
                while (n2 > 0 && isspace((unsigned char)nr[n2 - 1])) nr[--n2] = '\0';
                nr[n2] = '\0';
                free(raw);
                raw = nr;
            }
            regfree(&ru);
        }
    }

    /* leading "every" sugar */
    {
        /* tokens = raw.split(None, 1); if tokens[0].lower()=='every' */
        size_t sp = 0;
        while (raw[sp] && !isspace((unsigned char)raw[sp])) sp++;
        size_t wlen = sp;
        int is_every = 0;
        if (wlen == 5 && strncasecmp(raw, "every", 5) == 0) {
            /* require a space after */
            if (raw[wlen] && isspace((unsigned char)raw[wlen])) is_every = 1;
        }
        if (is_every) {
            char *rest = raw + wlen;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            char *nr = strdup(rest);
            free(raw);
            raw = nr;
        }
    }

    /* leading interval token */
    long interval = -1;
    char *prompt = strdup("");
    {
        size_t sp = 0;
        while (raw[sp] && !isspace((unsigned char)raw[sp])) sp++;
        char *first = (char *)malloc(sp + 1);
        memcpy(first, raw, sp); first[sp] = '\0';
        int maybe = loops_parse_interval_token(first);
        if (maybe >= 0) {
            interval = maybe;
            char *rest = raw + sp;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            free(prompt);
            prompt = strdup(rest);
        }
        free(first);
    }

    const char *err = "null";
    if (prompt[0] == '\0') {
        err = "\"missing prompt (usage: /loop [interval] <prompt>)\"";
    }

    /* build JSON (escape prompt/until) */
    char pesc[8192]; json_escape(prompt, pesc, sizeof(pesc));
    char uesc[8192]; json_escape(until, uesc, sizeof(uesc));
    size_t cap = strlen(pesc) + strlen(uesc) + 256;
    char *out = (char *)malloc(cap);
    char iv[32];
    if (interval >= 0) snprintf(iv, sizeof(iv), "%ld", interval);
    else strcpy(iv, "null");
    snprintf(out, cap,
        "{\"interval_seconds\":%s,\"prompt\":\"%s\",\"times\":%ld,\"until\":\"%s\",\"error\":%s}",
        iv, pesc, times, uesc, err);
    free(prompt);
    free(raw_dup);
    return out;
}
