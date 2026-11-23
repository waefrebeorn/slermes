/*
 * mcp_oauth_provider.c — C port of upstream Hermes tools/mcp_oauth_provider.py.
 *
 * PoP: w11_mcpoauthp_5e630c_metadata_issuer @ tools/mcp_oauth_provider.py:_metadata_issuer
 * PoP: w11_mcpoauthp_5e630c_asm_discovery_failure @ tools/mcp_oauth_provider.py:_asm_discovery_failure
 * PoP: w11_mcpoauthp_5e630c_with_discovery_context @ tools/mcp_oauth_provider.py:_with_discovery_context
 * PoP: w11_mcpoauthp_5e630c_metadata_issued_by_origin @ tools/mcp_oauth_provider.py:metadata_issued_by_origin
 * PoP: w11_mcpoauthp_5e630c_google_offline_access_params @ tools/mcp_oauth_provider.py:google_offline_access_params
 *
 * SDK objects are represented as JSON doubles from the fixture; urllib.parse
 * urlsplit is transcribed in C so no Python object model is needed here.
 */
#include "noc_w11_mcpoauthp_5e630c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DISCOVERY_LEAD "Could not read authorization-server metadata"
static const char *const ASM_PATHS[2] = {
    "/.well-known/oauth-authorization-server", "/.well-known/openid-configuration"
};

typedef struct {
    char scheme[64], netloc[512], path[2048], query[1024], fragment[1024];
    int has_userinfo;
} url_t;

static void url_split(const char *s, url_t *u) {
    memset(u, 0, sizeof(*u));
    char work[4096];
    const char *src = s ? s : "";
    snprintf(work, sizeof(work), "%s", src);
    char *hash = strchr(work, '#');
    if (hash) { *hash = '\0'; snprintf(u->fragment, sizeof(u->fragment), "%s", hash + 1); }
    char *q = strchr(work, '?');
    if (q) { *q = '\0'; snprintf(u->query, sizeof(u->query), "%s", q + 1); }
    char *colon = strchr(work, ':');
    char *slash = strchr(work, '/');
    char *rest = work;
    if (colon && (!slash || colon < slash)) {
        snprintf(u->scheme, sizeof(u->scheme), "%.*s", (int)(colon - work), work);
        rest = colon + 1;
    }
    if (!strncmp(rest, "//", 2)) {
        char *start = rest + 2;
        char *end = strchr(start, '/');
        if (end) {
            snprintf(u->netloc, sizeof(u->netloc), "%.*s", (int)(end - start), start);
            snprintf(u->path, sizeof(u->path), "%s", end);
        } else {
            snprintf(u->netloc, sizeof(u->netloc), "%s", start);
        }
    } else {
        snprintf(u->path, sizeof(u->path), "%s", rest);
    }
    if (strchr(u->netloc, '@')) u->has_userinfo = 1;
}

static char *rstrip_slash(const char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && s[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    if (n) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

json_t *w11_mcpoauthp_5e630c_metadata_issuer(const struct json_t *context) {
    json_t *meta = json_obj_get(context, "oauth_metadata");
    json_t *iss = json_obj_get(meta, "issuer");
    if (!iss || iss->type != JSON_STRING || !iss->str_val || !*iss->str_val) return json_null();
    char *r = rstrip_slash(iss->str_val); /* rstrip("/") */
    json_t *out = (*r) ? json_string(r) : json_null();
    free(r);
    return out;
}

json_t *w11_mcpoauthp_5e630c_asm_discovery_failure(const struct json_t *response) {
    json_t *req = json_obj_get(response, "request");
    json_t *sc = json_obj_get(response, "status_code");
    if (!req || req->type == JSON_NULL || !sc || sc->type == JSON_NULL) return json_null();
    long long status = 0;
    if (sc->type == JSON_NUMBER) status = (long long)sc->num_val;
    else if (sc->type == JSON_STRING) status = strtoll(sc->str_val, NULL, 10);
    else return json_null();
    if (status >= 200 && status < 300) return json_null();
    const char *url = json_get_str(req, "url", "");
    int hit = 0;
    for (int i = 0; i < 2; i++) if (strstr(url, ASM_PATHS[i])) hit = 1;
    if (!hit) return json_null();
    char buf[4096];
    snprintf(buf, sizeof(buf), "%lld from %s", status, url);
    return json_string(buf);
}

json_t *w11_mcpoauthp_5e630c_with_discovery_context(const char *exc, const struct json_t *failures) {
    char joined[8192];
    size_t pos = 0;
    joined[0] = '\0';
    if (failures && failures->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(failures); i++) {
            json_t *e = json_get(failures, i);
            const char *s = (e && e->type == JSON_STRING) ? e->str_val : "";
            pos += (size_t)snprintf(joined + pos, sizeof(joined) - pos, "%s%s", i ? "; " : "", s);
        }
    }
    char buf[16384];
    snprintf(buf, sizeof(buf), "%s (%s); dynamic client registration then fell back to a guessed endpoint on the MCP host and failed: %s",
             DISCOVERY_LEAD, joined, exc ? exc : "");
    return json_string(buf);
}

static int has_dotdot_segment(const char *path) {
    const char *p = path;
    while (*p) {
        const char *seg = p;
        while (*p && *p != '/') p++;
        size_t n = (size_t)(p - seg);
        if (n == 2 && seg[0] == '.' && seg[1] == '.') return 1;
        if (*p == '/') p++;
    }
    return 0;
}

json_t *w11_mcpoauthp_5e630c_metadata_issued_by_origin(const struct json_t *metadata,
                                                       const char *auth_server_url,
                                                       const struct json_t *response) {
    if (!auth_server_url || !*auth_server_url) return json_bool(false);
    url_t parts;
    url_split(auth_server_url, &parts);
    char *path = rstrip_slash(parts.path);
    json_t *sc = json_obj_get(response, "status_code");
    long long status = -1;
    if (sc && sc->type == JSON_NUMBER) status = (long long)sc->num_val;
    else if (sc && sc->type == JSON_STRING) status = strtoll(sc->str_val, NULL, 10);
    int bad = (!path[0] || has_dotdot_segment(path) || parts.has_userinfo ||
               parts.query[0] || parts.fragment[0] || status != 200);
    if (bad) { free(path); return json_bool(false); }
    char origin[1024], derived[2048];
    snprintf(origin, sizeof(origin), "%s://%s", parts.scheme, parts.netloc);
    snprintf(derived, sizeof(derived), "%s/.well-known/oauth-authorization-server%s", origin, path);
    const char *rurl = json_get_str(response, "url", "");
    json_t *iss = json_obj_get(metadata, "issuer");
    char *issuer = rstrip_slash(iss && iss->type == JSON_STRING ? iss->str_val : "");
    int ok = (!strcmp(rurl, derived) && !strcmp(issuer, origin));
    free(issuer); free(path);
    return json_bool(ok ? true : false);
}

json_t *w11_mcpoauthp_5e630c_google_offline_access_params(const struct json_t *context) {
    json_t *iss = w11_mcpoauthp_5e630c_metadata_issuer(context);
    json_t *out = json_object();
    if (iss && iss->type == JSON_STRING && iss->str_val) {
        url_t parts;
        url_split(iss->str_val, &parts);
        if (!strcmp(parts.netloc, "accounts.google.com"))
            json_set(out, "access_type", json_string("offline"));
    }
    json_free(iss);
    return out;
}
