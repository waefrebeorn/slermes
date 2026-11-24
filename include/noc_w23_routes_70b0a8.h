/*
 * noc_w23_routes_70b0a8.h - C surface for upstream
 * persistence/sqlite/gateway/_routes.py (gate w23_routes_70b0a8).
 *
 * The private SQL of the SQLite RoutingRepository: the route-retirement
 * predicate and its named parameters, the epoch-window fences, the session
 * recency expression and the topic-table literals.  Pure text: no connection,
 * no clock, no environment is read here.
 */
#ifndef NOC_W23_ROUTES_70B0A8_H
#define NOC_W23_ROUTES_70B0A8_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Module literals, mirrored from the Python source. */
#define W23_ROUTES_70B0A8_FENCE_PREFIX "gateway.retired_route.v1."
#define W23_ROUTES_70B0A8_RETIRED_PREFIX "gateway.retired_session.v1."
#define W23_ROUTES_70B0A8_RETRY_HORIZON_S 604800
#define W23_ROUTES_70B0A8_IN_WINDOW "BETWEEN 0.0 AND 4200000000.0"

/* _sql_in_window(expr): *expr* when inside the coerce_epoch window, else NULL.
 * Empty/NULL *expr* renders as the Python None text.  Caller frees. */
char *w23_routes_70b0a8_in_window(const char *expr);

/* _sql_freshest_of(activity, session_id_expr, started).  Caller frees. */
char *w23_routes_70b0a8_freshest_of(const char *activity,
                                    const char *session_id_expr,
                                    const char *started);

/* _sql_session_last_active(alias="s").  Caller frees. */
char *w23_routes_70b0a8_last_active(const char *alias);

/* route_fence_key(namespace, session_id).  Caller frees. */
char *w23_routes_70b0a8_fence_key(const char *ns, const char *session_id);

/* route_namespace(session_key): the <ns> slot of an agent:<ns>:... key, else
 * "main".  Caller frees. */
char *w23_routes_70b0a8_namespace(const char *key);

/* route_retired_params(session_key, entry_json, now) -> the exact named
 * parameters of ROUTE_RETIRED_SQL (entry, ns, now, horizon, retired_prefix,
 * fence_prefix).  Keys absent from *req* read as None. */
json_t *w23_routes_70b0a8_retired_params(const json_t *req);

/* The module literals as one object: fence_prefix, retired_prefix, in_window,
 * expired, horizon, recoverable[], recoverable_sql, donor_columns,
 * retired_sql, topic_tables[], topic_columns[]. */
json_t *w23_routes_70b0a8_constants(void);

#ifdef __cplusplus
}
#endif
#endif
