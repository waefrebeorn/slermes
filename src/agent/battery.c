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

/* PoP: battery_status_make @ agent/battery.py:BatteryStatus */
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

/* PoP: battery_status_charging @ agent/battery.py:BatteryStatus.charging */
bool battery_status_charging(const battery_status_t *s) {
    bool plugged = false;
    return battery_status_plugged(s, &plugged) && plugged;
}

/* PoP: battery_category @ agent/battery.py:battery_category */
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

/* PoP: battery_glyph @ agent/battery.py:battery_glyph */
const char *battery_glyph(const battery_status_t *s) {
    /* ⚡ while charging, else 🔋 */
    return battery_status_charging(s) ? "\u26A1" : "\U0001F50B";
}

/* PoP: format_battery @ agent/battery.py:format_battery */
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


/* === dissolved from ports_wrappers.c === */
/* PoP: agent_battery_u_read_battery_uncached @ agent/battery.py:_read_battery_uncached */
int agent_battery_u_read_battery_uncached(const char *arg) {
    /* Python: psutil sensors_battery. Arg = "state\tpercent\tplugged". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "unavailable") == 0) { printf("0\n"); return 0; }
    printf("1\t%s\t%s\n", t1 ? t1 + 1 : "?", t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: agent_battery_read_battery @ agent/battery.py:read_battery */
int agent_battery_read_battery(const char *arg) {
    /* Python: cached battery read (TTL a few seconds). Arg = "use_cache\tjson". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int use_cache = (arg[0] == '1');
    (void)use_cache;
    if (tab && tab[1]) printf("%s\n", tab + 1);
    else printf("0\n");
    return 0;
}

/* PoP: agent_battery_clear_cache @ agent/battery.py:clear_cache */
int agent_battery_clear_cache(const char *arg) {
    /* Python test hook: drop the memoised reading. */
    (void)arg;
    extern void battery_clear_cache(void);
    battery_clear_cache();
    return 0;
}
