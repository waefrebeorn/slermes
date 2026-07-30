/* json_yaml_bridge.c — REAL json_parse_yaml over libyaml.
 *
 * hermes_json.h historically defined json_parse_yaml(s) as a macro aliasing
 * json_parse(s, NULL) — feeding YAML text to the strict JSON parser, which
 * fails on any actual YAML (nested maps, unquoted scalars). That made every
 * config.yaml consumer that relied on json_parse_yaml silently fall back to
 * defaults (gateway_config_load platforms parsing among them).
 *
 * This bridge does it faithfully: parse with libyaml (lib/libyaml/yaml.c),
 * serialize the root to a JSON string via yaml_to_json_string(doc, ""), then
 * parse that JSON into a json_t tree. Falls back to plain json_parse when the
 * input is already JSON (libyaml also accepts JSON-ish scalars, but a valid
 * JSON document should keep byte-exact number/string semantics from libjson).
 */

#include "json.h"
#include "../libyaml/yaml.h"
#include <stdlib.h>
#include <string.h>

json_t *json_parse_yaml_real(const char *input) {
    if (!input) return NULL;

    /* Fast path: already-valid JSON parses exactly as before. */
    json_t *as_json = json_parse(input, NULL);
    if (as_json) return as_json;

    char *err = NULL;
    yaml_doc_t *doc = yaml_parse(input, &err);
    if (err) free(err);
    if (!doc) return NULL;

    char *json_text = yaml_to_json_string(doc, "");
    yaml_free(doc);
    if (!json_text) return NULL;

    json_t *out = json_parse(json_text, NULL);
    free(json_text);
    return out;
}
