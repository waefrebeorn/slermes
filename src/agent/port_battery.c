/* port_battery.c — faithful C11 port of agent/battery.py
 *
 * Pure-logic battery read-out for the CLI/TUI status bar. Ports the
 * BatteryStatus model, battery_category(), battery_glyph() and
 * format_battery(). The reading source (read_battery / _read_battery_uncached
 * / clear_cache) reads the host via psutil + a time-based cache and is NOT
 * ported (honest REAL_GAP).
 */

#include "battery.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* UNAVAILABLE singleton semantics: available=false, no percent/plugged. */
struct battery_status {
    bool  available;
    bool  has_percent;
    int   percent;        /* valid iff has_percent */
    bool  has_plugged;
    bool  plugged;        /* valid iff has_plugged */
};

/* PoP: battery_battery_status_make @ agent/battery.py:BatteryStatus */
battery_status_t *battery_status_make(bool available, const int *percent,
                                      const bool *plugged) {
    battery_status_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->available = available;
    if (percent) { s->has_percent = true; s->percent = *percent; }
    if (plugged) { s->has_plugged = true; s->plugged = *plugged; }
    return s;
}

void battery_status_free(battery_status_t *s) { free(s); }

bool battery_status_available(const battery_status_t *s) {
    return s ? s->available : false;
}

bool battery_status_percent(const battery_status_t *s, int *out) {
    if (s && s->has_percent) { *out = s->percent; return true; }
    *out = 0; return false;
}

bool battery_status_plugged(const battery_status_t *s, bool *out) {
    if (s && s->has_plugged) { *out = s->plugged; return true; }
    *out = false; return false;
}

/* PoP: battery_BatteryStatus_charging @ agent/battery.py:BatteryStatus.charging */
bool battery_status_charging(const battery_status_t *s) {
    bool plugged = false;
    return battery_status_plugged(s, &plugged) && plugged;
}

/* PoP: battery_battery_category @ agent/battery.py:battery_category */
const char *battery_category(const battery_status_t *s) {
    if (!battery_status_available(s)) return BATTERY_CATEGORY_DIM;
    int pct = 0;
    if (!battery_status_percent(s, &pct)) return BATTERY_CATEGORY_DIM;
    if (battery_status_charging(s)) return BATTERY_CATEGORY_GOOD;
    if (pct <= 10) return BATTERY_CATEGORY_CRITICAL;
    if (pct <= 20) return BATTERY_CATEGORY_BAD;
    if (pct <= 50) return BATTERY_CATEGORY_WARN;
    return BATTERY_CATEGORY_GOOD;
}

/* PoP: battery_battery_glyph @ agent/battery.py:battery_glyph */
const char *battery_glyph(const battery_status_t *s) {
    /* ⚡ while charging, else 🔋 */
    return battery_status_charging(s) ? "\u26A1" : "\U0001F50B";
}

/* PoP: battery_format_battery @ agent/battery.py:format_battery */
char *format_battery(const battery_status_t *s) {
    if (!s || !battery_status_available(s)) return strdup("");
    int pct = 0;
    if (!battery_status_percent(s, &pct)) return strdup("");
    const char *glyph = battery_glyph(s);
    char *out = malloc(32);
    if (!out) return NULL;
    snprintf(out, 32, "%s %d%%", glyph, pct);
    return out;
}

/* Alias for callers that used the older name. */
char *battery_format(const battery_status_t *s) { return format_battery(s); }
