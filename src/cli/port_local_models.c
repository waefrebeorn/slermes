/*
 * port_local_models.c
 *
 * Pure helpers ported from hermes_cli/web_routers/local_models.py (54 defs,
 * previously 0 ported).
 *
 *   lm_human_gb      <- _human_gb
 *   lm_k_label       <- _k_label
 *   lm_rate_and_eta  <- _rate_and_eta
 *   lm_record_rate   <- _record_rate's sample-window logic
 *
 * The FastAPI / asyncio half stays in Python. `_record_rate` itself needs the
 * _RUNNING registry and time.monotonic; what is portable is the sample window
 * and the rate/ETA derivation, which is where the real logic lives.
 */

#include "port_local_models.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* local_models.py:_RATE_MIN_ELAPSED */
#define LM_RATE_MIN_ELAPSED 0.5

/* local_models.py:_RATE_WINDOW */
#define LM_RATE_WINDOW 8.0

/* ===========================================================================
 *  _human_gb  (local_models.py:_human_gb)
 *
 *      return f"{n / (1 << 30):.1f} GB"
 *
 *  `1 << 30` is 1073741824 - a GiB, not a GB, despite the label.
 *  Caller frees.
 * =========================================================================== */

/* PoP: _human_gb @ hermes_cli/web_routers/local_models.py:_human_gb */
char *lm_human_gb(double n)
{
    char *out = (char *)malloc(32);
    if (!out) return NULL;
    snprintf(out, 32, "%.1f GB", n / (double)(1u << 30));
    return out;
}

/* ===========================================================================
 *  _k_label  (local_models.py:_k_label)
 *
 *      return f"{tokens // 1024}K"
 *
 *  INTEGER division (//), so 2047 -> "1K", not "2K". Caller frees.
 * =========================================================================== */

/* PoP: _k_label @ hermes_cli/web_routers/local_models.py:_k_label */
char *lm_k_label(long long tokens)
{
    char *out = (char *)malloc(24);
    if (!out) return NULL;
    snprintf(out, 24, "%lldK", tokens / 1024);
    return out;
}

/* ===========================================================================
 *  _rate_and_eta  (local_models.py:_rate_and_eta)
 *
 *      first_at, first_done = samples[0]
 *      elapsed = samples[-1][0] - first_at
 *      if elapsed < _RATE_MIN_ELAPSED or done <= first_done:
 *          return None, None
 *      rate = (done - first_done) / elapsed
 *      if total:
 *          return rate, max(0, round(max(0, total - done) / rate))
 *      return rate, None
 *
 *  The rate is the slope across the whole trailing window, not the last two
 *  ticks, so a burst reads as throughput rather than a spike. Anything that
 *  cannot be turned into an honest number (one sample, a window too short to
 *  divide by, a transfer that hasn't moved) reports unknown, not a guess.
 *
 *  `samples` is a flat [t0, done0, t1, done1, ...] array of at least one pair.
 *  `out_rate` / `out_eta` receive the results; `out_eta` is set to -1 for None.
 * =========================================================================== */

/* PoP: _rate_and_eta @ hermes_cli/web_routers/local_models.py:_rate_and_eta */
void lm_rate_and_eta(const double *samples, int nsamples,
                     long long done, long long total,
                     int *have_rate, double *out_rate,
                     int *have_eta, long long *out_eta)
{
    *have_rate = 0; *have_eta = 0;
    if (nsamples < 1) return;

    double first_at = samples[0];
    double first_done = samples[1];
    double last_at = samples[(nsamples - 1) * 2];
    double elapsed = last_at - first_at;

    if (elapsed < LM_RATE_MIN_ELAPSED) return;
    if ((double)done <= first_done) return;

    double rate = ((double)done - first_done) / elapsed;
    *have_rate = 1;
    *out_rate = rate;

    if (total) {
        double remaining = (double)total - (double)done;
        if (remaining < 0) remaining = 0;
        double eta = remaining / rate;
        long long rounded = (long long)llround(eta);
        if (rounded < 0) rounded = 0;
        *have_eta = 1;
        *out_eta = rounded;
    }
}

/* ===========================================================================
 *  _record_rate's sample window  (local_models.py:_record_rate)
 *
 *      samples.append((now, done))
 *      while len(samples) > 1 and now - samples[0][0] > _RATE_WINDOW:
 *          samples.popleft()
 *
 *  The window is trimmed to the newest _RATE_WINDOW seconds but ALWAYS keeps at
 *  least one sample (the `len(samples) > 1` guard), so a stalled transfer still
 *  has a baseline to compare against. Returns the new sample count.
 * =========================================================================== */

/* PoP: _record_rate (window trim) @ hermes_cli/web_routers/local_models.py:_record_rate */
int lm_trim_sample_window(double *samples, int nsamples, double now)
{
    int start = 0;
    while (nsamples - start > 1 && now - samples[start * 2] > LM_RATE_WINDOW) start++;
    if (start > 0) {
        int kept = nsamples - start;
        memmove(samples, samples + start * 2, (size_t)kept * 2 * sizeof(double));
        nsamples = kept;
    }
    return nsamples;
}
