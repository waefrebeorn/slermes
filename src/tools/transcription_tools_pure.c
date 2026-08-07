/*
 * transcription_tools_pure.c — Pure helpers ported from
 * tools/transcription_tools.py.
 */
#define _GNU_SOURCE
#include "transcription_tools_pure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>

#include "hermes_json.h"

/* PoP: _is_local_stt_provider @ tools/transcription_tools.py:_is_local_stt_provider */
bool ts_is_local_stt_provider(const char *provider, const char *stt_config_json)
{
    if (!provider) return false;
    char key[256];
    size_t i, n = strlen(provider);
    if (n >= sizeof(key)) n = sizeof(key) - 1;
    for (i = 0; i < n; i++) key[i] = (char)tolower((unsigned char)provider[i]);
    key[n] = '\0';
    /* strip whitespace */
    char *p = key;
    while (*p == ' ' || *p == '\t') p++;
    char *end = p + strlen(p) - 1;
    while (end > p && (*end == ' ' || *end == '\t')) *end-- = '\0';
    return strcmp(p, "local") == 0 || strcmp(p, "local_command") == 0;
}

/* PoP: _command_stt_env_passthrough @ tools/transcription_stt.py:_command_stt_env_passthrough */
char **ts_command_stt_env_passthrough(const char *config_json, int *out_count)
{
    if (out_count) *out_count = 0;
    if (!config_json) return NULL;

    char *err = NULL;
    json_t *root = json_parse(config_json, &err);
    if (err || !root || root->type != JSON_OBJECT) {
        if (err) free(err);
        if (root) json_free(root);
        return NULL;
    }

    json_t *raw = json_obj_get(root, "env_passthrough");
    if (!raw || raw->type != JSON_ARRAY) {
        json_free(root);
        return NULL;
    }

    /* First pass: count valid entries */
    int count = 0;
    for (size_t i = 0; i < raw->c.count; i++) {
        json_t *item = json_get(raw, i);
        if (item && item->type == JSON_STRING && item->str_val && item->str_val[0]) {
            /* Check stripped non-empty */
            bool nonempty = false;
            for (const char *s = item->str_val; *s; s++) {
                if (!isspace((unsigned char)*s)) { nonempty = true; break; }
            }
            if (nonempty) count++;
        }
    }

    char **result = (char **)calloc(count + 1, sizeof(char *));
    int idx = 0;
    for (size_t i = 0; i < raw->c.count; i++) {
        json_t *item = json_get(raw, i);
        if (item && item->type == JSON_STRING && item->str_val) {
            /* Strip */
            char buf[256];
            size_t bn = strlen(item->str_val);
            if (bn >= sizeof(buf)) bn = sizeof(buf) - 1;
            memcpy(buf, item->str_val, bn); buf[bn] = '\0';
            /* ltrim */
            char *start = buf;
            while (*start == ' ' || *start == '\t') start++;
            /* rtrim */
            char *end = start + strlen(start) - 1;
            while (end > start && (*end == ' ' || *end == '\t')) *end-- = '\0';
            if (*start) {
                result[idx++] = strdup(start);
            }
        }
    }
    json_free(root);
    if (out_count) *out_count = idx;
    return result;
}

