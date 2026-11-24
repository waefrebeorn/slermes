#ifndef NOC_W17_MODELMETAD_923E37_H
#define NOC_W17_MODELMETAD_923E37_H

#include "hermes_json.h"

/* Replays agent/model_metadata_llamacpp.py context-window probes against a
 * scripted HTTP client. ``responses`` is a JSON array of response specs,
 * consumed in call order (the same order the live client.get calls occur):
 *
 *   {"status": <int, default 200>,
 *    "body": <JSON value returned by resp.json()>,
 *    "json_raises": <bool: resp.json() raises ValueError>,
 *    "error": <bool: client.get() raises httpx.HTTPError>}
 *
 * llamacpp_context consumes up to two /props responses and, on fallback, one
 * /models response. router_launch_ctx consumes one /models response.
 * Both return a JSON number (the context size) or JSON null. */
json_t *w17_modelmetad_923e37_llamacpp_context(const char *model, json_t *responses);
json_t *w17_modelmetad_923e37_router_launch_ctx(const char *model, json_t *responses);

#endif
