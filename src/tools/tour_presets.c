/*
 * tour_presets.c — real C11 port of upstream Hermes tools/tour_presets.py.
 *
 * PoP: tourpresets_is_valid @ tools/tour_presets.py:TourPreset
 * PoP: tourpresets_coerce @ tools/tour_presets.py:TourPreset
 * PoP: tourpresets_values @ tools/tour_presets.py:TourPreset
 *
 * The whole module is one StrEnum; these functions expose its lookup,
 * membership and iteration behaviour with no runtime dependency on Python.
 */
#include "noc_w6_tourpresets_052f0d.h"

#include <string.h>

static const char *const TOURPRESET_VALUES[] = {"quick", "full"};
static const int TOURPRESET_COUNT = 2;

int tourpresets_is_valid(const char *name) {
    if (!name) return 0;
    for (int i = 0; i < TOURPRESET_COUNT; i++)
        if (!strcmp(name, TOURPRESET_VALUES[i])) return 1;
    return 0;
}

const char *tourpresets_coerce(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < TOURPRESET_COUNT; i++)
        if (!strcmp(name, TOURPRESET_VALUES[i])) return TOURPRESET_VALUES[i];
    return NULL;
}

struct json_t *tourpresets_values(void) {
    struct json_t *out = json_array();
    for (int i = 0; i < TOURPRESET_COUNT; i++)
        json_append(out, json_string(TOURPRESET_VALUES[i]));
    return out;
}
