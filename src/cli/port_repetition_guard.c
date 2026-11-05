/*
 * port_repetition_guard.c — Port of Python: agent/repetition_guard.py.
 */

#include "port_repetition_guard.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RG_MIN_FRAGMENT_LENGTH 400
#define RG_REPEAT_WINDOW 60
#define RG_MIN_REPEAT_COUNT 5
#define RG_DOMINANCE_RATIO 0.5
#define RG_MAX_ANCHOR_SAMPLES 32
#define RG_MAX_ANCHOR_MATCHES 8
#define RG_RUNAWAY_DISTINCT_LINE_RATIO 0.5

/* PoP: rg_line_repetition_dominated @ agent/repetition_guard.py:_line_repetition_dominated */
int rg_line_repetition_dominated(const char *text, int n)
{
    if (!text) return 0;
    const double ratio = (double)n * RG_DOMINANCE_RATIO;
    size_t tlen = strlen(text);

    /* Recover each non-empty stripped line and count it, mirroring Counter. */
    size_t i = 0;
    while (i <= tlen) {
        size_t start = i;
        size_t end = i;
        while (end < tlen && text[end] != '\n')
            end++;
        /* strip spaces/tabs on both ends */
        while (start < end && (text[start] == ' ' || text[start] == '\t' ||
                               text[start] == '\r'))
            start++;
        while (end > start && (text[end - 1] == ' ' || text[end - 1] == '\t' ||
                               text[end - 1] == '\r'))
            end--;
        size_t len = end - start;
        if (len > 0) {
            /* count occurrences of this exact line across the whole text */
            int c = 0;
            size_t j = 0;
            while (j <= tlen) {
                size_t s = j, e = j;
                while (e < tlen && text[e] != '\n')
                    e++;
                while (s < e && (text[s] == ' ' || text[s] == '\t' || text[s] == '\r'))
                    s++;
                while (e > s && (text[e - 1] == ' ' || text[e - 1] == '\t' ||
                                 text[e - 1] == '\r'))
                    e--;
                if (e - s == len && strncmp(text + start, text + s, len) == 0)
                    c++;
                j = e + 1;
            }
            if (c >= RG_MIN_REPEAT_COUNT && (double)c * (double)len >= ratio)
                return 1;
        }
        i = end + 1;
    }
    return 0;
}

/* PoP: rg_window_count_dominated @ agent/repetition_guard.py:_window_count_dominated */
int rg_window_count_dominated(const char *text, int n)
{
    const int window = RG_REPEAT_WINDOW;
    double needed_d = ceil((double)n * RG_DOMINANCE_RATIO / (double)window);
    long long needed = (long long)needed_d;
    if ((long long)RG_MIN_REPEAT_COUNT > needed)
        needed = (long long)RG_MIN_REPEAT_COUNT;

    char key[RG_REPEAT_WINDOW + 1];
    /* A flat dictionary of every distinct 60-char window. Worst case ~ n/60^? */
    /* Iterate and count each distinct window globally (Python builds a counts dict). */
    for (int i = 0; i <= n - window; i++) {
        memcpy(key, text + i, window);
        key[window] = '\0';
        long long c = 0;
        for (int k = i; k <= n - window; k++) {
            if (memcmp(text + k, key, window) == 0)
                c++;
        }
        if (c >= needed)
            return 1;
    }
    return 0;
}

/* PoP: rg_expand_run @ agent/repetition_guard.py:_expand_run */
void rg_expand_run(const char *text, int n, int start, int period, int matched,
                   int *left, int *right)
{
    int l = start;
    while (l > 0 && text[l - 1] == text[l - 1 + period])
        l--;

    int r = start + matched;
    while (r + period < n && text[r] == text[r + period])
        r++;
    *left = l;
    *right = r + period;
}

static int in_rejected_run(int start, long long period,
                           const int *lefts, const int *rights,
                           const int *periods, int count)
{
    for (int k = 0; k < count; k++)
        if (lefts[k] <= start && start < rights[k] && period % periods[k] == 0)
            return 1;
    return 0;
}

