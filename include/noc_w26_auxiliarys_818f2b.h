/*
 * noc_w26_auxiliarys_818f2b.h - C surface for upstream
 * agent/auxiliary_structured_output.py (gate w26_auxiliarys_818f2b).
 *
 * Structured-output (``response_format``) capability for auxiliary requests:
 * the route key an endpoint maps to, the format type a request would carry,
 * whether a rejection speaks to the route's capability, the drop of a format a
 * route is known to refuse, the retry kwargs with every format removed, and the
 * memo entry a rejection records.  The provider profile registry is a seam and
 * arrives as the ``unsupported`` input; the process memo arrives as ``rejected``.
 */
#ifndef NOC_W26_AUXILIARYS_818F2B_H
#define NOC_W26_AUXILIARYS_818F2B_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {provider, base_url} -> the route key (endpoint host:port, else the
 * stripped, lowercased provider name). */
json_t *w26_auxiliarys_818f2b_route_key(const json_t *req);

/* request_kwargs -> the response_format type it carries (extra_body first,
 * then the top level), or null. */
json_t *w26_auxiliarys_818f2b_format_type(const json_t *kwargs);

/* error -> whether the rejection names the capability (memoisable) rather than
 * this request's schema. */
json_t *w26_auxiliarys_818f2b_capability(const json_t *error);

/* req: {extra_body, provider, base_url, model, unsupported:[type...],
 *       rejected:[[route, model, type]...]} -> extra_body minus a refused
 * response_format, unchanged otherwise. */
json_t *w26_auxiliarys_818f2b_without_format(const json_t *req);

/* kwargs -> a copy with response_format removed everywhere, or null when
 * nothing was removed. */
json_t *w26_auxiliarys_818f2b_strip_format(const json_t *kwargs);

/* req: {provider, base_url, kwargs, error} -> the memo entry the rejection
 * records ([route, model, type]), or null when it records nothing. */
json_t *w26_auxiliarys_818f2b_remember(const json_t *req);

/* The capability-rejection markers. */
json_t *w26_auxiliarys_818f2b_constants(void);

#ifdef __cplusplus
}
#endif
#endif
