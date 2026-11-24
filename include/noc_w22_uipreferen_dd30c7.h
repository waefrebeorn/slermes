#ifndef NOC_W22_UIPREFEREN_DD30C7_H
#define NOC_W22_UIPREFEREN_DD30C7_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Port of hermes_cli/web_routers/ui_preferences.py (gate w22_uipreferen_dd30c7).
 *
 * The module is thin HTTP over `hermes_cli.ui_preferences`: the preferences
 * store (`read_preferences` / `write_preferences`, its doc, its busy lock and
 * its file) is the seam stubbed out of the oracle. What the router itself
 * decides is pure and ported here:
 *
 *   - model:  the `PreferencesPatch` request model (pydantic v2, lax mode):
 *             `values` defaults to {} and must map str -> str|None (an int, a
 *             bool or a list is a validation error, not a coercion); `seed`
 *             defaults to False and takes the lax bool tokens/1.0/0.0.
 *   - get / patch: the exception -> HTTP status mapping of the two handlers
 *             (503 for PreferencesBusy on both, 400 for PreferencesError on
 *             PATCH only -- a GET lets PreferencesError escape as a 500), and
 *             the store call the PATCH handler makes with the validated body.
 *   - routes: the two routes the APIRouter declares.
 */

/* PoP: w22_uipreferen_dd30c7_model @ hermes_cli/web_routers/ui_preferences.py:PreferencesPatch
 * -> {"valid": bool, "values": {k: str|null} | null, "seed": bool | null}
 * `valid` false carries nulls; it is the FastAPI 422 branch. */
json_t *w22_uipreferen_dd30c7_model(const json_t *body);

/* PoP: w22_uipreferen_dd30c7_routes @ hermes_cli/web_routers/ui_preferences.py:router
 * [{"method": "GET", "path": "/api/ui-preferences"},
 *  {"method": "PATCH", "path": "/api/ui-preferences"}] */
json_t *w22_uipreferen_dd30c7_routes(void);

/* PoP: w22_uipreferen_dd30c7_get @ hermes_cli/web_routers/ui_preferences.py:get_ui_preferences
 * error_kind: "" | "busy" | "error" (the store raising PreferencesBusy /
 * PreferencesError). -> {"status": 200|500|503, "body": <doc>|null,
 *                        "detail": <str>|null}
 * A PreferencesError is not caught on GET, so it surfaces as a 500. */
json_t *w22_uipreferen_dd30c7_get(const json_t *doc, const char *error_kind,
                                  const char *error_msg);

/* PoP: w22_uipreferen_dd30c7_patch @ hermes_cli/web_routers/ui_preferences.py:patch_ui_preferences
 * body: the raw request object (validated through the model first; an invalid
 * body is the router's 422 and never reaches the store).
 * error_kind: "" | "busy" | "error" for the store's write.
 * -> {"status": 200|400|422|503, "body": {...}|null, "detail": <str>|null}
 * On 200 the body is the store's echo of the call:
 * {"wrote_values": {k: str|null}, "wrote_seed": bool}. */
json_t *w22_uipreferen_dd30c7_patch(const json_t *body, const char *error_kind,
                                    const char *error_msg);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W22_UIPREFEREN_DD30C7_H */
