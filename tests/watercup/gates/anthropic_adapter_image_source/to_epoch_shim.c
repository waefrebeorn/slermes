/* Gate-local shim: auth.c reuses kanban_db's to_epoch; provide a faithful
   copy (same minimal ISO/int/float parser) without dragging the whole
   kanban_db + sqlite into the harness link. */
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
long to_epoch(const char *val)
{
    if (!val) return -1;
    char *end = NULL;
    long as_long = strtol(val, &end, 10);
    if (end != val && *end == '\0') return as_long;
    double as_double = strtod(val, &end);
    if (end != val && *end == '\0') return (long)as_double;
    while (*val && isspace((unsigned char)*val)) val++;
    size_t n = strlen(val);
    while (n > 0 && isspace((unsigned char)val[n - 1])) n--;
    if (n == 0) return -1;
    char buf[64];
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, val, n); buf[n] = '\0';
    int Y, M, D, h = 0, m = 0, s = 0, tz_h = 0, tz_m = 0;
    char sep1, sep2;
    int got = sscanf(buf, "%d-%d-%d%c%d:%d:%d%c%d:%d",
                     &Y, &M, &D, &sep1, &h, &m, &s, &sep2, &tz_h, &tz_m);
    if (got >= 7) {
        int yy = Y - (M <= 2 ? 1 : 0);
        int era = (yy >= 0 ? yy : yy - 399) / 400;
        long yoe = (long)(yy - era * 400);
        long doy = (153 * (M + (M > 2 ? -3 : 9)) + 2) / 5 + D - 1;
        long doe = (era * 365L + yoe / 4 - yoe / 100) * 1L + doy;
        long days = era * 146097L + doe - 719468L;
        long secs = days * 86400L + h * 3600L + m * 60L + s - (tz_h * 3600L + tz_m * 60L);
        return secs;
    }
    return -1;
}
