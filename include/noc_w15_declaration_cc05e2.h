#ifndef NOC_W15_DECLARATION_CC05E2_H
#define NOC_W15_DECLARATION_CC05E2_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_declaration_cc05e2_gpu_label @ hermes_platform/declaration.py:gpu_label
 * NULL (python None) / "" -> "a supported GPU"; "nvidia" -> "an NVIDIA GPU".
 * Returns a malloc'd string (caller frees). */
char *w15_declaration_cc05e2_gpu_label(const char *gpu);

/* PoP: w15_declaration_cc05e2_location_is_rooted @ hermes_platform/declaration.py:location_is_rooted
 * osf is a NUL-terminated OS family string ("win32" uses the drive regex). */
int w15_declaration_cc05e2_location_is_rooted(const char *location, const char *osf);

/* PoP: w15_declaration_cc05e2_is_relative_name @ hermes_platform/declaration.py:_is_relative_name
 * Non-string JSON values are false (python: not isinstance(value, str)). */
int w15_declaration_cc05e2_is_relative_name(const struct json_t *value, int allow_dirs);

/* PoP: w15_declaration_cc05e2_parse_requires @ hermes_platform/declaration.py:parse_requires
 * `app_per_os` maps os_family -> version_kind string; JSON null means no app block.
 * Returns a NEW object {"app":bool,"min_version":str|null,"gpu":str|null};
 * json_null() input `raw` returns the default RequiresSpec object. Caller frees. */
struct json_t *w15_declaration_cc05e2_parse_requires(const struct json_t *raw,
                                                     const struct json_t *app_per_os);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_DECLARATION_CC05E2_H */
