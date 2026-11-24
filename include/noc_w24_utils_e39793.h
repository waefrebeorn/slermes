/*
 * noc_w24_utils_e39793.h - C surface for upstream
 * plugins/platforms/qqbot/utils.py (gate w24_utils_e39793).
 *
 * The pure half of the QQBot adapter utilities: the User-Agent it presents,
 * the header set that goes with it, and the config coercion that turns a
 * comma-separated string / list / scalar into a trimmed string list.  The
 * host facts (``sys.version_info``, ``platform.system()``, the installed
 * hermes-agent version) and the network are seams: they arrive as values.
 */
#ifndef NOC_W24_UTILS_E39793_H
#define NOC_W24_UTILS_E39793_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* value: <any> -> [string, ...].  None -> []; a str is split on ","; a
 * list/tuple/set is taken as items; anything else is one item.  Every item is
 * str()-ed, stripped, and dropped when empty. */
json_t *w24_utils_e39793_coerce_list(const json_t *value);

/* {qqbot_version?, py_major, py_minor, py_micro, os_name, hermes_version} ->
 * "QQBotAdapter/<qqbot> (Python/<ma>.<mi>.<mc>; <os.lower()>; Hermes/<ver>)". */
json_t *w24_utils_e39793_build_user_agent(const json_t *req);

/* {user_agent} -> {"Content-Type", "Accept", "User-Agent"} (both types are
 * application/json; q.qq.com serves an anti-bot page without the Accept). */
json_t *w24_utils_e39793_api_headers(const json_t *req);

/* QQBOT_VERSION and the endpoint/header literals, as an object. */
json_t *w24_utils_e39793_constants(void);

#ifdef __cplusplus
}
#endif
#endif
