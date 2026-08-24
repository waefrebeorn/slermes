#define _POSIX_C_SOURCE 200809L
/* loops_pure.c - Faithful port of the pure, deterministic helpers in
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
#include "loops_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <regex.h>

static const long LOOPS_DEFAULT_MIN_INTERVAL_SECONDS = 30;
static const long LOOPS_DEFAULT_MAX_TICKS              = 100;
static const long LOOPS_DEFAULT_SELF_PACED_FLOOR_SECONDS = 60;
static const long LOOPS_DEFAULT_SELF_PACED_CEILING_SECONDS = (15 * 60);

/* parse_interval_token -------------------------------------------------  */
/* ^(?=\d)(?:(\d+)h)?(?:(\d+)m)?(?:(\d+)s)?$  (case-insensitive)  */
/* PoP: loops_parse_interval_token @ hermes_cli/loops.py:parse_interval_token */

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

#
/* --- format_interval --------------------------------------------- */
/* PoP: loops_format_interval @ hermes_cli/loops.py:format_interval */
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

/* --- response_signals_complete -----------------------------------  */
/* (?im)^\s*LOOP_COMPLETE\s*[.!]?\s*$  */
/* PoP: loops_response_signals_complete @ hermes_cli/loops.py:response_signals_complete */

int loops_response_signals_complete(const char *response) {
    if (!response) return 0;
    /* case-insensitive, multiline: a line that is only LOOP_COMPLETE
     * (POSIX ERE has no (?i) inline flag — REG_ICASE below does it). */
    regex_t re;
    if (regcomp(&re, "^[[:space:]]*LOOP_COMPLETE[[:space:]]*[.!]?[[:space:]]*$", REG_EXTENDED | REG_NEWLINE | REG_ICASE) != 0)
        return 0;
    int found = (regexec(&re, response, 0, NULL, 0) == 0);
    regfree(&re);
    return found;
}

/* --- config helpers (optional config JSON: NULL -> defaults) ----- */
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
/* PoP: loops_min_interval_seconds @ hermes_cli/loops.py:min_interval_seconds */

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
}/* PoP: loops_max_ticks_default @ hermes_cli/loops.py:max_ticks_default */

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
}/* PoP: loops_self_paced_floor_seconds @ hermes_cli/loops.py:self_paced_floor_seconds */

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
}/* PoP: loops_self_paced_ceiling_seconds @ hermes_cli/loops.py:self_paced_ceiling_seconds */

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

/* --- parse_loop_args ---------------------------------------------  */
/* PoP: loops_parse_loop_args @ hermes_cli/loops.py:parse_loop_args */
char *loops_parse_loop_args(const char *text) {
    if (!text) text = "";
    while (*text && isspace((unsigned char)*text)) text++;
    char *raw_dup = strdup(text);
    char *raw = raw_dup;
    /* strip trailing whitespace */
    size_t rl = strlen(raw);
    while (rl > 0 && isspace((unsigned char)raw[rl - 1])) raw[--rl] = '\0';

    /* Empty input → error "empty" (matches Python's early `if not raw` check). */
    if (raw[0] == '\0') {
        char *out = strdup("{\"interval_seconds\":null,\"prompt\":\"\",\"times\":0,\"until\":\"\",\"error\":\"empty\"}");
        free(raw_dup);
        return out;
    }

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
                    /* Python: error = f"--times expects a positive integer, got {m.group(1)!r}" */
                    char *out = (char *)malloc(strlen(tv) + 96);
                    snprintf(out, strlen(tv) + 96,
                        "{\"interval_seconds\":null,\"prompt\":\"\",\"times\":0,\"until\":\"\","
                        "\"error\":\"--times expects a positive integer, got '%s'\"}", tv);
                    free(tv);
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
        } else {
            /* No leading interval token: the whole remaining text (after any
             * "every" sugar) is the prompt, exactly like Python (which only
             * reassigns raw when maybe is not None). */
            free(prompt);
            prompt = strdup(raw);
        }
        free(first);
    }

    /* Python returns EARLY on missing prompt, with interval_seconds left null
     * (result fields are never assigned on the error path). */
    if (prompt[0] == '\0') {
        char pe[8192]; json_escape(prompt, pe, sizeof(pe));
        char ue[8192]; json_escape(until, ue, sizeof(ue));
        size_t cap2 = strlen(pe) + strlen(ue) + 256;
        char *out = (char *)malloc(cap2);
        snprintf(out, cap2,
            "{\"interval_seconds\":null,\"prompt\":\"%s\",\"times\":%ld,\"until\":\"%s\","
            "\"error\":\"missing prompt (usage: /loop [interval] <prompt>)\"}",
            pe, times, ue);
        free(prompt);
        free(raw_dup);   /* raw may alias raw_dup — free only the owner */
        return out;
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
        "{\"interval_seconds\":%s,\"prompt\":\"%s\",\"times\":%ld,\"until\":\"%s\",\"error\":null}",
        iv, pesc, times, uesc);
    free(prompt);
    free(raw);
    return out;
}

