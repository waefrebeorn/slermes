/*
 * port_send_cmd.c
 *
 * Pure helpers ported from hermes_cli/send_cmd.py.
 *
 *   sc_exit_code <- send_cmd.py:_emit_result's exit-code decision
 *   sc_display   <- send_cmd.py:_emit_result's output routing
 *
 * The Python prints; the C port returns the same DECISIONS so the caller can
 * render them:
 *
 *      try: payload = json.loads(result_json) if result_json else {}
 *      except json.JSONDecodeError:
 *          payload = {"error": "invalid JSON from send_message_tool", "raw": result_json}
 *      if json_mode: print(json.dumps(payload, indent=2))
 *      elif not quiet:
 *          if payload.get("error"): print(f"hermes send: {payload['error']}", stderr)
 *          elif payload.get("success"): print(payload.get("note") or "sent")
 *          else: print(json.dumps(payload, indent=2))   # unknown shape
 *      if not payload.get("error") and (payload.get("skipped") or payload.get("success")):
 *          return _SUCCESS_EXIT        # 0
 *      return _FAILURE_EXIT            # 1
 *
 * Unknown / unexpected shapes are failures so scripts notice - that is why the
 * exit code is not simply "did it parse".
 */

#include "port_send_cmd.h"

#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"

/* send_cmd.py:_SUCCESS_EXIT / _FAILURE_EXIT */
#define SC_SUCCESS_EXIT 0
#define SC_FAILURE_EXIT 1

/* Branch selectors, in the Python's evaluation order. */
#define SC_DISPLAY_SUPPRESSED 0   /* quiet and not json_mode */
#define SC_DISPLAY_ERROR      1   /* payload["error"] -> stderr */
#define SC_DISPLAY_NOTE       2   /* payload["success"] -> stdout: note or "sent" */
#define SC_DISPLAY_UNKNOWN    3   /* neither -> stdout: the whole payload */
#define SC_DISPLAY_JSON_MODE  4   /* -j: the whole payload on stdout */

/* ===========================================================================
 *  _emit_result's parse step, shared by both helpers.
 *
 *  `json.loads(result_json) if result_json else {}` - an empty string is
 *  falsy, so it becomes an empty object rather than a parse error.
 *  On a decode error the payload is the error object above.
 *
 *  Returns a new json_t (caller frees) or NULL on OOM.
 * =========================================================================== */

static json_t *sc_parse_payload(const char *result_json)
{
    if (!result_json || !result_json[0]) {
        return json_object();
    }
    json_t *p = json_parse(result_json, NULL);
    if (!p) {
        /* json.JSONDecodeError -> the error object, raw string included */
        p = json_object();
        json_set(p, "error", json_string("invalid JSON from send_message_tool"));
        json_set(p, "raw", json_string(result_json));
    }
    return p;
}

/* ===========================================================================
 *  sc_exit_code  (send_cmd.py:_emit_result's return)
 *
 *      if not payload.get("error") and (payload.get("skipped") or payload.get("success")):
 *          return 0
 *      return 1
 *
 *  Truthiness matters: payload.get("error") is truthy for any non-empty string
 *  and falsy for None / "" / false / 0. A payload with error="" still counts
 *  as no error, matching Python.
 * =========================================================================== */

/* PoP: _emit_result (exit-code decision) @ hermes_cli/send_cmd.py:_emit_result */
int sc_exit_code(const char *result_json)
{
    json_t *p = sc_parse_payload(result_json);
    if (!p) return SC_FAILURE_EXIT;

    json_t *err     = json_object_get(p, "error");
    json_t *skipped = json_object_get(p, "skipped");
    json_t *success = json_object_get(p, "success");

    int has_error = (err && err->type == JSON_STRING && err->str_val && err->str_val[0])
                 || (err && err->type == JSON_BOOL && err->bool_val)
                 || (err && err->type == JSON_NUMBER && err->num_val != 0.0);
    int truthy_skipped = (skipped && skipped->type == JSON_BOOL && skipped->bool_val)
                      || (skipped && skipped->type == JSON_STRING && skipped->str_val && skipped->str_val[0]);
    int truthy_success = (success && success->type == JSON_BOOL && success->bool_val)
                      || (success && success->type == JSON_STRING && success->str_val && success->str_val[0]);

    json_free(p);
    return (!has_error && (truthy_skipped || truthy_success)) ? SC_SUCCESS_EXIT : SC_FAILURE_EXIT;
}

/* ===========================================================================
 *  sc_display  (send_cmd.py:_emit_result's output routing)
 *
 *      if json_mode: -> the whole payload on stdout (SC_DISPLAY_JSON_MODE)
 *      elif quiet:   -> nothing (SC_DISPLAY_SUPPRESSED)
 *      elif error:   -> SC_DISPLAY_ERROR
 *      elif success: -> SC_DISPLAY_NOTE
 *      else:         -> SC_DISPLAY_UNKNOWN
 *
 *  `out_to_stderr` is set for SC_DISPLAY_ERROR only.
 *  `out_text` receives a malloc'd string the caller frees: the note (or "sent")
 *  for SC_DISPLAY_NOTE, otherwise NULL. The JSON dump itself is the caller's
 *  to render - json_serialize(payload) - since the Python's indent=2 is a
 *  formatting choice, not a decision.
 * =========================================================================== */

/* PoP: _emit_result (output routing) @ hermes_cli/send_cmd.py:_emit_result */
int sc_display(const char *result_json, int json_mode, int quiet,
               char **out_text, int *out_to_stderr)
{
    if (out_text) *out_text = NULL;
    if (out_to_stderr) *out_to_stderr = 0;

    json_t *p = sc_parse_payload(result_json);
    if (!p) return SC_DISPLAY_UNKNOWN;

    json_t *err     = json_object_get(p, "error");
    json_t *success = json_object_get(p, "success");

    int has_error = (err && err->type == JSON_STRING && err->str_val && err->str_val[0])
                 || (err && err->type == JSON_BOOL && err->bool_val)
                 || (err && err->type == JSON_NUMBER && err->num_val != 0.0);
    int has_success = success && success->type == JSON_BOOL && success->bool_val;

    int result;
    if (json_mode) {
        result = SC_DISPLAY_JSON_MODE;
    } else if (quiet) {
        result = SC_DISPLAY_SUPPRESSED;
    } else if (has_error) {
        result = SC_DISPLAY_ERROR;
        if (out_to_stderr) *out_to_stderr = 1;
    } else if (has_success) {
        result = SC_DISPLAY_NOTE;
        if (out_text) {
            json_t *note = json_object_get(p, "note");
            if (note && note->type == JSON_STRING && note->str_val && note->str_val[0]) {
                *out_text = strdup(note->str_val);
            } else {
                *out_text = strdup("sent");
            }
        }
    } else {
        result = SC_DISPLAY_UNKNOWN;
    }
    json_free(p);
    return result;
}
