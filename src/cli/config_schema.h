#ifndef SLERMES_CONFIG_SCHEMA_H
#define SLERMES_CONFIG_SCHEMA_H

#include <stdbool.h>
#include <stdio.h>
#include <json.h>

typedef struct config_schema config_schema_t;

config_schema_t *config_schema_init(void);
void config_schema_cleanup(config_schema_t *s);

json_t *schema_prop(const char *type, const char *desc, const char *default_val);
json_t *schema_prop_int(const char *desc, int def, int min, int max);
json_t *schema_prop_num(const char *desc, double def, double min, double max);
json_t *schema_prop_bool(const char *desc, bool def);
void schema_add_enum(json_t *prop, const char **values, int count);
void exp_str(FILE *f, const char *key, const char *val);
void exp_int(FILE *f, const char *key, int val);
void exp_bool(FILE *f, const char *key, bool val);
void exp_float(FILE *f, const char *key, float val);

#endif /* SLERMES_CONFIG_SCHEMA_H */
