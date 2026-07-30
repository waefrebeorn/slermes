#ifndef HERMES_MIDDLEWARE_H
#define HERMES_MIDDLEWARE_H

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MW_OBSERVER_SCHEMA_VERSION "hermes.observer.v1"
#define MW_MIDDLEWARE_SCHEMA_VERSION "hermes.middleware.v1"

json_t *middleware_observer_payload(const json_t *kwargs);
json_t *middleware_middleware_payload(const json_t *kwargs);
json_t *middleware_trace_entry(const json_t *result);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_MIDDLEWARE_H */
