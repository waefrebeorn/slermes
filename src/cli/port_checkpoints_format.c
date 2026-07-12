/* Slermes C port — hermes_cli/checkpoints.py (pure format helpers) */

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: hermes_cli_checkpoints__fmt_bytes @ hermes_cli/checkpoints.py:_fmt_bytes */
void hermes_cli_checkpoints_fmt_bytes(long n, char *out, size_t outsz)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = (double)(n > 0 ? n : 0);
    int i = 0;
    for (i = 0; i < 4; i++) {
        if (size < 1024.0) break;
        size /= 1024.0;
    }
    if (i == 0) snprintf(out, outsz, "%ld %s", (long)(n > 0 ? n : 0), units[0]);
    else snprintf(out, outsz, "%.1f %s", size, units[i]);
}

/* PoP: hermes_cli_checkpoints__fmt_age @ hermes_cli/checkpoints.py:_fmt_age
 * now is passed in (Python reads time.time() internally; we expose it for testability). */
void hermes_cli_checkpoints_fmt_age(double ts, double now, char *out, size_t outsz)
{
    if (ts != ts) { snprintf(out, outsz, "—"); return; }  /* NaN guard */
    double age = now - ts;
    if (age != age) { snprintf(out, outsz, "—"); return; }
    if (age < 0) { snprintf(out, outsz, "now"); return; }
    if (age < 60) { snprintf(out, outsz, "%ds ago", (int)age); return; }
    if (age < 3600) { snprintf(out, outsz, "%dm ago", (int)(age / 60)); return; }
    if (age < 86400) { snprintf(out, outsz, "%dh ago", (int)(age / 3600)); return; }
    snprintf(out, outsz, "%dd ago", (int)(age / 86400));
}
