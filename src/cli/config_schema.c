/*
 * config_schema.c — extracted concern module from config.c.
 * Self-contained, opaque struct, minimal includes, C11.
 */

#include "config_schema.h"
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

struct config_schema { int unused; };
config_schema_t *config_schema_init(void) { return calloc(1, sizeof(config_schema_t)); }
void config_schema_cleanup(config_schema_t *s) { free(s); }

/* Helper: create schema property definition */
json_t *schema_prop(const char *type, const char *desc, const char *default_val) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string(type));
    if (desc && desc[0]) json_set(prop, "description", json_string(desc));
    if (default_val && default_val[0]) json_set(prop, "default", json_string(default_val));
    return prop;
}

json_t *schema_prop_int(const char *desc, int def, int min, int max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("integer"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number((double)def));
    json_set(prop, "minimum", json_number((double)min));
    json_set(prop, "maximum", json_number((double)max));
    return prop;
}

json_t *schema_prop_num(const char *desc, double def, double min, double max) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("number"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_number(def));
    json_set(prop, "minimum", json_number(min));
    json_set(prop, "maximum", json_number(max));
    return prop;
}

json_t *schema_prop_bool(const char *desc, bool def) {
    json_t *prop = json_object();
    json_set(prop, "type", json_string("boolean"));
    if (desc) json_set(prop, "description", json_string(desc));
    json_set(prop, "default", json_bool(def));
    return prop;
}

void schema_add_enum(json_t *prop, const char **values, int count) {
    json_t *arr = json_array();
    for (int i = 0; i < count; i++)
        json_append(arr, json_string(values[i]));
    json_set(prop, "enum", arr);
}

/* Helper: write a config value line */
void exp_str(FILE *f, const char *key, const char *val) {
    if (val && val[0]) fprintf(f, "%s: %s\n", key, val);
}

void exp_int(FILE *f, const char *key, int val) {
    fprintf(f, "%s: %d\n", key, val);
}

void exp_bool(FILE *f, const char *key, bool val) {
    fprintf(f, "%s: %s\n", key, val ? "true" : "false");
}

void exp_float(FILE *f, const char *key, float val) {
    fprintf(f, "%s: %.2f\n", key, (double)val);
}