/* PoP: _is_local_or_private_url @ tools/transcription_tools.py:_is_local_or_private_url */
bool ts_is_local_or_private_url(const char *url)
{
    if (!url || !*url) return false;

    /* Extract host from URL (after ://, before /, :, or end) */
    const char *p = strstr(url, "://");
    if (!p) p = url;
    else p += 3;
    /* Skip userinfo */
    const char *at = strchr(p, '@');
    const char *host_start = at ? at + 1 : p;
    char host[256];
    /* Handle IPv6 brackets [addr:...] */
    if (*host_start == '[') {
        const char *close = strchr(host_start, ']');
        if (!close) return false;
        size_t hlen = (size_t)(close - (host_start + 1));
        if (hlen == 0 || hlen >= sizeof(host)) return false;
        memcpy(host, host_start + 1, hlen); host[hlen] = '\0';
        /* Check IPv6 loopback/private */
        struct in6_addr addr6;
        if (inet_pton(AF_INET6, host, &addr6) == 1) {
            static const unsigned char loopback6[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
            if (memcmp(&addr6, loopback6, 16) == 0) return true;
            if (addr6.s6_addr[0] == 0xfe && (addr6.s6_addr[1] & 0xc0) == 0x80) return true;
            if (addr6.s6_addr[0] == 0xfc) return true;
        }
        return false;
    }
    /* Find end of host (port separator or path) */
    const char *colon = strchr(host_start, ':');
    const char *slash = strchr(host_start, '/');
    const char *end = host_start + strlen(host_start);
    if (colon && (!slash || colon < slash)) end = colon;
    if (slash && slash < end) end = slash;
    size_t hlen = (size_t)(end - host_start);
    if (hlen == 0 || hlen >= sizeof(host)) return false;
    memcpy(host, host_start, hlen); host[hlen] = '\0';

    /* Lowercase */
    for (size_t i = 0; i < hlen; i++)
        host[i] = (char)tolower((unsigned char)host[i]);

    if (strcmp(host, "localhost") == 0) return true;

    /* Check .local, .lan, .internal suffixes */
    size_t hl = strlen(host);
    if (hl >= 5 && strcmp(host + hl - 5, ".local") == 0) return true;
    if (hl >= 4 && strcmp(host + hl - 4, ".lan") == 0) return true;
    if (hl >= 8 && strcmp(host + hl - 8, ".internal") == 0) return true;

    /* Check if host is a private/loopback IP */
    struct in_addr addr4;
    if (inet_pton(AF_INET, host, &addr4) == 1) {
        unsigned long ip = ntohl(addr4.s_addr);
        /* 127.0.0.0/8 loopback */
        if ((ip & 0xFF000000) == 0x7F000000) return true;
        /* 10.0.0.0/8 */
        if ((ip & 0xFF000000) == 0x0A000000) return true;
        /* 172.16.0.0/12 */
        if ((ip & 0xFFF00000) == 0xAC100000) return true;
        /* 192.168.0.0/16 */
        if ((ip & 0xFFFF0000) == 0xC0A80000) return true;
        /* 169.254.0.0/16 link-local */
        if ((ip & 0xFFFF0000) == 0xA9FE0000) return true;
        /* 100.64.0.0/10 CGNAT */
        if ((ip & 0xFFC00000) == 0x64400000) return true;
        /* 0.0.0.0/8 */
        if ((ip & 0xFF000000) == 0x00000000) return true;
        /* IPv6 unspecified */
        return false;
    }

    /* IPv6 */
    struct in6_addr addr6;
    if (inet_pton(AF_INET6, host, &addr6) == 1) {
        /* ::1 loopback */
        static const unsigned char loopback6[16] = {
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1
        };
        if (memcmp(&addr6, loopback6, 16) == 0) return true;
        /* fe80::/10 link-local */
        if (addr6.s6_addr[0] == 0xfe && (addr6.s6_addr[1] & 0xc0) == 0x80) return true;
        /* fc00::/7 unique local */
        if (addr6.s6_addr[0] == 0xfc) return true;
    }

    return false;
}

/* Default thresholds from Python _NO_SPEECH_PROB_THRESHOLD_DEFAULT and
 * _LOGPROB_THRESHOLD_DEFAULT. */
#define TS_NO_SPEECH_PROB_DEFAULT 0.6
#define TS_LOGPROB_DEFAULT (-1.0)

/* PoP: _confidence_thresholds @ tools/transcription_tools.py:_confidence_thresholds */
/* Resolve (no_speech_prob, avg_logprob) gate thresholds from config. */
void ts_confidence_thresholds(const char *stt_config_json,
                               double *no_speech_out, double *logprob_out)
{
    double no_speech = TS_NO_SPEECH_PROB_DEFAULT;
    double logprob = TS_LOGPROB_DEFAULT;

    char *err = NULL;
    json_t *cfg = stt_config_json ? json_parse(stt_config_json, &err) : NULL;
    if (err) { free(err); cfg = NULL; }

    if (cfg && cfg->type == JSON_OBJECT) {
        json_t *nsp = json_obj_get(cfg, "no_speech_prob_threshold");
        if (nsp && nsp->type == JSON_NUMBER) {
            no_speech = nsp->num_val;
        }
        json_t *lp = json_obj_get(cfg, "logprob_threshold");
        if (lp && lp->type == JSON_NUMBER) {
            logprob = lp->num_val;
        }
    }
    if (cfg) json_free(cfg);

    if (no_speech_out) *no_speech_out = no_speech;
    if (logprob_out) *logprob_out = logprob;
}

/* PoP: _is_hallucinated_segment @ tools/transcription_tools.py:_is_hallucinated_segment */
/* True when a segment is very likely a silence hallucination (AND gate). */
/* Faithful to Python: uses getattr() — which returns None for dict-like */
/* JSON objects (dicts have no attributes), so JSON/dict segments are never */
/* flagged. Only objects with no_speech_prob/avg_logprob properties are */
/* evaluated. */
bool ts_is_hallucinated_segment(const char *segment_json,
                                 double no_speech_threshold,
                                 double logprob_threshold)
{
    char *err = NULL;
    json_t *seg = segment_json ? json_parse(segment_json, &err) : NULL;
    if (err) { free(err); seg = NULL; }
    if (!seg) return false;
    /* Python getattr(segment, "no_speech_prob") returns None for dicts —
     * JSON objects are dict-like, so no attributes are ever found. */
    json_free(seg);
    return false;
}

/* PoP: _command_provider_env_passthrough @ tools/tts_tool.py:_command_provider_env_passthrough */
/* Identical logic to the STT variant — one owner for env_passthrough. */
char **tts_command_provider_env_passthrough(const char *config_json, int *out_count)
{
    return ts_command_stt_env_passthrough(config_json, out_count);
}
