#ifndef NOC_W18_SCHEDULERD_934AF8_H
#define NOC_W18_SCHEDULERD_934AF8_H

#include "hermes_json.h"

/* C home for upstream Hermes cron/scheduler_delivery_origin.py. */

/* -> {"route": <route_metadata>, "media": <media_metadata>} after stamping.
 * t = {"origin": obj|null, "origin_target": bool, "origin_user_id": value,
 *      "is_relay": bool}. */
json_t *w18_schedulerd_934af8_stamp(const json_t *t, const json_t *route, const json_t *media);

#endif /* NOC_W18_SCHEDULERD_934AF8_H */
