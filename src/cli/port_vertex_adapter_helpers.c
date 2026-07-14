/*
 * port_vertex_adapter_helpers.c
 *
 * Pure, portable helper functions ported from agent/vertex_adapter.py.
 * No google-auth, no secret_scope, no os.path.exists, no config load. The
 * precedence resolvers are ported as pure functions taking already-resolved
 * inputs (env/cfg values supplied by the caller), faithful to the precedence
 * rules. Coupled helpers (get_vertex_credentials, _refresh_credentials,
 * get_vertex_config, _vertex_config) stay REAL_GAP.
 *
 * C name <- python name (module prefix 'vertex_adapter_'):
 *   vertex_adapter_build_base_url        <- build_vertex_base_url
 *   vertex_adapter_resolve_region         <- _resolve_region
 *   vertex_adapter_resolve_project_override <- _resolve_project_override
 *   vertex_adapter_resolve_credentials_path <- _resolve_credentials_path
 *   vertex_adapter_has_vertex_credentials <- has_vertex_credentials
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/stat.h>
#include "hermes_json.h"
#include "hermes_yaml.h"
#include "hermes_http.h"
#include "crypto.h"
#include "google_oauth.h"

#define VERTEX_DEFAULT_REGION "global"

/* trim leading/trailing whitespace for "explicit.strip()" semantics */
static void trim(char *s)
{
    if (!s) return;
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\n'||s[n-1]=='\r')) s[--n]='\0';
}

static char *first_nonempty(const char *a, const char *b)
{
    if (a && a[0]) return strdup(a);
    if (b && b[0]) return strdup(b);
    return NULL;
}

/*
 * PoP: build_vertex_base_url @ agent/vertex_adapter.py:build_vertex_base_url */
char *vertex_adapter_build_base_url(const char *project_id, const char *region)
{
    if (!project_id) project_id = "";
    if (!region || !region[0]) region = VERTEX_DEFAULT_REGION;
    const char *host = (strcmp(region, "global") == 0)
        ? "aiplatform.googleapis.com"
        : NULL;
    char buf[1024];
    if (host) {
        snprintf(buf, sizeof(buf),
            "https://%s/v1beta1/projects/%s/locations/%s/endpoints/openapi",
            host, project_id, region);
    } else {
        snprintf(buf, sizeof(buf),
            "https://%s-aiplatform.googleapis.com/v1beta1/projects/%s/locations/%s/endpoints/openapi",
            region, project_id, region);
    }
    return strdup(buf);
}

/*
 * PoP: _resolve_region @ agent/vertex_adapter.py:_resolve_region
 * precedence: explicit > env_region > cfg_region > "global". Caller supplies
 * trimmed env/cfg values. Returns malloc'd region. Caller frees. */
char *vertex_adapter_resolve_region(const char *explicit, const char *env_region, const char *cfg_region)
{
    char *a = explicit ? strdup(explicit) : NULL; if (a) { trim(a); }
    char *b = env_region ? strdup(env_region) : NULL; if (b) { trim(b); }
    char *c = cfg_region ? strdup(cfg_region) : NULL; if (c) { trim(c); }
    char *r = first_nonempty(a, NULL);
    if (!r) r = first_nonempty(b, NULL);
    if (!r) r = first_nonempty(c, NULL);
    free(a); free(b); free(c);
    if (!r) return strdup(VERTEX_DEFAULT_REGION);
    return r;
}

/*
 * PoP: _resolve_project_override @ agent/vertex_adapter.py:_resolve_project_override
 * precedence: env_project > cfg_project > None. Returns malloc'd project or NULL. */
char *vertex_adapter_resolve_project_override(const char *env_project, const char *cfg_project)
{
    char *a = env_project ? strdup(env_project) : NULL; if (a) { trim(a); }
    char *b = cfg_project ? strdup(cfg_project) : NULL; if (b) { trim(b); }
    char *r = first_nonempty(a, NULL);
    if (!r) r = first_nonempty(b, NULL);
    free(a); free(b);
    return r; /* may be NULL */
}

/*
 * PoP: _resolve_credentials_path @ agent/vertex_adapter.py:_resolve_credentials_path
 * precedence: explicit > VERTEX_CREDENTIALS_PATH > GOOGLE_APPLICATION_CREDENTIALS.
 * The Python version also checks os.path.exists; here the caller passes already
 * path-checked candidates (or NULL). Returns malloc'd path or NULL. */
