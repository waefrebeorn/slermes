/*
 * tool_output_truncate.c — C port of upstream Hermes tools/tool_output_truncate.py.
 * PoP: touttrunc_notice @ tools/tool_output_truncate.py:truncation_notice
 * PoP: touttrunc_head_tail_split @ tools/tool_output_truncate.py:head_tail_split
 * PoP: touttrunc_head_tail @ tools/tool_output_truncate.py:truncate_head_tail
 */
#include "noc_touttrunc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void touttrunc_comma(long long n, char *buf, size_t cap) {
    char tmp[80], digs[40];
    int len = 0, neg = n < 0, nd = 0;
    unsigned long long u = neg ? (unsigned long long)(-(n + 1)) + 1ULL : (unsigned long long)n;
    if (u == 0) digs[nd++] = '0';
    while (u) { digs[nd++] = (char)('0' + (int)(u % 10)); u /= 10; }
    /* Build least-significant first, inserting a comma every three digits. */
    for (int i = 0; i < nd; i++) {
        tmp[len++] = digs[i];
        if ((i + 1) % 3 == 0 && i + 1 < nd) tmp[len++] = ',';
    }
    if (neg) tmp[len++] = '-';
    tmp[len] = 0;
    size_t k = 0;
    for (int i = len - 1; i >= 0 && k + 1 < cap; i--) buf[k++] = tmp[i];
    buf[k] = 0;
}

char *touttrunc_notice(long long omitted, long long total, const char *label, const char *unit) {
    char a[80], b[80];
    touttrunc_comma(omitted, a, sizeof a);
    touttrunc_comma(total, b, sizeof b);
    size_t cap = strlen(label) + strlen(unit) + strlen(a) + strlen(b) + 128;
    char *out = (char *)malloc(cap);
    snprintf(out, cap, "\n\n... [%s TRUNCATED - %s %s omitted out of %s total] ...\n\n",
             label, a, unit, b);
    return out;
}

void touttrunc_head_tail_split(long long budget, long long *head, long long *tail) {
    long long h = (long long)((double)budget * 0.4);
    *head = h;
    *tail = budget - h;
}

char *touttrunc_head_tail(const char *text, long long max_chars, const char *label) {
    size_t len = strlen(text);
    if ((long long)len <= max_chars) return strdup(text);
    long long h, t;
    touttrunc_head_tail_split(max_chars, &h, &t);
    char *notice = touttrunc_notice((long long)len - h - t, (long long)len, label, "chars");
    size_t cap = strlen(notice) + (size_t)h + (size_t)t + 16;
    char *out = (char *)malloc(cap);
    memcpy(out, text, (size_t)h);
    out[h] = 0;
    strcat(out, notice);
    strcat(out, text + len - (size_t)t);
    free(notice);
    return out;
}
