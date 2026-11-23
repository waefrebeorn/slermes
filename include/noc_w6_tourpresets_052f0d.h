#ifndef NOC_W6_TOURPRESETS_052F0D_H
#define NOC_W6_TOURPRESETS_052F0D_H

#include "hermes_json.h"

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: tourpresets_is_valid @ tools/tour_presets.py:TourPreset
 * TourPreset is a StrEnum with members quick="quick" and full="full".
 * Returns 1 iff `name` is exactly one of those values (case-sensitive). */
int tourpresets_is_valid(const char *name);

/* PoP: tourpresets_coerce @ tools/tour_presets.py:TourPreset
 * Equivalent to TourPreset(name).value: returns a borrowed static string
 * "quick"/"full" when valid, NULL otherwise (ValueError in Python). */
const char *tourpresets_coerce(const char *name);

/* PoP: tourpresets_values @ tools/tour_presets.py:TourPreset
 * Returns ["quick", "full"] in definition order (caller frees). */
struct json_t *tourpresets_values(void);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_TOURPRESETS_052F0D_H */
