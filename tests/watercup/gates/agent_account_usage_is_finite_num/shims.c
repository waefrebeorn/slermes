#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Match Python _is_finite_num: True iff the string represents a real numeric
 * value (int/float, not bool, not NaN/Inf, not a non-numeric string). */
bool account_usage_is_finite_num(const char *s) {
    if (!s || !*s) return false;
    /* Python: isinstance(v, (int,float)) and not isinstance(v,bool)
     * and math.isfinite(v).  We must reject non-numeric strings.
     * atof("hello") = 0.0 which IS finite -> wrong.
     * Check that the string looks like a number first. */
    const char *p = s;
    /* skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return false;
    /* optional sign */
    if (*p == '+' || *p == '-') p++;
    if (!*p) return false;
    /* digits before decimal point (at least one digit required) */
    bool has_digits = false;
    while (isdigit((unsigned char)*p)) { has_digits = true; p++; }
    /* optional fractional part */
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) { has_digits = true; p++; }
    }
    /* optional exponent */
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!*p) return false;
        while (isdigit((unsigned char)*p)) p++;
    }
    /* skip trailing whitespace */
    while (*p == ' ' || *p == '\t') p++;
    if (*p) return false;  /* trailing garbage */
    if (!has_digits) return false;
    /* Now parse as double and check finiteness */
    char *end;
    double v = strtod(s, &end);
    if (end == s) return false;
    return isfinite(v) && !isnan(v);
}
