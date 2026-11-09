/*
 * port_local_models.h -- pure helpers ported from
 * hermes_cli/web_routers/local_models.py.
 *
 * The FastAPI / asyncio half stays in Python.
 */
#ifndef PORT_LOCAL_MODELS_H
#define PORT_LOCAL_MODELS_H

#include <stddef.h>

/* _human_gb -- "%.1f GB" of n / (1 << 30) - a GiB, despite the label.
 * Caller frees. */
char *lm_human_gb(double n);

/* _k_label -- integer division by 1024, then "K". Caller frees. */
char *lm_k_label(long long tokens);

/* _rate_and_eta -- the slope across the trailing sample window, and the
 * remaining seconds. `samples` is a flat [t0, done0, t1, done1, ...] array.
 * *out_eta is only set when *have_eta is 1. */
void lm_rate_and_eta(const double *samples, int nsamples,
                     long long done, long long total,
                     int *have_rate, double *out_rate,
                     int *have_eta, long long *out_eta);

/* _record_rate's sample window -- trim to the newest 8 s, always keeping at
 * least one sample. Returns the new sample count. */
int lm_trim_sample_window(double *samples, int nsamples, double now);

#endif /* PORT_LOCAL_MODELS_H */
