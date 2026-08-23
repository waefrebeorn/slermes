/* gate: reasoning_effort_clamp_effort — traced copy of
 * src/agent/reasoning_effort.c::clamp_effort
 * (LIVE twin: agent/reasoning_effort.py::clamp_effort).
 *
 * Input per argv token: JSON [effort, supported_list, overrides_obj].
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: no requested / no supported        -> passthrough effort
 *   per-supported-item: L tick, B2 level in ladder
 *   B3: supported_norm empty OR requested already in it -> effort
 *   B4: overrides non-empty
 *     per-override: L tick, B5 key==requested
 *       per-norm: L tick, B6 mapped supported -> mapped
 *   B7: requested not on canonical ladder  -> effort
 *   per-norm candidate scan: L tick, B8 level != "none"
 *   B9: no candidates                      -> effort
 *   per-candidate weaker scan: L tick, B10 strictly-weaker-and-nearest
 *   B11: a weaker level exists -> it, else weakest supported
 *   V : final resolved effort string
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include <json.h>

static const char *const LADDER[] = {
    "none", "minimal", "low", "medium", "high", "xhigh", "max", "ultra",
};
#define LADDER_N 8

static int ladder_index(const char *level)
{
    if (!level) return -1;
    for (int i = 0; i < LADDER_N; i++)
        if (strcmp(LADDER[i], level) == 0) return i;
    return -1;
}

static char *strip_lower(const char *s, char *out, size_t cap)
{
    if (!s) return NULL;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t o = 0;
    for (; *s && o + 1 < cap; s++)
        out[o++] = (char)tolower((unsigned char)*s);
    while (o > 0 && (out[o-1] == ' ' || out[o-1] == '\t' ||
                     out[o-1] == '\n' || out[o-1] == '\r'))
        o--;
    out[o] = '\0';
    if (!o) return NULL;
    return out;
}

static const char *trace_clamp(const char *effort,
                               const json_t *supported,
                               const json_t *overrides)
{
    WCUP_BEGIN("clamp_effort");
    const char *result = effort;
    char requested_buf[64];
    char *requested = strip_lower(effort ? effort : "", requested_buf,
                                  sizeof requested_buf);

    int have_supported = supported && supported->type == JSON_ARRAY &&
                         supported->c.count > 0;
    WCUP_BRANCH(!requested || !have_supported);            /* B1 */
    if (!requested || !have_supported)
        goto out;

    {
        const char *supported_norm[64];
        int snorm = 0;
        for (size_t i = 0; i < supported->c.count && snorm < 64; i++) {
            WCUP_LOOP_TICK();
            json_t *lvl_v = supported->c.items[i];
            char nb[64];
            char *lvl = strip_lower(
                (lvl_v && lvl_v->type == JSON_STRING && lvl_v->str_val)
                    ? lvl_v->str_val : "", nb, sizeof nb);
            int on_ladder = lvl && ladder_index(lvl) >= 0;
            WCUP_BRANCH(on_ladder);                        /* B2 */
            if (on_ladder)
                supported_norm[snorm++] = LADDER[ladder_index(lvl)];
        }

        int in_norm = 0;
        for (int i = 0; i < snorm; i++)
            if (strcmp(supported_norm[i], requested) == 0) { in_norm = 1; break; }
        WCUP_BRANCH(snorm == 0 || in_norm);                /* B3 */
        if (snorm == 0 || in_norm)
            goto out;

        WCUP_BRANCH(overrides && overrides->type == JSON_OBJECT &&
                    overrides->c.count > 0);               /* B4 */
        if (overrides && overrides->type == JSON_OBJECT) {
            for (size_t i = 0; i < overrides->c.count; i++) {
                WCUP_LOOP_TICK();
                const char *key = overrides->c.keys[i];
                json_t *mapped_v = overrides->c.items[i];
                int match = key && strcmp(key, requested) == 0;
                WCUP_BRANCH(match);                        /* B5 */
                if (!match) continue;
                const char *mapped =
                    (mapped_v && mapped_v->type == JSON_STRING &&
                     mapped_v->str_val) ? mapped_v->str_val : NULL;
                for (int j = 0; j < snorm; j++) {
                    WCUP_LOOP_TICK();
                    int hit = mapped && strcmp(supported_norm[j], mapped) == 0;
                    WCUP_BRANCH(hit);                      /* B6 */
                    if (hit) { result = mapped; goto out; }
                }
            }
        }

        WCUP_BRANCH(ladder_index(requested) < 0);          /* B7 */
        if (ladder_index(requested) < 0)
            goto out;
        int ridx = ladder_index(requested);

        const char *candidates[64];
        int ncand = 0;
        for (int i = 0; i < snorm; i++) {
            WCUP_LOOP_TICK();
            int not_none = strcmp(supported_norm[i], "none") != 0;
            WCUP_BRANCH(not_none);                         /* B8 */
            if (not_none) candidates[ncand++] = supported_norm[i];
        }
        WCUP_BRANCH(ncand == 0);                           /* B9 */
        if (ncand == 0)
            goto out;

        int best_below = -1, best_idx = -1, min_idx = -1;
        for (int i = 0; i < ncand; i++) {
            WCUP_LOOP_TICK();
            int idx = ladder_index(candidates[i]);
            if (min_idx < 0 || idx < min_idx) min_idx = idx;
            int better = idx < ridx && idx > best_idx;
            WCUP_BRANCH(better);                           /* B10 */
            if (better) { best_idx = idx; best_below = i; }
        }
        WCUP_BRANCH(best_below >= 0);                      /* B11 */
        if (best_below >= 0) {
            result = candidates[best_below];
            goto out;
        }
        for (int i = 0; i < ncand; i++)
            if (ladder_index(candidates[i]) == min_idx) { result = candidates[i]; break; }
    }
out:
    WCUP_VALSTR(result ? result : "");
    {
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("clamp_effort");
        return result;
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s clamp_effort '[effort,supported,overrides]'...\n",
                argv[0]);
        return 2;
    }
    for (int i = 2; i < argc; i++) {
        char *jerr = NULL;
        json_t *root = json_parse(argv[i], &jerr);
        if (!root || root->type != JSON_ARRAY || root->c.count < 2) {
            fprintf(stderr, "bad input: %s\n", argv[i]);
            return 2;
        }
        json_t *sup = root->c.items[1];
        json_t *ovr = root->c.count > 2 ? root->c.items[2] : NULL;
        const char *eff = NULL;
        json_t *ev = root->c.items[0];
        if (ev && ev->type == JSON_STRING) eff = ev->str_val;
        trace_clamp(eff, sup && sup->type == JSON_ARRAY ? sup : NULL,
                    ovr && ovr->type == JSON_OBJECT ? ovr : NULL);
    }
    return 0;
}
