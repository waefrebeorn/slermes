#ifndef SLERMES_PORT_MODELS_PRICING_H
#define SLERMES_PORT_MODELS_PRICING_H

#include <stddef.h>

/* hermes_cli/models_pricing.py pure helpers. String outputs go into a
 * caller-supplied buffer (64 bytes is ample for any price string). */

void mp_strip_v1(const char *url, char *out, size_t outsz);

/* 1 on success (finite per the positivity rule, NaN always rejected), 0 otherwise. */
int mp_price_float(const char *raw, int positive, double *out);

/* Discount percent, or -1 for "no sale / unusable input". */
int mp_sale_pct(const char *current, const char *original);

void mp_format_price_per_mtok(const char *per_token_str, char *out, size_t outsz);

#endif /* SLERMES_PORT_MODELS_PRICING_H */
