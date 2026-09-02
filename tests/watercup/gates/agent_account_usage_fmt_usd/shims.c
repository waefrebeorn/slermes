/* shim.c for agent_account_usage_fmt_usd — mirrors Python _fmt_usd.
 * Python: _fmt_usd(amount) -> str
 *   - amount is numeric (int/float). Raises TypeError on None.
 *   - float('nan') -> '$nan'
 *   - float('inf') -> '$inf'
 *   - Strings are NOT accepted (raises ValueError).
 *   - Format: f"${amount:,.2f}" with comma grouping.
 *     -99.99 -> '$-99.99'
 *     0.0 -> '$0.00'
 *     1234.56 -> '$1,234.56'
 *     1000000 -> '$1,000,000.00'
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

void account_usage_fmt_usd_str(const char *s, char *buf, size_t sz) {
    /* NULL / empty -> None sentinel (Python raises TypeError on None) */
    if (!s || !*s || strcmp(s, "NULL") == 0) {
        snprintf(buf, sz, "None");
        return;
    }
    /* Parse as double */
    char *endp = NULL;
    double d = strtod(s, &endp);
    if (endp == s || *endp != '\0') {
        /* Not a pure number string -> ValueError sentinel */
        snprintf(buf, sz, "None");
        return;
    }
    if (isnan(d) || isinf(d)) {
        snprintf(buf, sz, "$nan");
        return;
    }
    /* Format as $X,XXX.XX matching Python f"${d:,.2f}" */
    double ipart_d;
    double fpart_d = modf(d, &ipart_d);
    long long ipart = (long long)ipart_d;
    int frac = (int)lround(fpart_d * 100.0);
    if (frac >= 100) { frac -= 100; ipart += 1; }
    if (frac <= -100) { frac += 100; ipart -= 1; }
    int negative = (ipart < 0);
    unsigned long long mag = negative ? (unsigned long long)(-ipart) : (unsigned long long)ipart;
    char intbuf[64];
    int n = 0;
    if (mag == 0) {
        intbuf[n++] = '0';
    } else {
        unsigned long long tmp = mag;
        int digit_count = 0;
        while (tmp > 0) {
            if (digit_count > 0 && digit_count % 3 == 0)
                intbuf[n++] = ',';
            intbuf[n++] = (char)('0' + (int)(tmp % 10));
            digit_count++;
            tmp /= 10;
        }
    }
    /* Reverse */
    for (int i = 0, j = n - 1; i < j; i++, j--) {
        char tmp = intbuf[i];
        intbuf[i] = intbuf[j];
        intbuf[j] = tmp;
    }
    intbuf[n] = '\0';
    if (negative) {
        snprintf(buf, sz, "$-%s.%02d", intbuf, -frac);
    } else {
        snprintf(buf, sz, "$%s.%02d", intbuf, frac);
    }
}