char *vertex_adapter_resolve_credentials_path(const char *explicit, const char *path1, const char *path2)
{
    char *a = explicit ? strdup(explicit) : NULL; if (a) { trim(a); }
    char *b = path1 ? strdup(path1) : NULL; if (b) { trim(b); }
    char *c = path2 ? strdup(path2) : NULL; if (c) { trim(c); }
    char *r = first_nonempty(a, NULL);
    if (!r) r = first_nonempty(b, NULL);
    if (!r) r = first_nonempty(c, NULL);
    free(a); free(b); free(c);
    return r; /* may be NULL */
}

/*
 * PoP: has_vertex_credentials @ agent/vertex_adapter.py:has_vertex_credentials
 * True when a resolved creds path OR a project override is present. Caller
 * supplies the already-resolved candidates (matches the python short-circuit). */
int vertex_adapter_has_vertex_credentials(const char *creds_path, const char *project_override)
{
    if (creds_path && creds_path[0]) return 1;
    if (project_override && project_override[0]) return 1;
    return 0;
}

/* === AG26: real port of the credential-minting functions (no NA) ===
 *
 * The Python module obtains a GCP access token via google.auth (service-account
 * JWT assertion to oauth2.googleapis.com). We implement the equivalent flow in
 * C: parse the SA JSON, build + RS256-sign a JWT, exchange it for an access
 * token over HTTPS. Token is cached and re-minted within 5 min of expiry. */

static char *vx_get_secret(const char *name)
{
    /* Read from environment (in single-process C the profile == process). */
    const char *v = getenv(name);
    return (v && v[0]) ? (char *)v : NULL;
}

static char *vx_config_path(void)
{
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    static char buf[1024];
    if (!home) return NULL;
    snprintf(buf, sizeof(buf), "%s/.hermes/config.yaml", home);
    return buf;
}

/*
 * PoP: _vertex_config @ agent/vertex_adapter.py:_vertex_config
 * Returns malloc'd JSON object of the `vertex:` section of config.yaml, or
 * "{}" on any failure. Caller frees. */
char *vertex_adapter_vertex_config(void)
{
    char *path = vx_config_path();
    if (!path) return strdup("{}");
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return strdup("{}");
    char *js = yaml_to_json_string(doc, "vertex");
    yaml_free(doc);
    if (!js) return strdup("{}");
    /* yaml_to_json_string may return "null" for missing key; normalize. */
    if (strcmp(js, "null") == 0) { free(js); return strdup("{}"); }
    return js;
}

/*
 * PoP: _resolve_credentials_path @ agent/vertex_adapter.py:_resolve_credentials_path
 * (already ported above as vertex_adapter_resolve_credentials_path). */

/* In-process token cache (mirrors Python _creds_cache). */
typedef struct {
    char *path;        /* cache key = resolved SA path or "__adc__" */
    char *token;
    char *project_id;
    double expiry;     /* unix seconds */
} vx_cache_t;
static vx_cache_t g_vx_cache;

/* Parse a service-account JSON file. Returns malloc'd token via token_uri
 * exchange, sets *out_project. Caller frees token; *out_project is malloc'd
 * (caller frees) or NULL. Returns NULL on failure. */
