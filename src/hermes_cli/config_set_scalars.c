/*
 * config_set_scalars.c — faithful C11 port of hermes_cli/config_set_scalars.py
 * (gate w17_configset_671393).
 *
 * PoP: w17_configset_671393_coerce_int   @ hermes_cli/config_set_scalars.py:_coerce_int
 * PoP: w17_configset_671393_coerce_float @ hermes_cli/config_set_scalars.py:_coerce_float
 * PoP: w17_configset_671393_quoted_bool  @ hermes_cli/config_set_scalars.py:_quoted_bool
 *
 * int()/float()/Decimal() semantics are reproduced for decimal literals:
 * surrounding whitespace, an optional sign, underscores between digits, and
 * rejection of NaN/inf or of a decimal more precise than the round-tripping
 * binary float (Decimal(value) != Decimal(str(f))).
 */
#include "noc_w17_configset_671393.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Python int(value) for a base-10 literal. Returns 1 and *out on success. */
static int parse_int_literal(const char *s, long long *out) {
    char buf[128];
    size_t n = 0;
    int neg = 0, prev_us = 1;
    long long v;
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
    if (!*s) return 0;
    for (; *s && !isspace((unsigned char)*s); s++) {
        if (*s == '_') {
            if (n == 0 || prev_us) return 0;
            prev_us = 1;
        } else if (*s >= '0' && *s <= '9') {
            if (n >= sizeof(buf) - 1) return 0;
            buf[n++] = *s;
            prev_us = 0;
        } else {
            return 0;
        }
    }
    while (isspace((unsigned char)*s)) s++;
    if (*s || n == 0 || prev_us) return 0;
    buf[n] = '\0';
    errno = 0;
    v = strtoll(buf, NULL, 10);
    if (errno == ERANGE) return 0;      /* Python big ints are out of C's range */
    *out = neg ? -v : v;
    return 1;
}

json_t *w17_configset_671393_coerce_int(const char *value) {
    long long v;
    if (!parse_int_literal(value, &v)) return json_null();
    return json_number((double)v);
}

/* A normalised decimal literal: sign, digit string (no leading/trailing zeros), exp10. */
typedef struct { int neg; char digits[96]; int exp; } Dec;

static int dec_parse(const char *s, Dec *d) {
    char clean[160];
    size_t cn = 0, i, nd = 0;
    int frac = 0, exp10 = 0, saw_digit = 0, prev_us = 1;
    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;
    memset(d, 0, sizeof(*d));
    if (*s == '+' || *s == '-') { d->neg = (*s == '-'); s++; }
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (isspace(c)) break;
        if (c == '_') {
            if (!saw_digit || prev_us) return 0;
            prev_us = 1;
            continue;
        }
        if (c >= '0' && c <= '9') {
            if (cn >= sizeof(clean) - 1) return 0;
            clean[cn++] = (char)c;
            saw_digit = 1;
            prev_us = 0;
            if (frac) exp10--;
        } else if (c == '.' && !frac) {
            frac = 1;
            prev_us = 1;
        } else if ((c == 'e' || c == 'E') && saw_digit) {
            int esign = 1, eval = 0;
            s++;
            if (*s == '+' || *s == '-') { esign = (*s == '-') ? -1 : 1; s++; }
            if (*s < '0' || *s > '9') return 0;
            while (*s >= '0' && *s <= '9') { eval = eval * 10 + (*s - '0'); s++; }
            exp10 += esign * eval;
            s--;
        } else {
            return 0;
        }
    }
    while (isspace((unsigned char)*s)) s++;
    if (*s || !saw_digit) return 0;
    for (i = 0; i < cn; i++) if (clean[i] != '0') break;
    if (i == cn) { d->neg = 0; d->digits[0] = '0'; d->digits[1] = '\0'; d->exp = 0; return 1; }
    for (; i < cn; i++) {
        if (nd >= sizeof(d->digits) - 1) return 0;
        d->digits[nd++] = clean[i];
    }
    d->digits[nd] = '\0';
    while (nd > 1 && d->digits[nd - 1] == '0') { d->digits[--nd] = '\0'; d->exp++; }
    d->exp += exp10;
    return 1;
}

static int dec_equal(const Dec *a, const Dec *b) {
    char aa[160], bb[160];
    size_t la, lb, i;
    if (a->neg != b->neg) return 0;
    if (strcmp(a->digits, b->digits)) return 0;
    la = strlen(a->digits); lb = la;
    strcpy(aa, a->digits); strcpy(bb, b->digits);
    if (a->exp > b->exp) {
        size_t k = (size_t)(a->exp - b->exp);
        if (k + lb >= sizeof(bb)) return 0;
        for (i = 0; i < k; i++) bb[lb++] = '0';
        bb[lb] = '\0';
    } else if (b->exp > a->exp) {
        size_t k = (size_t)(b->exp - a->exp);
        if (k + la >= sizeof(aa)) return 0;
        for (i = 0; i < k; i++) aa[la++] = '0';
        aa[la] = '\0';
    }
    return !strcmp(aa, bb);
}

json_t *w17_configset_671393_coerce_float(const char *value) {
    char clean[256];
    size_t cn = 0, i;
    double f, chk;
    int p;
    Dec in, rp;
    if (!value) return json_null();
    /* Strip whitespace and underscores for strtod (Python float() accepts both). */
    for (i = 0; value[i]; i++) {
        unsigned char c = (unsigned char)value[i];
        if (c == '_') continue;
        if (isspace(c) && cn == 0) continue;
        if (isspace(c)) break;
        if (cn >= sizeof(clean) - 1) return json_null();
        clean[cn++] = (char)c;
    }
    clean[cn] = '\0';
    while (cn > 0 && isspace((unsigned char)clean[cn - 1])) clean[--cn] = '\0';
    if (!cn) return json_null();
    errno = 0;
    f = strtod(clean, NULL);
    if (errno == ERANGE || isnan(f) || isinf(f)) return json_null();
    if (!dec_parse(value, &in)) return json_null();
    /* Smallest %.pg that round-trips is C's equivalent of Python str(f). */
    for (p = 1; p <= 17; p++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*g", p, f);
        chk = strtod(buf, NULL);
        if (chk == f) {
            if (!dec_parse(buf, &rp)) return json_null();
            return dec_equal(&in, &rp) ? json_number(f) : json_null();
        }
    }
    return json_null();
}

json_t *w17_configset_671393_quoted_bool(const char *stripped) {
    size_t n, a, b, i;
    char word[64];
    if (!stripped) return json_null();
    n = strlen(stripped);
    if (n < 2 || stripped[0] != stripped[n - 1]) return json_null();
    if (stripped[0] != '\'' && stripped[0] != '"') return json_null();
    a = 1; b = n - 1;
    while (a < b && isspace((unsigned char)stripped[a])) a++;
    while (b > a && isspace((unsigned char)stripped[b - 1])) b--;
    if (b - a >= sizeof(word)) return json_null();
    for (i = a; i < b; i++) word[i - a] = (char)tolower((unsigned char)stripped[i]);
    word[b - a] = '\0';
    if (!strcmp(word, "true") || !strcmp(word, "yes") || !strcmp(word, "on")) return json_bool(1);
    if (!strcmp(word, "false") || !strcmp(word, "no") || !strcmp(word, "off")) return json_bool(0);
    return json_null();   /* quoted null/none/~ and every other word are not bools */
}
