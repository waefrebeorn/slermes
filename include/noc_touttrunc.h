#ifndef NOC_TOUTTRUNC_H
#define NOC_TOUTTRUNC_H
#include <stddef.h>
void    touttrunc_comma(long long n, char *buf, size_t cap);
char   *touttrunc_notice(long long omitted, long long total, const char *label, const char *unit);
void    touttrunc_head_tail_split(long long budget, long long *head, long long *tail);
char   *touttrunc_head_tail(const char *text, long long max_chars, const char *label);
#endif
