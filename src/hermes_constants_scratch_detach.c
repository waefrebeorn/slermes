/*
 * hermes_constants_scratch_detach.c — real C11 port of
 * hermes_constants_scratch_detach.py (gate w5_hermesconst_e6f362).
 *
 * PoP: hcsdetach_encode_lineage @ hermes_constants_scratch_detach.py:_encode_lineage
 * PoP: hcsdetach_decode_lineage @ hermes_constants_scratch_detach.py:run
 * PoP: hcsdetach_child_script @ hermes_constants_scratch_detach.py:_CHILD
 *
 * The lineage wire format and its float repr (Python repr(float)) are ported
 * exactly; process spawning stays out of the pure gate.
 */
#include "noc_w5_hermesconst_e6f362.h"
#include "hermes_json.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HCSCHILD "-c program is injected by the launcher"

const char *const hcsdetach_child_text =
    "import sys; sys.path[:] = sys.argv[3:]; "
    "import hermes_constants_scratch_detach as d; d.run(*sys.argv[1:3])";

/* Python repr() of a finite double: shortest round-tripping digits, fixed
 * notation for 1e-4 <= |v| < 1e16, scientific otherwise. */
static char *py_float_repr(double v) {
    if (isnan(v)) return strdup("nan");
    if (isinf(v)) return strdup(v > 0 ? "inf" : "-inf");
    int neg = signbit(v) != 0;
    double a = fabs(v);
    if (a == 0.0) return strdup(neg ? "-0.0" : "0.0");
    int p;
    char probe[64];
    for (p = 1; p <= 17; p++) {
        snprintf(probe, sizeof(probe), "%.*g", p, a);
        if (strtod(probe, NULL) == a) break;
    }
    if (p > 17) p = 17;
    char sci[64];
    snprintf(sci, sizeof(sci), "%.*e", p - 1, a);
    char mant[64];
    int exp10 = 0;
    char *ep = strchr(sci, 'e');
    size_t ml = ep ? (size_t)(ep - sci) : strlen(sci);
    memcpy(mant, sci, ml);
    mant[ml] = '\0';
    if (ep) exp10 = atoi(ep + 1);
    char *dot = strchr(mant, '.');
    if (dot) {
        char *end = mant + strlen(mant);
        while (end > dot + 1 && end[-1] == '0') --end;
        *end = '\0';
    }
    char out[160];
    if (exp10 >= 16 || exp10 < -4) {
        snprintf(out, sizeof(out), "%s%se%+03d", neg ? "-" : "", mant, exp10);
    } else {
        char digits[64];
        int nd = 0;
        for (char *q = mant; *q; q++) if (*q != '.') digits[nd++] = *q;
        digits[nd] = '\0';
        char fixed[128];
        if (exp10 >= 0) {
            int intlen = exp10 + 1;
            if (nd <= intlen) {
                snprintf(fixed, sizeof(fixed), "%s", digits);
                for (int i = nd; i < intlen; i++) strcat(fixed, "0");
                strcat(fixed, ".0");
            } else {
                snprintf(fixed, sizeof(fixed), "%.*s.%s", intlen, digits, digits + intlen);
            }
        } else {
            snprintf(fixed, sizeof(fixed), "0.%0*d%s", -exp10 - 1, 0, digits);
        }
        snprintf(out, sizeof(out), "%s%s", neg ? "-" : "", fixed);
    }
    return strdup(out);
}

int hcsdetach_encode_lineage(const char *chain_json, char **out) {
    json_t *chain = json_parse(chain_json ? chain_json : "null", NULL);
    if (!chain) return 0;
    size_t cap = 64, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { json_free(chain); return 0; }
    buf[0] = '\0';
    if (chain->type == JSON_NULL) {
        memcpy(buf, "-", 2); len = 1;
    } else if (chain->type == JSON_ARRAY) {
        for (size_t i = 0; i < chain->c.count; i++) {
            json_t *pair = chain->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) { free(buf); json_free(chain); return 0; }
            json_t *pn = pair->c.items[0], *sn = pair->c.items[1];
            long long pid = 0;
            if (pn && pn->type == JSON_NUMBER) pid = (long long)pn->num_val;
            else if (pn && pn->type == JSON_STRING) pid = strtoll(pn->str_val, NULL, 10);
            double started = (sn && sn->type == JSON_NUMBER) ? sn->num_val
                           : (sn && sn->type == JSON_STRING ? strtod(sn->str_val, NULL) : 0.0);
            char *sr = py_float_repr(started);
            char piece[192];
            snprintf(piece, sizeof(piece), "%s%lld:%s", i ? "," : "", pid, sr ? sr : "0.0");
            free(sr);
            size_t pl = strlen(piece);
            if (len + pl + 1 > cap) {
                while (len + pl + 1 > cap) cap *= 2;
                char *nb = (char *)realloc(buf, cap);
                if (!nb) { free(buf); json_free(chain); return 0; }
                buf = nb;
            }
            memcpy(buf + len, piece, pl + 1);
            len += pl;
        }
    } else {
        free(buf); json_free(chain); return 0;
    }
    json_free(chain);
    *out = buf;
    return 1;
}

int hcsdetach_decode_lineage(const char *lineage, char **out_json) {
    if (!lineage) return 0;
    if (!strcmp(lineage, "-")) {
        *out_json = strdup("null");
        return *out_json ? 1 : 0;
    }
    json_t *arr = json_array();
    if (!arr) return 0;
    const char *p = lineage;
    while (1) {
        const char *comma = strchr(p, ',');
        size_t itemlen = comma ? (size_t)(comma - p) : strlen(p);
        if (itemlen == 0) { json_free(arr); return 0; }
        char item[512];
        if (itemlen >= sizeof(item)) { json_free(arr); return 0; }
        memcpy(item, p, itemlen); item[itemlen] = '\0';
        char *colon = strchr(item, ':');
        if (!colon) { json_free(arr); return 0; }
        *colon = '\0';
        char *endp = NULL;
        long long pid = strtoll(item, &endp, 10);
        while (*endp == ' ') endp++;
        if (endp == item || *endp != '\0') { json_free(arr); return 0; }
        double started = strtod(colon + 1, &endp);
        while (*endp == ' ') endp++;
        if (endp == colon + 1 || *endp != '\0') { json_free(arr); return 0; }
        /* frozenset dedupes identical (pid, started) pairs */
        int dup = 0;
        for (size_t i = 0; i < json_len(arr); i++) {
            json_t *e = json_get(arr, i);
            if (e && json_len(e) >= 2) {
                json_t *a = json_get(e, 0), *b = json_get(e, 1);
                if (a && b && a->num_val == (double)pid && b->num_val == started) { dup = 1; break; }
            }
        }
        if (!dup) {
            json_t *pair = json_array();
            json_append(pair, json_number((double)pid));
            json_append(pair, json_number(started));
            json_append(arr, pair);
        }
        if (!comma) break;
        p = comma + 1;
    }
    *out_json = json_dumps(arr, 0);
    json_free(arr);
    return *out_json ? 1 : 0;
}

char *hcsdetach_child_script(void) {
    (void)HCSCHILD;
    return strdup(hcsdetach_child_text);
}
