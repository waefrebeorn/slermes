#ifndef NOC_CTRLDISP_H
#define NOC_CTRLDISP_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: ctrldisp_is_producer @ gateway/control_dispatch.py:ControlRequestDispatcher.dispatch
 * PoP: ctrldisp_select      @ gateway/control_dispatch.py:ControlRequestDispatcher.dispatch
 *
 * Faithful port of the scheduling split inside ControlRequestDispatcher.dispatch:
 *   request  = json.loads(raw)   (ValueError/RecursionError -> None)
 *   producer = isinstance(request, dict) and request.get('verb') == 'hosted-producer'
 *   executor = producers if producer else controls
 *
 * Both functions take the raw control payload as a NUL-terminated UTF-8 string
 * (the bytes `raw` that dispatch hands to json.loads). Each returns a NEWLY
 * allocated json_t that the caller owns and must json_free().
 */

/* 1 when json.loads(raw) yields a dict whose 'verb' is "hosted-producer". */
struct json_t *ctrldisp_is_producer(const char *raw);

/* A json_t STRING: "producers" for a hosted-producer request, else "controls". */
struct json_t *ctrldisp_select(const char *raw);

#ifdef __cplusplus
}
#endif

#endif /* NOC_CTRLDISP_H */
