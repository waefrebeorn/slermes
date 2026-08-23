/* gate: hermes_cli_models_compute_sale_discount — traced copy of the FIXED
 * src/hermes_cli/models_py_helpers.c::models_compute_sale_discount.
 * LIVE twin: hermes_cli/models.py::compute_sale_discount.
 *
 * Input format (single argv token per case): "cp|cc|wp|wc"
 *   cp/cc = current prompt/completion price strings ("" = empty)
 *   wp/wc = original dict prompt/completion ("-" on BOTH = original is not a
 *           dict at all)
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: FREE/$0 hit            -> VSTR(free|100|wp|wc)
 *   B2: original is a dict     -> stop (none) if false
 *   B3: both was-* empty       -> stop (none) if true
 *   B4: prompt cheaper         -> B5 pct>=1 -> sale
 *   B6: completion cheaper     -> B7 pct>=1 -> sale
 *   VSTR(sale:<pct>) / VSTR(none)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "watercup.h"

static double m_finite(const char *raw) {
    if (!raw || !raw[0]) return -1.0;
    char *end = NULL;
    double n = strtod(raw, &end);
    if (end == raw || *end != '\0') return -1.0;
    if (!(n > 0.0) || n != n) return -1.0;
    return n;
}
static double m_nonneg(const char *raw) {
    if (!raw || !raw[0]) return -1.0;
    char *end = NULL;
    double n = strtod(raw, &end);
    if (end == raw || *end != '\0') return -1.0;
    if (!(n >= 0.0) || n != n) return -1.0;
    return n;
}

static void wc_case(char *input) {
    WCUP_BEGIN("models_compute_sale_discount");
    /* parse cp|cc|wp|wc */
    char *f[4] = {NULL, NULL, NULL, NULL};
    int nf = 0;
    f[nf++] = input;
    for (char *p = input; *p && nf < 4; p++)
        if (*p == '|') { *p = '\0'; f[nf++] = p + 1; }
    const char *cp = f[0], *cc = f[1];
    int have_dict = !(nf >= 4 && strcmp(f[2], "-") == 0 && strcmp(f[3], "-") == 0);
    const char *wp = have_dict ? f[2] : NULL;
    const char *wc = have_dict ? f[3] : NULL;
    if (wp && strcmp(wp, "") == 0) wp = NULL;
    if (wc && strcmp(wc, "") == 0) wc = NULL;

    char label[256];

    /* B1: FREE/$0 first (LIVE order) */
    {
        double cur_c_any = cc ? m_nonneg(cc) : -1.0;
        int p_zero = (cp && cp[0] && m_nonneg(cp) == 0.0);
        int c_zero_or_none = (!cc || !cc[0] || cur_c_any <= 0.0);
        int free_hit = (p_zero && c_zero_or_none);
        WCUP_BRANCH(free_hit);
        if (free_hit) {
            snprintf(label, sizeof label, "free|%d|%s|%s", 100,
                     (wp && wp[0]) ? wp : "", (wc && wc[0]) ? wc : "");
            goto done;
        }
    }

    /* B2: original is a dict */
    WCUP_BRANCH(have_dict);
    if (!have_dict) { strcpy(label, "none"); goto done; }

    /* B3: both was-* empty */
    {
        int was_empty = ((!wp || !wp[0]) && (!wc || !wc[0]));
        WCUP_BRANCH(was_empty);
        if (was_empty) { strcpy(label, "none"); goto done; }
    }

    /* B4/B5: prompt cheaper */
    {
        double cur_p = (cp && cp[0]) ? m_finite(cp) : -1.0;
        double orig_p = m_finite(wp);
        int cheaper = (cur_p > 0 && orig_p > 0 && cur_p < orig_p);
        WCUP_BRANCH(cheaper);
        if (cheaper) {
            int pct = (int)lround((1.0 - (cur_p / orig_p)) * 100.0);
            int ok = pct >= 1;
            WCUP_BRANCH(ok);
            if (ok) {
                snprintf(label, sizeof label, "sale|%d|%s|%s", pct,
                         (wp && wp[0]) ? wp : "", (wc && wc[0]) ? wc : "");
                goto done;
            }
            strcpy(label, "none");
            goto done;
        }
    }

    /* B6/B7: completion cheaper */
    {
        double cur_c = (cc && cc[0]) ? m_finite(cc) : -1.0;
        double orig_c = m_finite(wc);
        int cheaper = (cur_c > 0 && orig_c > 0 && cur_c < orig_c);
        WCUP_BRANCH(cheaper);
        if (cheaper) {
            int pct = (int)lround((1.0 - (cur_c / orig_c)) * 100.0);
            int ok = pct >= 1;
            WCUP_BRANCH(ok);
            if (ok) {
                snprintf(label, sizeof label, "sale|%d|%s|%s", pct,
                         (wp && wp[0]) ? wp : "", (wc && wc[0]) ? wc : "");
                goto done;
            }
        }
        strcpy(label, "none");
    }

done:
    WCUP_VALSTR(label);
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("models_compute_sale_discount");
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s models_compute_sale_discount cp|cc|wp|wc ...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_case(argv[i]);
    return 0;
}
