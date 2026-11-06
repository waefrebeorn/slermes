/*
 * port_compression_facade.h — pure warning-message builders from
 * agent/compression_facade.py. The live functions build these exact strings and
 * hand them to agent._emit_warning (the unported sink); here the caller gets the
 * allocated message and chooses where to emit. Integer/one-decimal rounding
 * matches Python's format — round-half-to-even (f"{7.5:.0f}" is "8").
 */
#ifndef PORT_COMPRESSION_FACADE_H
#define PORT_COMPRESSION_FACADE_H

/* _warn_commit_overrun: waits/ceiling rendered as {:.0f}. */
char *cf_warn_commit_overrun(double waited, double ceiling);

/* _report_compression_timeout total-exhausted branch: i.e. waited {:.1f} and
 * "after summary output was observed" appended iff progress_observed. */
char *cf_commit_timeout_total(double waited, int progress_observed);

/* _report_compression_timeout stalled branch: idle {:.1f}. */
char *cf_commit_timeout_stalled(double idle);

#endif /* PORT_COMPRESSION_FACADE_H */
