/* gate: gateway_browser_control_broker_browser_control_enabled — traced copy
 * of src/gateway/browser_control_broker.c::bcb_browser_control_enabled's
 * config-dict path (bcb_flag_true ladder). The REAL port is ALSO linked via
 * extra_src and called per input as an internal consistency check.
 * LIVE twin: gateway/browser_control_broker.py::browser_control_enabled
 * (config-dict path; the config=None live-read path is out of scope).
 *
 * Input encoding: one JSON object per argv item (the `config` dict).
 *
 * Branch ladder (twin.py mirrors EXACTLY):
 *   B1: config is an object
 *   B2: config.browser is an object
 *   B3: config.browser.extension_control is an object
 *   B4: extension_control.enabled is exactly True
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"

int bcb_browser_control_enabled(const json_t *config);

void hermes_get_home(char *out, size_t n) { snprintf(out, n, "%s", "/nonexistent-home-for-gate"); }
bool crypto_random_bytes(unsigned char *buf, size_t n) { (void)buf; (void)n; return false; } /* stub: not on the traced path */

static void trace_one(const char *config_json) {
    WCUP_BEGIN("browser_control_enabled");
    int result = 0;
    json_t *cfg = config_json ? json_parse(config_json, NULL) : NULL;
    if (!cfg || cfg->type != JSON_OBJECT) {           /* B1 */
        WCUP_BRANCH(0);
        result = 0;
    } else {
        WCUP_BRANCH(1);
        json_t *browser = json_obj_get(cfg, "browser");
        if (!browser || browser->type != JSON_OBJECT) {   /* B2 */
            WCUP_BRANCH(0);
            result = 0;
        } else {
            WCUP_BRANCH(1);
            json_t *ext = json_obj_get(browser, "extension_control");
            if (!ext || ext->type != JSON_OBJECT) {       /* B3 */
                WCUP_BRANCH(0);
                result = 0;
            } else {
                WCUP_BRANCH(1);
                json_t *v = json_obj_get(ext, "enabled");
                if (v && v->type == JSON_BOOL && v->bool_val) {  /* B4 */
                    WCUP_BRANCH(1);
                    result = 1;
                } else {
                    WCUP_BRANCH(0);
                    result = 0;
                }
            }
        }
    }
    if (cfg) json_free(cfg);
    char val[4] = {result ? '1' : '0', 0};
    WCUP_VALSTR(val);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("browser_control_enabled");

    /* consistency: REAL linked port must agree */
    {
        json_t *again = config_json ? json_parse(config_json, NULL) : NULL;
        int real = bcb_browser_control_enabled(again);
        if (again) json_free(again);
        if (real != result) {
            fprintf(stderr, "real port disagrees on '%s'\n", config_json);
            exit(3);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s browser_control_enabled <config-json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) trace_one(argv[i]);
    return 0;
}