static char *vx_mint_from_sa(const char *sa_path, char **out_project)
{
    *out_project = NULL;
    FILE *f = fopen(sa_path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f); buf[rd] = '\0';
    fclose(f);

    json_t *sa = json_parse(buf, NULL);
    free(buf);
    if (!sa || sa->type != JSON_OBJECT) { if (sa) json_free(sa); return NULL; }

    json_t *client_email = json_obj_get(sa, "client_email");
    json_t *private_key = json_obj_get(sa, "private_key");
    json_t *project_id = json_obj_get(sa, "project_id");
    json_t *token_uri = json_obj_get(sa, "token_uri");
    if (!client_email || client_email->type != JSON_STRING ||
        !private_key || private_key->type != JSON_STRING) {
        json_free(sa); return NULL;
    }
    const char *iss = client_email->str_val;
    const char *pk = private_key->str_val;
    const char *aud = (token_uri && token_uri->type == JSON_STRING && token_uri->str_val[0])
        ? token_uri->str_val : "https://oauth2.googleapis.com/token";
    if (project_id && project_id->type == JSON_STRING && project_id->str_val[0])
        *out_project = strdup(project_id->str_val);

    long now = (long)time(NULL);
    char header[256];
    snprintf(header, sizeof(header), "{\"alg\":\"RS256\",\"typ\":\"JWT\"}");
    char claim[1024];
    snprintf(claim, sizeof(claim),
        "{\"iss\":\"%s\",\"scope\":\"https://www.googleapis.com/auth/cloud-platform\","
        "\"aud\":\"%s\",\"iat\":%ld,\"exp\":%ld}",
        iss, aud, now, now + 3600);

    char *h_b64 = crypto_base64url_encode((const unsigned char *)header, strlen(header));
    char *c_b64 = crypto_base64url_encode((const unsigned char *)claim, strlen(claim));
    size_t sil_len = strlen(h_b64) + 1 + strlen(c_b64);
    char *signing_input = malloc(sil_len + 1);
    snprintf(signing_input, sil_len + 1, "%s.%s", h_b64, c_b64);
    free(h_b64); free(c_b64);

    char *sig = crypto_rs256_sign_b64url(pk, (const unsigned char *)signing_input, strlen(signing_input));
    free(signing_input);
    if (!sig) { json_free(sa); return NULL; }
    size_t assertion_len = strlen(signing_input) + 1 + strlen(sig);
    char *assertion = malloc(assertion_len + 1);
    snprintf(assertion, assertion_len + 1, "%s.%s", signing_input, sig);
    free(sig);

    /* Build form body. */
    char *body = malloc(strlen(assertion) + 128);
    snprintf(body, strlen(assertion) + 128,
        "grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Ajwt-bearer&assertion=%s",
        assertion);
    free(assertion);

    http_t *h = http_new(30);
    char *token = NULL;
    if (h) {
        const char *hdr = "Content-Type: application/x-www-form-urlencoded";
        http_resp_t *resp = http_request(h, HTTP_POST, aud, hdr, body, strlen(body));
        if (resp && resp->status == 200 && resp->body) {
            json_t *r = json_parse(resp->body, NULL);
            if (r && r->type == JSON_OBJECT) {
                json_t *at = json_obj_get(r, "access_token");
                if (at && at->type == JSON_STRING) token = strdup(at->str_val);
            }
            if (r) json_free(r);
        }
        if (resp) http_resp_free(resp);
        http_free(h);
    }
    free(body);
    json_free(sa);
    return token;
}

/*
 * PoP: get_vertex_credentials @ agent/vertex_adapter.py:get_vertex_credentials
 * Returns malloc'd (access_token, project_id). On failure both are NULL.
 * token_out is malloc'd (caller frees) or NULL; *project_out is malloc'd or NULL. */
