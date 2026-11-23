#ifndef NOC_W8_PLUGINDECLA_451F92_H
#define NOC_W8_PLUGINDECLA_451F92_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w8_plugindecla_451f92_manifest_version_error @ pm/plugin_declarations.py:manifest_version_error
 * requires_hermes_error() is injected via a fixture seam (fixture key
 * "requires_error", str|null). SUPPORTED_MANIFEST_VERSION == 2. Returns the
 * load-blocking message or JSON null. */
struct json_t *w8_plugindecla_451f92_manifest_version_error(const char *name,
                                                          const struct json_t *manifest,
                                                          const struct json_t *requires_error);

/* PoP: w8_plugindecla_451f92_declaration_external @ pm/plugin_declarations.py:PythonDeclaration.external
 * str(manifest.get("python_runtime") or "").strip().lower() == "external". */
int w8_plugindecla_451f92_declaration_external(const struct json_t *manifest);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_PLUGINDECLA_451F92_H */
