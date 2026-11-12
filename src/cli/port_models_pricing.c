/*
 * port_models_pricing.c — Port of hermes_cli/models_pricing.py:
 * the pure pricing helpers (float validation, sale percentage, $/MTok
 * formatting, /v1 stripping). The network/cache half of that module is not
 * ported here; these are the stdlib-only pieces.
 */

#include "port_models_pricing.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python 3 round(): banker's rounding (half to even), not C's half-away-from-zero. */
static int py_round(double x)
{
    double f = floor(x);
    double diff = x - f;
    if (diff < 0.5)
        return (int)f;
    if (diff > 0.5)
        return (int)f + 1;
    return fmod(f, 2.0) == 0.0 ? (int)f : (int)f + 1;
}

static double py_round_to(double x, int prec)
{
    double scale = pow(10.0, (double)prec);
    return py_round(x * scale) / scale;
}

/* float(s): leading/trailing whitespace allowed, whole string must parse. */
static int lenient_double(const char *s, double *out)
{
    if (!s)
        return 0;
    const char *p = s;
    while (isspace((unsigned char)*p))
        p++;
    if (!*p)
        return 0;
    char *end = NULL;
    double n = strtod(p, &end);
    if (end == p)
        return 0;
    while (isspace((unsigned char)*end))
        end++;
    if (*end)
        return 0;
    if (out)
        *out = n;
    return 1;
}

/* PoP: _strip_v1 @ hermes_cli/models_pricing.py:_strip_v1 */
void mp_strip_v1(const char *url, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    const char *u = url ? url : "";
    size_t n = strlen(u);
    if (n >= 3 && strcmp(u + n - 3, "/v1") == 0) {
        n -= 3;
        while (n > 0 && u[n - 1] == '/')
            n--;
    }
    if (n >= outsz)
        n = outsz - 1;
    memcpy(out, u, n);
    out[n] = '\0';
}

/* PoP: _price_float @ hermes_cli/models_pricing.py:_price_float */
int mp_price_float(const char *raw, int positive, double *out)
{
    double n;
    if (!lenient_double(raw, &n))
        return 0;
    if (n != n || (positive ? n <= 0 : n < 0))
        return 0;
    if (out)
        *out = n;
    return 1;
}

/* PoP: _sale_pct @ hermes_cli/models_pricing.py:_sale_pct  (-1 == None) */
int mp_sale_pct(const char *current, const char *original)
{
    double cur, orig;
    if (!mp_price_float(current, 1, &cur) || !mp_price_float(original, 1, &orig))
        return -1;
    if (cur >= orig)
        return -1;
    return py_round((1.0 - (cur / orig)) * 100.0);
}

/* PoP: _format_price_per_mtok @ hermes_cli/models_pricing.py:_format_price_per_mtok */
void mp_format_price_per_mtok(const char *per_token_str, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    out[0] = '\0';

    double val;
    if (!lenient_double(per_token_str, &val)) {
        snprintf(out, outsz, "?");
        return;
    }
    if (val == 0) {
        snprintf(out, outsz, "free");
        return;
    }
    double per_m = val * 1000000.0;
    char text[64];
    if (isnan(per_m)) {
        snprintf(out, outsz, "$nan");
        return;
    }
    snprintf(text, sizeof(text), "%.2f", per_m);
    if (per_m < 0.01) {
        int prec = 3;
        while (prec < 12 && py_round_to(per_m, prec) == 0)
            prec++;
        int width = prec + 1 > 12 ? 12 : prec + 1;
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%.*f", width, per_m);
        size_t l = strlen(tmp);
        while (l > 0 && tmp[l - 1] == '0')
            tmp[--l] = '\0';
        if (l > 0 && tmp[l - 1] == '.')
            tmp[--l] = '\0';
        snprintf(text, sizeof(text), "%s", tmp);
    }
    snprintf(out, outsz, "$%s", text);
}
