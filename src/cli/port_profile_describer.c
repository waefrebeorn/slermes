/*
 * port_profile_describer.c — pure helper (see header).
 * MAX_SKILLS_FOR_PROMPT = 60.
 */

#include "port_profile_describer.h"
#include "hermes_json.h"

#include <stdlib.h>

char *pd_sample_skills(const char *names_json)
{
    json_t *arr = names_json ? json_parse(names_json, NULL) : NULL;
    json_t *out = NULL;
    char *ser;

    if (!arr || arr->type != JSON_ARRAY) {
        if (arr) json_free(arr);
        return strdup("[]");
    }
    if (arr->c.count <= 60) {
        ser = json_serialize(arr);
        json_free(arr);
        return ser;
    }
    {
        double step = (double)arr->c.count / 60.0;
        out = json_array();
        for (int i = 0; i < 60; i++) {
            int idx = (int)(i * step);
            const json_t *it = arr->c.items[idx];
            /* clone the item string */
            char *s = json_serialize(it);
            json_t *copy = s ? json_parse(s, NULL) : NULL;
            if (copy) json_array_append(out, copy);
            free(s);
        }
        ser = json_serialize(out);
        json_free(out);
        json_free(arr);
        return ser;
    }
}
