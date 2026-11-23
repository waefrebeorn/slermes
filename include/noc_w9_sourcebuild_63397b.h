#ifndef NOC_W9_SOURCEBUILD_63397B_H
#define NOC_W9_SOURCEBUILD_63397B_H

#ifdef __cplusplus
extern "C" {
#endif

struct json_t;

/* PoP: w9_sourcebuild_63397b_npmrc_script_settings @ hermes_cli/source_build.py:_npmrc_script_settings
 * `settings` maps npmrc keys to string values. Returns the ordered JSON array
 * of [key, value] pairs that are plain settings and not credentials. */
struct json_t *w9_sourcebuild_63397b_npmrc_script_settings(const struct json_t *settings);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_SOURCEBUILD_63397B_H */