/* PoP: rg_periodic_run_dominated @ agent/repetition_guard.py:_periodic_run_dominated */
int rg_periodic_run_dominated(const char *text, int n)
{
    const int window = RG_REPEAT_WINDOW;
    int max_start = n - window;
    if (max_start < 1)
        return 0;

    int num = max_start + RG_MAX_ANCHOR_SAMPLES - 2;
    int sample_step = num < (RG_MAX_ANCHOR_SAMPLES - 1) ? 1
                                                        : num / (RG_MAX_ANCHOR_SAMPLES - 1);
    if (sample_step < 1)
        sample_step = 1;

    int starts[RG_MAX_ANCHOR_SAMPLES + 2];
    int n_starts = 0;
    for (int v = 0; v <= max_start && n_starts < RG_MAX_ANCHOR_SAMPLES + 2;
         v += sample_step)
        starts[n_starts++] = v;
    if (starts[n_starts - 1] != max_start)
        starts[n_starts++] = max_start;

    char anchor[RG_REPEAT_WINDOW + 1];
    int ret = 0;
    enum { MAX_REJ = RG_MAX_ANCHOR_SAMPLES * RG_MAX_ANCHOR_MATCHES };
    int rejected_lefts[MAX_REJ], rejected_rights[MAX_REJ], rejected_periods[MAX_REJ];
    int n_rejected = 0;

    for (int si = 0; si < n_starts && !ret; si++) {
        int start = starts[si];
        memcpy(anchor, text + start, window);
        anchor[window] = '\0';
        int search_from = start + 1;
        for (int m = 0; m < RG_MAX_ANCHOR_MATCHES && !ret; m++) {
            int match = -1;
            for (int pos = search_from; pos + window <= n; pos++) {
                if (memcmp(text + pos, anchor, window) == 0) {
                    match = pos;
                    break;
                }
            }
            if (match < 0)
                break;
            long long period = match - start;
            search_from = match + 1;
            if (in_rejected_run(start, period, rejected_lefts, rejected_rights,
                                rejected_periods, n_rejected))
                continue;
            int left, right;
            rg_expand_run(text, n, start, (int)period, window, &left, &right);
            long long run = right - left;
            if (run >= (long long)RG_MIN_REPEAT_COUNT * period &&
                run >= (long long)((double)n * RG_DOMINANCE_RATIO)) {
                ret = 1;
                break;
            }
            if (n_rejected < MAX_REJ) {
                rejected_lefts[n_rejected] = left;
                rejected_rights[n_rejected] = right;
                rejected_periods[n_rejected] = (int)period;
                n_rejected++;
            }
        }
    }
    return ret;
}

/* PoP: rg_is_repetition_dominated @ agent/repetition_guard.py:is_repetition_dominated */
int rg_is_repetition_dominated(const char *text)
{
    if (!text)
        return 0;
    size_t n = strlen(text);
    if (n < RG_MIN_FRAGMENT_LENGTH)
        return 0;
    if (rg_line_repetition_dominated(text, (int)n))
        return 1;
    return rg_window_count_dominated(text, (int)n) ||
           rg_periodic_run_dominated(text, (int)n);
}

/* Number of non-empty stripped lines; writes each line's (offset, len) into
 * the ORIGINAL text into bin[] (capacity bimax lines = bimax*2 size_t). */
static int collect_norm_lines(const char *text, size_t tlen, size_t *bin,
                              int bimax)
{
    int n = 0;
    size_t i = 0;
    while (i <= tlen && n < bimax) {
        size_t s = i, e = i;
        while (e < tlen && text[e] != '\n')
            e++;
        while (s < e && (text[s] == ' ' || text[s] == '\t' || text[s] == '\r'))
            s++;
        while (e > s && (text[e - 1] == ' ' || text[e - 1] == '\t' ||
                         text[e - 1] == '\r'))
            e--;
        if (e - s > 0) {
            bin[n * 2] = s;
            bin[n * 2 + 1] = e - s;
            n++;
        }
        i = e + 1;
    }
    return n;
}

static int line_eq(const char *text, size_t a0, size_t a1, size_t b0, size_t b1)
{
    return a1 == b1 && memcmp(text + a0, text + b0, a1) == 0;
}

/* PoP: rg_is_runaway_repetition @ agent/repetition_guard.py:is_runaway_repetition */
int rg_is_runaway_repetition(const char *text)
{
    if (!rg_is_repetition_dominated(text))
        return 0;
    size_t tlen = strlen(text);
    size_t lines[8192 * 2];
    const int nLines = collect_norm_lines(text, tlen, lines, 8192);
    if (nLines < RG_MIN_REPEAT_COUNT)
        return 1; /* dominated single-line loop: no structure to judge by */

    int distinct = 0;
    for (int a = 0; a < nLines; a++) {
        int seen = 0;
        for (int b = 0; b < a; b++) {
            if (line_eq(text, lines[a * 2], lines[a * 2 + 1],
                        lines[b * 2], lines[b * 2 + 1])) {
                seen = 1;
                break;
            }
        }
        if (!seen)
            distinct++;
    }
    return distinct <= (int)((double)nLines * RG_RUNAWAY_DISTINCT_LINE_RATIO);
}
