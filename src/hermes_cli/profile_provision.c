/*
 * profile_provision.c — real C11 port of selected pure helpers in upstream
 * Hermes hermes_cli/profile_provision.py.
 *
 * PoP: w16_profilepro_1706f1_fresh @ hermes_cli/profile_provision.py:ProfileCreateRequest.fresh
 * PoP: w16_profilepro_1706f1_refusal @ hermes_cli/profile_provision.py:refusal
 * PoP: w16_profilepro_1706f1_message_from @ hermes_cli/profile_provision.py:message_from
 * PoP: w16_profilepro_1706f1_refusal_from @ hermes_cli/profile_provision.py:refusal_from
 */
#include "noc_w16_profilepro_1706f1.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* dupstr: never shadow POSIX dup(2). */
static char *dupstr(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* ERROR_DATA_TEXT * ERROR_DATA_ITEMS from hermes_cli.gateway_client (200 * 16). */
#define ERROR_DATA_TEXT 200
#define ERROR_DATA_ITEMS 16
#define KEPT (ERROR_DATA_TEXT * ERROR_DATA_ITEMS)
/* _TRUNCATED = " … [truncated]" : 14 code points. */
#define TRUNCATED " \xE2\x80\xA6 [truncated]"
#define TRUNCATED_CHARS 14

int w16_profilepro_1706f1_fresh(const char *clone_from, int clone_all, int clone_config) {
    int has_source = (clone_from && clone_from[0]) || clone_all || clone_config;
    return has_source ? 0 : 1;
}

json_t *w16_profilepro_1706f1_refusal(const char *text, const char *exc_type) {
    const char *t = text ? text : "";
    const char *kind = exc_type ? exc_type : "Exception";
    size_t n = strlen(t), i;
    json_t *parts = json_array();
    json_t *obj = json_object();

    if (n > KEPT) {
        /* Python slices by code point: text[:3186] (ASCII here => 3186 bytes)
         * then appends the 14-code-point marker.  Chunk the ASCII prefix by
         * 200 code points and attach the marker to the final partial chunk. */
        size_t prefix_len = KEPT - TRUNCATED_CHARS;
        for (i = 0; i + ERROR_DATA_TEXT <= prefix_len; i += ERROR_DATA_TEXT) {
            char *chunk = (char *)malloc(ERROR_DATA_TEXT + 1);
            if (!chunk) break;
            memcpy(chunk, t + i, ERROR_DATA_TEXT);
            chunk[ERROR_DATA_TEXT] = '\0';
            json_append(parts, json_string(chunk));
            free(chunk);
        }
        if (i < prefix_len) {
            size_t remain = prefix_len - i;
            char *chunk = (char *)malloc(remain + sizeof(TRUNCATED));
            if (chunk) {
                memcpy(chunk, t + i, remain);
                memcpy(chunk + remain, TRUNCATED, sizeof(TRUNCATED));
                json_append(parts, json_string(chunk));
                free(chunk);
            }
        }
    } else {
        for (i = 0; i < n; i += ERROR_DATA_TEXT) {
            size_t take = n - i;
            char *chunk;
            if (take > ERROR_DATA_TEXT) take = ERROR_DATA_TEXT;
            chunk = (char *)malloc(take + 1);
            if (!chunk) break;
            memcpy(chunk, t + i, take);
            chunk[take] = '\0';
            json_append(parts, json_string(chunk));
            free(chunk);
        }
    }

    json_set(obj, "text", json_string(t));
    json_set(obj, "type", json_string(kind));
    json_set(obj, "parts", parts);
    return obj;
}

char *w16_profilepro_1706f1_message_from(json_t *parts) {
    size_t total = 0, used = 0, i, count;
    char *out;
    if (parts && parts->type == JSON_ARRAY) count = parts->c.count; else count = 0;
    for (i = 0; i < count; i++) {
        json_t *p = parts->c.items[i];
        if (p && p->type == JSON_STRING && p->str_val) total += strlen(p->str_val);
    }
    out = (char *)malloc(total + 1);
    if (!out) return NULL;
    for (i = 0; i < count; i++) {
        json_t *p = parts->c.items[i];
        if (p && p->type == JSON_STRING && p->str_val) {
            size_t m = strlen(p->str_val);
            memcpy(out + used, p->str_val, m);
            used += m;
        }
    }
    out[used] = '\0'; /* NUL at the real (possibly shortened) length */
    return out;
}

json_t *w16_profilepro_1706f1_refusal_from(const char *exc_type, json_t *parts) {
    static const char *const kinds[] = {"FileExistsError", "FileNotFoundError", "ValueError"};
    char *msg;
    json_t *obj;
    size_t i;
    int known = 0;
    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++)
        if (exc_type && !strcmp(exc_type, kinds[i])) { known = 1; break; }
    if (!known) return json_null();
    msg = w16_profilepro_1706f1_message_from(parts);
    obj = json_object();
    json_set(obj, "type", json_string(exc_type));
    json_set(obj, "message", json_string(msg && msg[0] ? msg
        : "the gateway refused the create; nothing was created"));
    free(msg);
    return obj;
}