/* Configuration defaults (mirror of _loops_config() defaults)            */

/* ──────────────────────────────────────────────────────────────────────
 * LoopState: serialized /loop state (loops.py::LoopState dataclass).
 * Opaque struct; constructed/inspected only through the functions here.
 * ──────────────────────────────────────────────────────────────────── */

#include "loops_state_internal.h"

/* PoP: loops_state_from_json @ hermes_cli/loops.py:LoopState.from_json */
loop_state_t *loops_state_from_json(const char *raw) {
    if (!raw) return NULL;
    json_t *data = json_parse(raw, NULL);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return NULL;
    }
    loop_state_t *s = (loop_state_t *)calloc(1, sizeof(*s));
    if (!s) { json_free(data); return NULL; }
    s->prompt  = strdup(json_get_str(data, "prompt", ""));
    s->status  = strdup(json_get_str(data, "status", "active"));
    s->mode    = strdup(json_get_str(data, "mode", "interval"));
    s->interval_seconds = json_get_num(data, "interval_seconds", 0.0);
    s->current_delay    = json_get_num(data, "current_delay", 0.0);
    s->times    = (long)json_get_num(data, "times", 0.0);
    s->until    = strdup(json_get_str(data, "until", ""));
    s->max_ticks   = (long)json_get_num(data, "max_ticks", 100.0);
    s->ticks_fired = (long)json_get_num(data, "ticks_fired", 0.0);
    s->created_at     = json_get_num(data, "created_at", 0.0);
    s->last_fired_at  = json_get_num(data, "last_fired_at", 0.0);
    s->next_due_at    = json_get_num(data, "next_due_at", 0.0);
    s->awaiting_response = json_get_bool(data, "awaiting_response", false) ? 1 : 0;
    s->last_response_digest = strdup(json_get_str(data, "last_response_digest", ""));
    json_t *pr = json_obj_get(data, "paused_reason");
    s->paused_reason = (pr && pr->type == JSON_STRING) ? strdup(pr->str_val) : NULL;
    json_t *ls = json_obj_get(data, "last_stop_reason");
    s->last_stop_reason = (ls && ls->type == JSON_STRING) ? strdup(ls->str_val) : NULL;
    json_free(data);
    return s;
}

/* PoP: loops_state_cadence_label @ hermes_cli/loops.py:LoopState.cadence_label */
char *loops_state_cadence_label(const loop_state_t *s) {
    if (!s) return strdup("");
    if (strcmp(s->mode, "self_paced") == 0) {
        if (s->current_delay > 0) {
            char *d = loops_format_interval(s->current_delay);
            char *out = malloc(strlen(d) + 32);
            snprintf(out, strlen(d) + 32, "self-paced, currently %s", d);
            free(d);
            return out;
        }
        return strdup("self-paced");
    }
    char *iv = loops_format_interval(s->interval_seconds);
    char *out = malloc(strlen(iv) + 16);
    snprintf(out, strlen(iv) + 16, "every %s", iv);
    free(iv);
    return out;
}

