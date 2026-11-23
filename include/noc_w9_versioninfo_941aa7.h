#ifndef NOC_W9_VERSIONINFO_941AA7_H
#define NOC_W9_VERSIONINFO_941AA7_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_versioninfo_941aa7_derived_version @ hermes_cli/version_info.py:_derived_version
 * distance_str is a decimal int64 string; "" is the None sentinel, as is
 * short_commit.  Returns a JSON string. */
struct json_t *w9_versioninfo_941aa7_derived_version(const char *base_version,
                                                     const char *distance_str,
                                                     int dirty,
                                                     const char *short_commit);

/* PoP: w9_versioninfo_941aa7_parse_nonnegative @ hermes_cli/version_info.py:_parse_nonnegative
 * int(value or "") with a >= 0 guard; invalid/negative -> JSON null. */
struct json_t *w9_versioninfo_941aa7_parse_nonnegative(const char *value);

/* PoP: w9_versioninfo_941aa7_display_version @ hermes_cli/version_info.py:VersionInfo.display_version
 * The property seam: base == "unknown" -> derived_version, otherwise
 * _derived_version(base, distance) with dirty/short_commit defaults. */
struct json_t *w9_versioninfo_941aa7_display_version(const char *base_version,
                                                     const char *derived_version,
                                                     const char *distance_str);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W9_VERSIONINFO_941AA7_H */
