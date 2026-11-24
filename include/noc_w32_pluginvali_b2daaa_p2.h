/*
 * noc_w32_pluginvali_b2daaa_p2.h - C surface for upstream
 * hermes_cli/plugin_validate_core_override.py (gate w32_pluginvali_b2daaa_p2).
 *
 * Second half of the core-override lint: a receiver chain rendered as the
 * finding names it, the private-state sink predicates that chain has to pass,
 * the call-site -> helper resolution with its container-receiver exemption, and
 * the report verdict/message shaping. The AST itself is a seam: callers hand in
 * a chain value ({"kind":"name"|"attr"|"sub"|"call"|"const"|"binop", ...}).
 */
#ifndef NOC_W32_PLUGINVALI_B2DAAA_P2_H
#define NOC_W32_PLUGINVALI_B2DAAA_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w32_pluginvali_b2daaa_p2_render @ hermes_cli/plugin_validate_core_override.py:_render
 * ``get_plugin_manager()._hooks[...]``: the chain as a finding names it.
 * Attribute -> "<value>.<attr>", subscript -> "<value>[...]", call -> "<func>()",
 * a bare name -> its id, anything else -> "<core>". Caller frees. */
char *w32_pluginvali_b2daaa_p2_render(const json_t *chain);

/* PoP: w32_pluginvali_b2daaa_p2_dotted @ hermes_cli/plugin_validate_core_override.py:_dotted
 * The dotted attribute chain: attribute segments joined innermost-last, a trailing
 * bare name kept, and a non-name terminal (call / subscript / constant) dropped
 * while its attributes stay (``f().x`` -> "x"). Caller frees. */
char *w32_pluginvali_b2daaa_p2_dotted(const json_t *chain);

/* PoP: w32_pluginvali_b2daaa_p2_has_private_segment @ hermes_cli/plugin_validate_core_override.py:_has_private_segment
 * Whether any attribute in the chain starts with "_" (``mgr._hooks[...]``,
 * ``m.__dict__``). Calls are followed through their func. */
int w32_pluginvali_b2daaa_p2_has_private_segment(const json_t *chain);

/* PoP: w32_pluginvali_b2daaa_p2_has_mutator_call @ hermes_cli/plugin_validate_core_override.py:_has_mutator_call
 * Whether the chain already holds a mutator CALL (``m._hooks.setdefault(k, []).append``):
 * only a ``Call`` whose func is one of ``_MUTATORS`` counts, a bare ``.append`` does not. */
int w32_pluginvali_b2daaa_p2_has_mutator_call(const json_t *chain);

/* PoP: w32_pluginvali_b2daaa_p2_func_key @ hermes_cli/plugin_validate_core_override.py:_Analysis._func_key
 * The helper name a call site resolves to: the last dotted segment of ``dotted``,
 * mapped through ``aliases`` when that name is an import alias. Caller frees. */
char *w32_pluginvali_b2daaa_p2_func_key(const char *dotted, const json_t *aliases);

/* PoP: w32_pluginvali_b2daaa_p2_helper_indices @ hermes_cli/plugin_validate_core_override.py:_Analysis.helper_indices
 * Argument positions the call hands to a patching / mutating helper, matched by
 * name -- unless the call is made ON a container (a display / un-rebound
 * constructor, or a name every binding in the module makes a container): that is
 * the container's own method, not a same-named plugin helper.
 *   spec   {"kind":"call","func":<chain>}
 *   table  {"<helper name>": [<arg index>, ...]}
 * ``containers`` / ``rebound`` are the module's exempt receiver names and rebound
 * constructors. Returns a sorted, deduplicated index array. */
json_t *w32_pluginvali_b2daaa_p2_helper_indices(const json_t *spec, const json_t *table,
                                                const json_t *aliases, const json_t *containers,
                                                const json_t *rebound);

/* PoP: w32_pluginvali_b2daaa_p2_report @ hermes_cli/plugin_validate_core_override.py:check_core_override
 * The report entry for a hit list: {"label","ok","detail"}. Clean -> ok and
 * "no runtime rebinds of Hermes core"; otherwise the first 8 hits joined by
 * "; " behind the "use a public hook, middleware or provider profile" advice,
 * with " (+N more)" when the list is longer. */
json_t *w32_pluginvali_b2daaa_p2_report(const json_t *hits);

#ifdef __cplusplus
}
#endif
#endif
