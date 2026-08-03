/*
 * battery.c — sysfs battery reader (fills the read_battery REAL_GAP).
 *
 * agent/battery.py's read_battery/_read_battery_uncached read the host via
 * psutil (sensors_battery). This C11 module reads the SAME source
 * (/sys/class/power_supply/*) directly, with the same time-based cache.
 * The model + category/glyph/format helpers live in port_battery.c; this
 * file only produces battery_status_t values via the public API.
 */

#define _POSIX_C_SOURCE 200809L
#include "battery.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BATTERY_CACHE_TTL_SECONDS 5

/* Cache of the last reading (mirrors Python's _cache (ts, status)). */
static struct {
    bool has;
    int  percent;
    bool plugged;
    time_t ts;
} g_cache = {false, -1, false, 0};

/* ── raw read ───────────────────────────────────────────────────────── */

static int read_int_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    int v = -1;
    if (fscanf(f, "%d", &v) != 1) v = -1;
    fclose(f);
    return v;
}

/* Returns true if a battery was found. */
static bool read_uncached(int *percent_out, bool *plugged_out) {
    *percent_out = -1;
    *plugged_out = false;

    DIR *d = opendir("/sys/class/power_supply");
    if (!d) return false;

    struct dirent *de;
    bool found = false;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char type_path[512];
        snprintf(type_path, sizeof(type_path),
                 "/sys/class/power_supply/%s/type", de->d_name);
        char type[64] = "";
        FILE *tf = fopen(type_path, "r");
        if (tf) {
            if (fgets(type, sizeof(type), tf)) {
                size_t n = strlen(type);
                while (n && (type[n-1] == '\n' || type[n-1] == '\r')) type[--n] = '\0';
            }
            fclose(tf);
        }
        if (strcmp(type, "Battery") != 0) continue;

        char cap_path[512];
        snprintf(cap_path, sizeof(cap_path),
                 "/sys/class/power_supply/%s/capacity", de->d_name);
        int pct = read_int_file(cap_path);
        if (pct < 0) continue; /* not readable — skip */

        found = true;
        *percent_out = pct > 100 ? 100 : pct;

        /* Charging state from the status file. */
        char status_path[512];
        snprintf(status_path, sizeof(status_path),
                 "/sys/class/power_supply/%s/status", de->d_name);
        char status[32] = "";
        FILE *sf = fopen(status_path, "r");
        if (sf) {
            if (fgets(status, sizeof(status), sf)) {
                size_t n = strlen(status);
                while (n && (status[n-1] == '\n' || status[n-1] == '\r')) status[--n] = '\0';
            }
            fclose(sf);
        }
        *plugged_out = (strcmp(status, "Charging") == 0 || strcmp(status, "Full") == 0);
        break; /* first Battery supply wins */
    }
    closedir(d);
    return found;
}

/* ── public API ─────────────────────────────────────────────────────── */

/* PoP: read_battery @ agent/battery.py:read_battery */
battery_status_t *battery_read(bool use_cache) {
    time_t now = time(NULL);
    if (use_cache && g_cache.has && (now - g_cache.ts) < BATTERY_CACHE_TTL_SECONDS) {
        int pct = g_cache.percent;
        bool plugged = g_cache.plugged;
        return battery_status_make(true, &pct, &plugged);
    }
    int pct = -1;
    bool plugged = false;
    bool found = read_uncached(&pct, &plugged);
    if (!found) {
        /* Unavailable singleton. */
        return battery_status_make(false, NULL, NULL);
    }
    g_cache.has = true;
    g_cache.percent = pct;
    g_cache.plugged = plugged;
    g_cache.ts = now;
    return battery_status_make(true, &pct, &plugged);
}

/* PoP: clear_cache @ agent/battery.py:clear_cache */
void battery_clear_cache(void) {
    g_cache.has = false;
    g_cache.percent = -1;
    g_cache.plugged = false;
    g_cache.ts = 0;
}
