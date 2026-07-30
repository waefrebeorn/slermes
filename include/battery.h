/* battery.h — opaque interface for the faithful C11 port of
 * hermes-agent/agent/battery.py
 *
 * Compact, colour-coded battery read-out for the CLI/TUI status bar.
 * Pure logic only (no IO/psutil): the reading source (read_battery /
 * _read_battery_uncached / clear_cache) is NOT ported — it reads the host
 * through psutil and a time-based cache, so it remains an honest REAL_GAP.
 * This header exposes the battery_status_t model and the pure classification
 * / formatting helpers that operate on an already-populated status.
 *
 * Opaque struct: battery_status_t is defined only in the .c; consumers use
 * the accessors below. Self-contained; minimal includes.
 */

#ifndef SLERMES_BATTERY_H
#define SLERMES_BATTERY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Colour buckets (mirror the status-bar context styles, inverted: full=good). */
#define BATTERY_CATEGORY_GOOD     "good"
#define BATTERY_CATEGORY_WARN     "warn"
#define BATTERY_CATEGORY_BAD      "bad"
#define BATTERY_CATEGORY_CRITICAL "critical"
#define BATTERY_CATEGORY_DIM      "dim"

/* Opaque battery reading. */
typedef struct battery_status battery_status_t;

/* Build a status. available=false -> the unavailable singleton semantics.
 * percent may be NULL (unknown); plugged may be NULL (platform can't tell).
 * The returned object is caller-owned (free with battery_status_free). */
battery_status_t *battery_status_make(bool available, const int *percent,
                                      const bool *plugged);
void battery_status_free(battery_status_t *s);

bool  battery_status_available(const battery_status_t *s);
/* Returns false (and *out=0) when percent is unknown. */
bool  battery_status_percent(const battery_status_t *s, int *out);
/* Returns false (and *out=false) when plugged is unknown. */
bool  battery_status_plugged(const battery_status_t *s, bool *out);
/* charging == bool(plugged) (plugged None -> not charging). */
bool  battery_status_charging(const battery_status_t *s);

/* Bucket a reading into a colour category (returns a string literal). */
const char *battery_category(const battery_status_t *s);

/* Leading glyph: bolt while charging, else battery. (UTF-8 string literal.) */
const char *battery_glyph(const battery_status_t *s);

/* Compact label like "🔋 82%" / "⚡ 82%" (empty string "" when N/A).
 * Caller frees. */
char *format_battery(const battery_status_t *s);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BATTERY_H */
