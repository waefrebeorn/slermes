/* noc_w12_pluginchoi_f29673.h — public C API port of hermes_cli/plugin_choices.py */
#ifndef NOC_W12_PLUGINCHOI_F29673_H
#define NOC_W12_PLUGINCHOI_F29673_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _options: fill `out` (a json array) with [value, plain label, shown label]
 * triples for each choice (a string or a dict with value/label/description). */
void w12_pluginchoi_f29673_options(const json_t *choices, json_t *out);

/* ask_choice core: with the surfaces' asker returning `picked`, map it back to
 * the matching choice value. Returns false for no asker/no options/no match. */
bool w12_pluginchoi_f29673_ask_choice(const json_t *choices, const char *picked,
                    char *out, size_t cap);

/* clarify_asker core: the q0 answer of a clarify-tool reply, only when it is a
 * string. Returns false for None/non-string answers. */
bool w12_pluginchoi_f29673_clarify_ask(const json_t *reply, char *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_PLUGINCHOI_F29673_H */