void vertex_adapter_get_vertex_credentials(const char *credentials_path,
                                            char **token_out, char **project_out)
{
    *token_out = NULL;
    *project_out = NULL;

    /* Resolve SA path: explicit > VERTEX_CREDENTIALS_PATH > GOOGLE_APPLICATION_CREDENTIALS */
    char *resolved = NULL;
    struct stat st;
    if (credentials_path && credentials_path[0] && stat(credentials_path, &st) == 0) {
        resolved = strdup(credentials_path);
    } else {
        const char *v1 = vx_get_secret("VERTEX_CREDENTIALS_PATH");
        const char *v2 = vx_get_secret("GOOGLE_APPLICATION_CREDENTIALS");
        if (v1 && v1[0] && stat(v1, &st) == 0) resolved = strdup(v1);
        else if (v2 && v2[0] && stat(v2, &st) == 0) resolved = strdup(v2);
    }
    const char *cache_key = resolved ? resolved : "__adc__";

    /* Cache hit? */
    if (g_vx_cache.path && strcmp(g_vx_cache.path, cache_key) == 0 &&
        g_vx_cache.token && (g_vx_cache.expiry - time(NULL)) > 300) {
        *token_out = strdup(g_vx_cache.token);
        if (g_vx_cache.project_id) *project_out = strdup(g_vx_cache.project_id);
        free(resolved);
        return;
    }

    char *project = NULL;
    char *token = NULL;
    if (resolved) {
        token = vx_mint_from_sa(resolved, &project);
    } else {
        /* No SA path: ADC — try the user OAuth flow's stored credentials. */
        char *err = NULL;
        token = google_oauth_get_valid_token(&err);
        if (err) free(err);
        if (token) project = NULL; /* project resolved via override below */
    }

    if (!token) { free(resolved); if (project) free(project); return; }

    /* Project override: VERTEX_PROJECT_ID env > config vertex.project_id */
    char *proj_override = NULL;
    const char *e = vx_get_secret("VERTEX_PROJECT_ID");
    if (e && e[0]) proj_override = strdup(e);
    else {
        char *cfg = vertex_adapter_vertex_config();
        if (cfg) {
            json_t *c = json_parse(cfg, NULL);
            free(cfg);
            if (c && c->type == JSON_OBJECT) {
                json_t *pid = json_obj_get(c, "project_id");
                if (pid && pid->type == JSON_STRING && pid->str_val[0])
                    proj_override = strdup(pid->str_val);
            }
            if (c) json_free(c);
        }
    }
    if (proj_override) { if (project) free(project); project = proj_override; }

    /* Update cache. */
    free(g_vx_cache.path); free(g_vx_cache.token); free(g_vx_cache.project_id);
    g_vx_cache.path = strdup(cache_key);
    g_vx_cache.token = strdup(token);
    g_vx_cache.project_id = project ? strdup(project) : NULL;
    g_vx_cache.expiry = (double)(time(NULL) + 3600);

    *token_out = token;        /* transfer ownership */
    *project_out = project;    /* transfer ownership */
    free(resolved);
}

/*
 * PoP: _refresh_credentials @ agent/vertex_adapter.py:_refresh_credentials
 * Re-mint the access token for the given SA path (SA flow has no refresh
 * token; "refresh" == re-assert). Writes the new token into *token_out
 * (malloc'd, caller frees) or sets it NULL on failure. */
void vertex_adapter_refresh_credentials(const char *credentials_path, char **token_out)
{
    *token_out = NULL;
    char *p = NULL;
    if (credentials_path && credentials_path[0]) p = (char *)credentials_path;
    else {
        const char *v1 = vx_get_secret("VERTEX_CREDENTIALS_PATH");
        const char *v2 = vx_get_secret("GOOGLE_APPLICATION_CREDENTIALS");
        p = (v1 && v1[0]) ? (char *)v1 : (char *)v2;
    }
    if (!p || !p[0]) return;
    struct stat st;
    if (stat(p, &st) != 0) return;
    char *proj = NULL;
    *token_out = vx_mint_from_sa(p, &proj);
    if (proj) free(proj);
}

/*
 * PoP: get_vertex_config @ agent/vertex_adapter.py:get_vertex_config
 * Returns malloc'd (access_token, base_url). On failure both NULL. */
void vertex_adapter_get_vertex_config(const char *credentials_path, const char *region,
                                       char **token_out, char **base_url_out)
{
    *token_out = NULL;
    *base_url_out = NULL;
    char *token = NULL, *project = NULL;
    vertex_adapter_get_vertex_credentials(credentials_path, &token, &project);
    if (!token || !project) { if (token) free(token); if (project) free(project); return; }

    /* effective_region = region arg > VERTEX_REGION env > config region > global */
    char *eff_region = NULL;
    if (region && region[0]) eff_region = strdup(region);
    else {
        const char *er = vx_get_secret("VERTEX_REGION");
        if (er && er[0]) eff_region = strdup(er);
        else {
            char *cfg = vertex_adapter_vertex_config();
            if (cfg) {
                json_t *c = json_parse(cfg, NULL); free(cfg);
                if (c && c->type == JSON_OBJECT) {
                    json_t *r = json_obj_get(c, "region");
                    if (r && r->type == JSON_STRING && r->str_val[0]) eff_region = strdup(r->str_val);
                    json_free(c);
                }
            }
        }
    }
    if (!eff_region) eff_region = strdup(VERTEX_DEFAULT_REGION);

    *token_out = token;
    *base_url_out = vertex_adapter_build_base_url(project, eff_region);
    free(project);
    free(eff_region);
}

