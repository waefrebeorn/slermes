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
