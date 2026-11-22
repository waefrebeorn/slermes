/* hermes_time_ports.c — LIVE-verified port of hermes_time._env_timezone. */
#include <stdlib.h>
#include <string.h>

/* PoP: _env_timezone @ hermes_time.py:_env_timezone */
const char *hc_env_timezone(int multiplex_active)
{
    static char out[512];
    out[0] = '\0';
    if (multiplex_active) return out;
    const char *v = getenv("HERMES_TIMEZONE");
    if (!v) return out;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    size_t n = strlen(v);
    while (n && (v[n-1] == ' ' || v[n-1] == '\t' ||
                 v[n-1] == '\n' || v[n-1] == '\r')) n--;
    if (n >= sizeof(out)) n = sizeof(out) - 1;
    memcpy(out, v, n);
    out[n] = '\0';
    return out;
}

/* PoP: _timezone_cache_identity @ hermes_time.py:_timezone_cache_identity */
void hc_timezone_cache_identity(int multiplex_active, const char *config_path,
                               char *out, size_t n)
{
    const char *tz = hc_env_timezone(multiplex_active);
    if (tz && *tz)
        snprintf(out, n, "[\"environment\",\"%s\"]", tz);
    else
        snprintf(out, n, "[\"config\",\"%s\"]", config_path ? config_path : "");
}
