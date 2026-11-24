#ifndef NOC_W19_USAGEANALY_6EC515_H
#define NOC_W19_USAGEANALY_6EC515_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_usageanaly_6ec515_scoped @ persistence/sqlite/conversation/usage_analytics.py:_scoped
 * {"all":unfiltered,"with_source":source-filtered} query pair sharing one body. */
json_t *w19_usageanaly_6ec515_scoped(const char *before, const char *after, const char *alias);

/* PoP: w19_usageanaly_6ec515_local_surface @ persistence/sqlite/conversation/usage_analytics.py:_LOCAL_SURFACE
 * The gateway-local surface filter for one table alias ("" or "s."). */
char *w19_usageanaly_6ec515_local_surface(const char *alias);

/* PoP: w19_usageanaly_6ec515_query_sql @ persistence/sqlite/conversation/usage_analytics.py:_InsightQueries.__init__
 * {"all":...,"with_source":...} after the missing-index and missing-created_source rewrites;
 * null for an unknown base name. */
json_t *w19_usageanaly_6ec515_query_sql(const char *base, int has_index, int local_surfaces);

/* PoP: w19_usageanaly_6ec515_query_params @ persistence/sqlite/conversation/usage_analytics.py:_InsightQueries._query
 * The bound parameters for one insights read. */
json_t *w19_usageanaly_6ec515_query_params(json_t *cutoff, json_t *source, int local_surfaces);

/* PoP: w19_usageanaly_6ec515_insight_wait @ persistence/sqlite/conversation/usage_analytics.py:SQLiteUsageAnalytics._insight_wait
 * Seconds an insights read waits on a lock. */
json_t *w19_usageanaly_6ec515_insight_wait(int read_only, double busy_timeout);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_USAGEANALY_6EC515_H */