/* PoP: loops_state_remaining_label @ hermes_cli/loops.py:LoopState.remaining_label */
/* now is injected (deterministic; Python uses time.time()). */
char *loops_state_remaining_label(const loop_state_t *s, double now) {
    if (!s || strcmp(s->status, "active") != 0) return strdup("");
    double remaining = s->next_due_at - now;
    if (remaining <= 0) return strdup("due now");
    char *iv = loops_format_interval(remaining);
    char *out = malloc(strlen(iv) + 16);
    snprintf(out, strlen(iv) + 16, "next in %s", iv);
    free(iv);
    return out;
}

/* PoP: loops_state_is_due @ hermes_cli/loops.py:LoopManager.is_due */
int loops_state_is_due(const loop_state_t *s, double now) {
    if (!s || strcmp(s->status, "active") != 0) return 0;
    if (s->awaiting_response) return 0;
    if (now < 0) now = 0;  /* caller injects real time; <0 guard only */
    return now >= s->next_due_at ? 1 : 0;
}

/*
 * Stable digest for self-paced change detection. Reproduces Python's exact
 * normalization (whitespace collapse, strip clock/timestamp tokens) and
 * SHA-256s the result (libcrypto).
 * PoP: loops_digest_response @ hermes_cli/loops.py:_digest_response
 */
#include <openssl/sha.h>
char *loops_digest_response(const char *response) {
    if (!response) response = "";
    char *buf = strdup(response);
    /* strip + collapse whitespace like Python's .strip().lower() + \s+ -> ' ' */
    /* normalize: lower, then remove timestamp/clock tokens */
    size_t n = strlen(buf);
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
    /* remove HH:MM(:SS) */
    {
        regex_t re;
        if (regcomp(&re, "[0-9]{1,2}:[0-9]{2}(:[0-9]{2})?", REG_EXTENDED) == 0) {
            regmatch_t m;
            while (regexec(&re, buf, 1, &m, 0) == 0) {
                size_t pre = (size_t)m.rm_so;
                size_t post = (size_t)m.rm_eo;
                memmove(buf + pre, buf + post, strlen(buf + post) + 1);
            }
            regfree(&re);
        }
    }
    /* remove YYYY-MM-DD */
    {
        regex_t re;
        if (regcomp(&re, "[0-9]{4}-[0-9]{2}-[0-9]{2}", REG_EXTENDED) == 0) {
            regmatch_t m;
            while (regexec(&re, buf, 1, &m, 0) == 0) {
                size_t pre = (size_t)m.rm_so;
                size_t post = (size_t)m.rm_eo;
                memmove(buf + pre, buf + post, strlen(buf + post) + 1);
            }
            regfree(&re);
        }
    }
    /* remove <num>(.<num>)? (s|search units) — word-bounded like Python \b */
    {
        regex_t re;
        if (regcomp(&re, "\\<[0-9]+(\\.[0-9]+)?[ \t]*(s|sec|secs|seconds|m|min|mins|minutes|h|hr|hrs|hours)\\>", REG_EXTENDED | REG_ICASE) == 0) {
            regmatch_t m;
            while (regexec(&re, buf, 1, &m, 0) == 0) {
                size_t pre = (size_t)m.rm_so;
                size_t post = (size_t)m.rm_eo;
                memmove(buf + pre, buf + post, strlen(buf + post) + 1);
            }
            regfree(&re);
        }
    }
    /* collapse whitespace to single spaces, trim */
    {
        char *out = malloc(strlen(buf) + 1);
        size_t o = 0, i = 0;
        int last_space = 1;
        while (buf[i]) {
            if (isspace((unsigned char)buf[i])) {
                if (!last_space) { out[o++] = ' '; last_space = 1; }
                i++;
            } else { out[o++] = buf[i++]; last_space = 0; }
        }
        out[o] = '\0';
        free(buf); buf = out;
    }
    /* SHA-256 hex digest */
    unsigned char md[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)buf, strlen(buf), md);
    char *hex = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
    static const char *hx = "0123456789abcdef";
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hex[i*2]   = hx[md[i] >> 4];
        hex[i*2+1] = hx[md[i] & 0xf];
    }
    hex[SHA256_DIGEST_LENGTH * 2] = '\0';
    free(buf);
    return hex;
}

