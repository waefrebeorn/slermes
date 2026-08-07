/*
 * billing_pure.h — Port of agent/billing_view.py pure helpers.
 */
#ifndef BILLING_PURE_H
#define BILLING_PURE_H

/* PoP: _parse_payment_method @ agent/billing_view.py:_parse_payment_method */
char *ts_parse_payment_method(const char *raw_json);

#endif
