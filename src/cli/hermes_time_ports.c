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
