/*
 * client.c - C11 port of upstream tools/connectors/portal/client.py
 * (gate w26_client_1fcd78).
 *
 * PoP: w26_client_1fcd78_validate_slug @ tools/connectors/portal/client.py:validate_slug
 * PoP: w26_client_1fcd78_route_key @ tools/connectors/portal/client.py:_route_key
 * PoP: w26_client_1fcd78_auth_token @ tools/connectors/portal/client.py:PortalConnectorClient.authorization_token
 * PoP: w26_client_1fcd78_constants @ tools/connectors/portal/client.py:<module constants>
 *
 * PortalConnectorClient's transport, endpoint resolver and header provider are
 * the seams: the live class takes all three by injection, so the header set it
 * would have built is handed in here and the bearer decision is made on it.
 * _request/_parse stay with the caller -- nothing in this file opens a socket,
 * reads a token or parses a pydantic model.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_client_1fcd78.h"

/* DEFAULT_TIMEOUT_SECONDS */
#define PC_TIMEOUT_SECONDS 30.0
/* _SLUG_RE: ^[a-z0-9][a-z0-9_-]*$ */
#define PC_SLUG_PATTERN "^[a-z0-9][a-z0-9_-]*$"

/* _PORTAL_ROUTE_WORDS */
static const char *const PC_ROUTE_WORDS[] = {
    "accounts", "catalog", "connectors", "policy", "public", "tools",
};
#define PC_ROUTE_WORD_N 6

static int pc_route_word(const char *part, size_t len) {
    int i;
    for (i = 0; i < PC_ROUTE_WORD_N; i++)
        if (strlen(PC_ROUTE_WORDS[i]) == len && !memcmp(PC_ROUTE_WORDS[i], part, len))
            return 1;
    return 0;
}

/* Python str.strip() over ASCII whitespace. */
static char *pc_strip(const char *s) {
    const char *end;
    size_t n;
    char *out;
    if (!s) return strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - s);
    out = (char *)malloc(n + 1);
    if (!out) return strdup("");
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

json_t *w26_client_1fcd78_validate_slug(const json_t *req) {
    const json_t *slug = req ? json_obj_get(req, "slug") : NULL;
    json_t *out = json_object();
    const char *s;
    size_t i;
    int ok;

    if (!slug || slug->type != JSON_STRING) {
        /* re.fullmatch on a non-str raises TypeError before any match. */
        json_set(out, "valid", json_bool(false));
        json_set(out, "error", json_string("TypeError"));
        return out;
    }
    s = slug->str_val ? slug->str_val : "";
    ok = s[0] != '\0' && ((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'));
    if (ok) {
        for (i = 1; s[i]; i++) {
            char c = s[i];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') continue;
            ok = 0;
            break;
        }
    }
    json_set(out, "valid", json_bool(ok ? true : false));
    json_set(out, "error", ok ? json_null() : json_string("InvalidConnectorSlug"));
    return out;
}

json_t *w26_client_1fcd78_route_key(const json_t *req) {
    const char *path = req ? json_get_str(req, "path", "") : "";
    char *work = strdup(path ? path : "");
    char *body, *p, *part;
    size_t start, end, index = 0;
    char out[512];
    size_t used;

    if (!work) return json_string("portal");
    /* path.strip("/") */
    start = 0;
    end = strlen(work);
    while (start < end && work[start] == '/') start++;
    while (end > start && work[end - 1] == '/') end--;
    memmove(work, work + start, end - start);
    work[end - start] = '\0';
    body = work;

    snprintf(out, sizeof(out), "portal");
    used = strlen(out);
    /* split("/")[2:]: the verbs after "/api/v1/" */
    part = body;
    while (part) {
        char *slash = strchr(part, '/');
        size_t len;
        if (slash) { *slash = '\0'; p = slash + 1; } else p = NULL;
        len = strlen(part);
        if (index >= 2 && len && pc_route_word(part, len) && used + len + 2 < sizeof(out)) {
            out[used++] = '.';
            memcpy(out + used, part, len);
            used += len;
            out[used] = '\0';
        }
        index++;
        part = p;
    }
    free(work);
    return json_string(out);
}

json_t *w26_client_1fcd78_auth_token(const json_t *req) {
    const json_t *headers = req ? json_obj_get(req, "headers") : NULL;
    const json_t *auth = headers && headers->type == JSON_OBJECT ? json_obj_get(headers, "Authorization") : NULL;
    json_t *out = json_object();

    if (!auth || auth->type != JSON_STRING || !auth->str_val) {
        json_set(out, "token", json_null());
        json_set(out, "error", json_string("GatewayAuthError"));
        return out;
    }
    {
        const char *value = auth->str_val;
        char *trimmed = pc_strip(value);
        const char *space;
        char *scheme, *token;
        if (!trimmed[0]) {
            free(trimmed);
            json_set(out, "token", json_null());
            json_set(out, "error", json_string("GatewayAuthError"));
            return out;
        }
        space = strchr(trimmed, ' ');
        if (!space) {
            scheme = trimmed;
            token = "";
        } else {
            scheme = trimmed;
            *((char *)space) = '\0';
            token = (char *)space + 1;
        }
        {
            char *stripped_token = pc_strip(token);
            size_t i;
            char lower[64];
            int is_bearer;
            for (i = 0; scheme[i] && i + 1 < sizeof(lower); i++)
                lower[i] = (char)tolower((unsigned char)scheme[i]);
            lower[i] = '\0';
            is_bearer = !strcmp(lower, "bearer");
            if (is_bearer && stripped_token[0]) {
                json_set(out, "token", json_string(stripped_token));
                json_set(out, "error", json_null());
            } else {
                json_set(out, "token", json_null());
                json_set(out, "error", json_null());
            }
            free(stripped_token);
        }
        free(trimmed);
    }
    return out;
}

json_t *w26_client_1fcd78_constants(void) {
    json_t *o = json_object();
    json_t *words = json_array();
    int i;
    json_set(o, "default_timeout_seconds", json_number(PC_TIMEOUT_SECONDS));
    json_set(o, "slug_pattern", json_string(PC_SLUG_PATTERN));
    for (i = 0; i < PC_ROUTE_WORD_N; i++)
        json_append(words, json_string(PC_ROUTE_WORDS[i]));
    json_set(o, "route_words", words);
    return o;
}
