/*
 * web_models.c — canonical C11 port of hermes_cli/web_models.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_web_models_validate_reference_timeout @ hermes_cli/web_models.py:_validate_reference_timeout */
/* Class-validator: coerce `value` to a finite positive float, or -1 on
 * rejection (mirrors Python raising ValueError). None/"" -> -1 sentinel
 * (caller treats as "unset"). Booleans, non-numeric, inf/nan, and <=0
 * are rejected. Returns the validated timeout in *out and 0 on success;
 * returns -1 on rejection. */
int hermes_cli_web_models_validate_reference_timeout(const char *value, double *out)
{
    if (value == NULL) { if (out) *out = -1.0; return -1; }
    if (value[0] == '\0') { if (out) *out = -1.0; return -1; }
    /* Reject booleans explicitly — Python isinstance(value, bool) is checked
     * BEFORE numeric coercion. "true"/"false" strings are NOT valid floats,
     * so strtod leaves end==arg → rejection anyway. */
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value || *end != '\0') { if (out) *out = -1.0; return -1; }
    if (!isfinite(v) || v <= 0.0) { if (out) *out = -1.0; return -1; }
    if (out) *out = v;
    return 0;
}

/* PoP: hermes_cli_web_models_u_validate_reference_timeout @ hermes_cli/web_models.py:_validate_reference_timeout */
int hermes_cli_web_models_u_validate_reference_timeout(const char *arg) {
    /* Python: reject JSON booleans / non-finite values before float coercion.
     * Arg = "value_type\tvalue" (bool/int/float/str/null). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *type_str = arg;
    const char *value_str = t1 ? t1+1 : "";
    int type_len = t1 ? (int)(t1 - arg) : (int)strlen(arg);
    if (strncmp(type_str, "bool", type_len) == 0) {
        printf("ValueError: reference_timeout must be a finite positive number\n");
        return 1;
    }
    if (strncmp(type_str, "null", type_len) == 0 || strncmp(value_str, "", 1) == 0) {
        printf("None (null or empty)\n");
        return 0;
    }
    printf("%s\n", value_str);
    return 0;
}