/*
 * PoP: loops_state_status_line @ hermes_cli/loops.py:LoopManager.status_line
 */
char *loops_state_status_line(const loop_state_t *s, double now) {
    if (!s || strcmp(s->status, "cleared") == 0)
        return strdup("No loop set. Start one with /loop [interval] <prompt>.");
    char *cad = loops_state_cadence_label(s);
    /* Python: fired = f"{ticks} tick{'s' if ticks != 1 else ''}" */
    char fired[64];
    snprintf(fired, sizeof(fired), "%ld tick%s", s->ticks_fired,
             s->ticks_fired == 1 ? "" : "s");
    char caps[320];
    if (s->times)
        snprintf(caps, sizeof(caps), "%ld/%ld runs", s->ticks_fired, s->times);
    else if (s->max_ticks)
        snprintf(caps, sizeof(caps), "%ld/%ld budget", s->ticks_fired, s->max_ticks);
    else
        snprintf(caps, sizeof(caps), "%s", fired);
    if (s->until && s->until[0]) {
        size_t need = strlen(caps) + strlen(s->until) + 16;
        char *c2 = malloc(need);
        snprintf(c2, need, "%s, until: %s", caps, s->until);
        strcpy(caps, c2); free(c2);
    }
    char meta[1024];
    snprintf(meta, sizeof(meta), "%s, %s", cad, caps);
    free(cad);
    char *base;
    if (strcmp(s->status, "active") == 0) {
        char *rem = loops_state_remaining_label(s, now);
        char *tail = (rem && rem[0]) ? rem : "";
        size_t need = 64 + strlen(meta) + (tail ? strlen(tail) : 0) + strlen(s->prompt);
        base = malloc(need);
        if (s->awaiting_response)
            snprintf(base, need, "\xE2\x86\xBB Loop (active, %s, wakeup running): %s", meta, s->prompt);
        else
            snprintf(base, need, "\xE2\x86\xBB Loop (active, %s, %s): %s", meta, tail, s->prompt);
        free(rem);
    } else if (strcmp(s->status, "paused") == 0) {
        size_t need = 64 + strlen(meta) + (s->paused_reason ? strlen(s->paused_reason) : 0) + strlen(s->prompt);
        base = malloc(need);
        snprintf(base, need, "\xE2\x8F\xB8 Loop (paused, %s%s%s): %s", meta,
                 s->paused_reason ? " — " : "", s->paused_reason ? s->paused_reason : "", s->prompt);
    } else { /* done */
        size_t need = 64 + strlen(meta) + (s->last_stop_reason ? strlen(s->last_stop_reason) : 0) + strlen(s->prompt);
        base = malloc(need);
        snprintf(base, need, "\xE2\x9C\x93 Loop finished (%s%s%s): %s", fired,
                 s->last_stop_reason ? " — " : "", s->last_stop_reason ? s->last_stop_reason : "", s->prompt);
    }
    return base;
}

/* Free a loop_state_t allocated by loops_state_from_json. */
void loops_state_free(loop_state_t *s) {
    if (!s) return;
    free(s->prompt); free(s->status); free(s->mode); free(s->until);
    free(s->last_response_digest); free(s->paused_reason); free(s->last_stop_reason);
    free(s);
}
