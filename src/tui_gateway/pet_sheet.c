/*
 * pet_sheet.c -- C home for upstream Hermes tui_gateway/pet_sheet.py
 * (wave8 gate w8_petsheet_21b5c1).
 *
 * PoP: w8_petsheet_21b5c1_sheet_revision @ tui_gateway/pet_sheet.py:_pet_sheet_revision
 * PoP: w8_petsheet_21b5c1_clone_payload @ tui_gateway/pet_sheet.py:_clone_pet_payload
 *
 * The stat()/PIL seams are injected: the harness supplies the two stat fields
 * as 64-bit decimal strings plus an explicit failure flag, so no filesystem or
 * image decode is performed here.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_petsheet_21b5c1.h"

json_t *w8_petsheet_21b5c1_sheet_revision(const char *mtime_ns,
                                         const char *size, int fail) {
    char buf[128];
    if (fail)
        return json_string("0:0");
    long long m = strtoll(mtime_ns ? mtime_ns : "0", NULL, 10);
    long long s = strtoll(size ? size : "0", NULL, 10);
    snprintf(buf, sizeof(buf), "%lld:%lld", m, s);
    return json_string(buf);
}

json_t *w8_petsheet_21b5c1_clone_payload(const json_t *payload) {
    json_t *out = json_object();
    if (!payload || payload->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(payload); i++) {
        const char *key = json_object_get_key_at(payload, i);
        json_t *val = json_object_get_at(payload, i);
        if (!key) continue;
        int typed = (!strcmp(key, "framesByState") || !strcmp(key, "framesByRow"))
                        ? (val && val->type == JSON_OBJECT)
                        : (!strcmp(key, "stateRows") ? (val && val->type == JSON_ARRAY) : 0);
        /* Python: out[key] = kind(payload[key]) for the typed keys, otherwise
         * the value survives the dict() shallow clone unchanged.  At the JSON
         * value level both are simply a copy of the original value. */
        (void)typed;
        json_set(out, key, json_copy(val));
    }
    return out;
}
