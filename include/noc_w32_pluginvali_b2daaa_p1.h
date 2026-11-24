/*
 * noc_w32_pluginvali_b2daaa_p1.h - C surface for upstream
 * hermes_cli/plugin_validate_core_override.py (gate w32_pluginvali_b2daaa_p1).
 *
 * First half of the "a plugin must not rebind Hermes core" admission lint: the
 * value layer that decides *what* a scan looks at and *what counts as core*.
 * The plugin directory walk, the ``ast.parse`` and the import closure stay
 * seams; this file owns the predicates and set algebra those scans consult.
 */
#ifndef NOC_W32_PLUGINVALI_B2DAAA_P1_H
#define NOC_W32_PLUGINVALI_B2DAAA_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w32_pluginvali_b2daaa_p1_is_test_file @ hermes_cli/plugin_validate_core_override.py:_is_test_file
 * ``name.startswith("test_") or name.endswith("_test.py") or name == "conftest.py"``
 * over the last path component of ``rel`` (both separators, trailing slash ignored). */
int w32_pluginvali_b2daaa_p1_is_test_file(const char *rel);

/* PoP: w32_pluginvali_b2daaa_p1_should_skip @ hermes_cli/plugin_validate_core_override.py:_python_files
 * ``_SKIP_DIRS.intersection(rel.parts[:-1]) or _is_test_file(rel)``: a file with a
 * skipped directory segment, or a test file, is not part of the scan. */
int w32_pluginvali_b2daaa_p1_should_skip(const char *rel);

/* PoP: w32_pluginvali_b2daaa_p1_core_module_name @ hermes_cli/plugin_validate_core_override.py:_Analysis._core_module_name
 * ``isinstance(name, str) and name.split(".")[0] in core_roots``. ``name`` is the
 * dotted module string; a non-string value is never core. */
int w32_pluginvali_b2daaa_p1_core_module_name(const json_t *name, const json_t *roots);

/* PoP: w32_pluginvali_b2daaa_p1_core_roots @ hermes_cli/plugin_validate_core_override.py:_Analysis.__init__
 * PoP: w32_pluginvali_b2daaa_p1_core_roots @ hermes_cli/plugin_validate_core_override.py:_hermes_top_level
 * ``(set(packages) | set(modules) | {m.split(".")[0] for m in moves} | {"hermes_plugins"})
 *  - {"setup", "conftest"} - set(local)``, sorted.  Package/module names enter
 * whole; only a MOVES key is reduced to its first segment. */
json_t *w32_pluginvali_b2daaa_p1_core_roots(const json_t *packages, const json_t *modules,
                                            const json_t *moves, const json_t *local);

/* PoP: w32_pluginvali_b2daaa_p1_lookup_is_core @ hermes_cli/plugin_validate_core_override.py:_Analysis._lookup_is_core
 * Whether a module-name expression names Hermes. ``spec`` is one of
 *   {"kind":"literal","value":<any>}     -- ast.Constant(value)
 *   {"kind":"name","id":"NAME"}          -- ast.Name, resolved through ``constants``
 *   {"kind":"fstring","head":<any>}      -- JoinedStr whose values[0] is that constant
 *   {"kind":"fstring_empty"}             -- JoinedStr with no values
 *   {"kind":"fstring"}                   -- JoinedStr whose values[0] is formatted
 * A name resolves to its module-level string constant (``constants`` maps the
 * name to its dotted value); an f-string is judged only on its literal head,
 * which must itself contain the "." separator. */
int w32_pluginvali_b2daaa_p1_lookup_is_core(const json_t *spec, const json_t *constants,
                                            const json_t *roots);

/* PoP: w32_pluginvali_b2daaa_p1_is_container @ hermes_cli/plugin_validate_core_override.py:_Analysis.is_container
 * A display (``{...}``, ``[...]``, ``{...} set``, and the three comprehensions) is a
 * container; so is a call to a container constructor the module does not rebind.
 * ``spec`` is the node to judge:
 *   {"kind":"dict"|"list"|"set"|"dictcomp"|"listcomp"|"setcomp"}   -- a display
 *   {"kind":"call","func":<chain>}                                 -- a constructor call
 * ``rebound`` is the list of container-constructor names the module rebinds. */
int w32_pluginvali_b2daaa_p1_is_container(const json_t *spec, const json_t *rebound);

/* PoP: w32_pluginvali_b2daaa_p1_bound_names @ hermes_cli/plugin_validate_core_override.py:_bound_names
 * The names a target list binds: ``Name`` ids, recursing through ``tuple`` / ``list``
 * unpacking targets, ignoring attribute / subscript targets. Sorted, deduplicated.
 * ``targets`` entries: {"kind":"name","id":...} | {"kind":"tuple"|"list","elts":[...]} | other. */
json_t *w32_pluginvali_b2daaa_p1_bound_names(const json_t *targets);

/* PoP: w32_pluginvali_b2daaa_p1_module_constants @ hermes_cli/plugin_validate_core_override.py:<module constants>
 * ``_SKIP_DIRS`` / ``_CONTAINER_CTORS`` / ``_NAMESPACE_ESCAPES`` / ``_MUTATORS`` as
 * sorted string arrays. */
json_t *w32_pluginvali_b2daaa_p1_module_constants(void);

#ifdef __cplusplus
}
#endif
#endif
