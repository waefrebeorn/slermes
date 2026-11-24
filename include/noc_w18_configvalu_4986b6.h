#ifndef NOC_W18_CONFIGVALU_4986B6_H
#define NOC_W18_CONFIGVALU_4986B6_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_configvalu_4986b6_validate_voice @ hermes_cli/config_value_checks.py:_validate_voice
 * Returns a JSON array of {"severity","message","hint"} issue objects. */
struct json_t *w18_configvalu_4986b6_validate_voice(const struct json_t *config);

/* PoP: w18_configvalu_4986b6_validate_timezone @ hermes_cli/config_value_checks.py:_validate_timezone
 * tz_available stands in for ZoneInfo("UTC") succeeding; valid_zones is the
 * injected set of names ZoneInfo() accepts. Same issue-object array result. */
struct json_t *w18_configvalu_4986b6_validate_timezone(const struct json_t *config,
                                                       int tz_available,
                                                       const struct json_t *valid_zones);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_CONFIGVALU_4986B6_H */
