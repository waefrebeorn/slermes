/* terminal_breadcrumbs_pure.c — Faithful C11 port of the pure, deterministic
 * helpers in hermes_cli/terminal_breadcrumbs.py (per-terminal session
 * breadcrumbs). The tty/config/DB glue is NOT ported (not pure logic).
 *
 * Ports (PoP):
 *   _SANITIZE_RE.sub("-", raw...)  -> tbr_sanitize  (filename-safe id)
 *   stale check (now - ts > window) -> tbr_is_stale_payload
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TBR_STALE_AFTER_SECONDS (30LL * 24 * 3600)

/* _SANITIZE_RE = [^A-Za-z0-9._-] ; sanitize strips + replaces, caps 120. */
char *tbr_sanitize(const char *raw) {
    char *s = strdup(raw ? raw : "");
    /* strip leading/trailing whitespace + slashes */
    char *p = s; while (*p == ' ' || *p == '\t') p++;
    size_t l = strlen(p); while (l > 0 && (p[l - 1] == ' ' || p[l - 1] == '\t' || p[l - 1] == '/')) p[--l] = 0;
    /* replace invalid chars with '-' */
    char *out = (char *)malloc(128);
    size_t o = 0;
    for (size_t i = 0; p[i] && o < 119; i++) {
        char c = p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            out[o++] = c;
        } else {
            out[o++] = '-';
        }
    }
    out[o] = '\0';
    free(s);
    return out;
}

/* Mirrors read_breadcrumb's staleness gate: ts (epoch float) older than the
 * window from now is stale. Returns 1 if stale, 0 otherwise. A non-finite /
 * missing ts is treated as stale (best-effort: drop). *//* PoP: tbr_is_stale_payload @ hermes_cli/terminal_breadcrumbs.py:_STALE_AFTER_SECONDS */

int tbr_is_stale_payload(double ts, double now) {
    if (!(ts == ts) || ts <= 0.0) return 1;  /* NaN or missing -> stale */
    return (now - ts) > (double)TBR_STALE_AFTER_SECONDS;
}
