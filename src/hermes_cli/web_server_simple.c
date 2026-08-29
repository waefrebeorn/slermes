/* web_server_simple.c — additional faithful C11 ports from hermes_cli/web_server.py.
 *
 * Covers REAL_GAP functions that are simple standalone helpers with minimal
 * Python dependencies.
 */

#define _GNU_SOURCE
#include "web_server_simple.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

/* Forward declaration for the public URL resolver in dashboard_auth_prefix.c */
extern int cli_hermes_cli_dashboard_auth_prefix_resolve_public_url(char *output, size_t output_size);

/* ── _FS_MIME_TYPES table ──────────────────────────────────────────── */
/* Mirrors hermes_cli/web_server.py:_FS_MIME_TYPES dict. */
static const struct { const char *suffix; const char *mime; } FS_MIME_TYPES[] = {
    { ".avi",  "video/x-msvideo" },
    { ".bmp",  "image/bmp" },
    { ".flac", "audio/flac" },
    { ".gif",  "image/gif" },
    { ".jpeg", "image/jpeg" },
    { ".jpg",  "image/jpeg" },
    { ".m4a",  "audio/mp4" },
    { ".mkv",  "video/x-matroska" },
    { ".mov",  "video/quicktime" },
    { ".mp3",  "audio/mpeg" },
    { ".mp4",  "video/mp4" },
    { ".ogg",  "audio/ogg" },
    { ".opus", "audio/ogg; codecs=opus" },
    { ".png",  "image/png" },
    { ".svg",  "image/svg+xml" },
    { ".wav",  "audio/wav" },
    { ".webm", "video/webm" },
    { ".webp", "image/webp" },
    { NULL, NULL }
};

/* ── _is_addr_in_use_error ──────────────────────────────────────────── */
/* PoP: ws_is_addr_in_use_error @ hermes_cli/web_server.py:_is_addr_in_use_error */
/* True when errno indicates an address-in-use bind failure. */
bool ws_is_addr_in_use_error(int errnum) {
    switch (errnum) {
        case 98:   /* Linux EADDRINUSE */
        case 48:   /* macOS EADDRINUSE */
        case 10048: /* Windows WSAEADDRINUSE */
            return true;
        default:
            return false;
    }
}

/* ── _durable_completed_update_action_id ────────────────────────────── */
/* PoP: ws_durable_completed_update_action_id @ hermes_cli/web_server.py:_durable_completed_update_action_id */
/* Recover the latest successful update identity from update.log lines. */
char *ws_durable_completed_update_action_id(const char **lines, size_t n_lines) {
    int last_start = -1;
    int last_completed = -1;
    char *completed_action_id = NULL;

    for (size_t idx = 0; idx < n_lines; idx++) {
        const char *line = lines[idx];
        if (!line) continue;

        /* Check for "=== hermes update started " prefix */
        if (strncmp(line, "=== hermes update started ", 27) == 0) {
            last_start = (int)idx;
        }

        /* Check for completion marker: === hermes-update completed <hex> === */
        if (strncmp(line, "=== hermes-update completed ", 29) == 0) {
            const char *id_start = line + 29;
            if (strlen(id_start) >= 32 &&
                strncmp(id_start + 32, " ===", 4) == 0) {
                last_completed = (int)idx;
                completed_action_id = (char *)malloc(33);
                if (completed_action_id) {
                    memcpy(completed_action_id, id_start, 32);
                    completed_action_id[32] = '\0';
                }
            }
        }
    }

    if (completed_action_id && last_completed > last_start) {
        return completed_action_id;
    }
    free(completed_action_id);
    return NULL;
}

/* ── _report_port_in_use ────────────────────────────────────────────── */
/* PoP: ws_report_port_in_use @ hermes_cli/web_server.py:_report_port_in_use */
/* Print the port-in-use sentinel and human hint. */
void ws_report_port_in_use(const char *host, int port) {
    printf("BACKEND_PORT_IN_USE port=%d\n", port);
    printf("  Port %d on %s is already in use — likely another "
           "'hermes serve' / 'hermes dashboard' backend or the Hermes gateway. "
           "Stop the other process, or pass --port <other> "
           "(--port 0 picks a free ephemeral port).\n", port, host ? host : "unknown");
}

/* get_public_url() — resolves the dashboard public URL via
 * cli_hermes_cli_dashboard_auth_prefix_resolve_public_url.
 * Returns a static buffer (or NULL when unset). */
const char *get_public_url(void) {
    static char buf[512];
    buf[0] = '\0';
    if (cli_hermes_cli_dashboard_auth_prefix_resolve_public_url(buf, sizeof(buf)) == 0 && buf[0]) {
        return buf;
    }
    return NULL;
}

/* Forward declaration for the public URL resolver in dashboard_auth_prefix.c */
extern int cli_hermes_cli_dashboard_auth_prefix_resolve_public_url(char *output, size_t output_size);

/* ── _dashboard_public_hosts ────────────────────────────────────────── */
/* PoP: ws_dashboard_public_hosts @ hermes_cli/web_server.py:_dashboard_public_hosts */
/* Return the hostname from dashboard.public_url (fail closed as empty string). */
char *ws_dashboard_public_hosts(void) {
    const char *public_url = get_public_url();
    if (!public_url || !*public_url) return strdup("");

    const char *scheme_end = strstr(public_url, "://");
    if (!scheme_end) return strdup("");
    const char *host_start = scheme_end + 3;

    const char *host_end = host_start;
    while (*host_end && *host_end != '/' && *host_end != ':' && *host_end != '?') {
        host_end++;
    }

    size_t len = host_end - host_start;
    if (len == 0) return strdup("");

    char *hostname = (char *)malloc(len + 1);
    if (!hostname) return strdup("");
    memcpy(hostname, host_start, len);
    hostname[len] = '\0';

    for (size_t i = 0; i < len; i++) {
        hostname[i] = (char)tolower((unsigned char)hostname[i]);
    }

    return hostname;
}
