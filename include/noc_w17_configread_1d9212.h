#ifndef NOC_W17_CONFIGREAD_1D9212_H
#define NOC_W17_CONFIGREAD_1D9212_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_read_errors.py (gate w17_configread_1d9212).
 * Pure message builders for unreadable / unparseable config.yaml. Exception
 * attributes and path spelling are injected from the fixture. */
json_t *w17_configread_1d9212_yaml_error_location(const json_t *exc);
json_t *w17_configread_1d9212_yaml_error_details(const json_t *exc);
json_t *w17_configread_1d9212_format_config_parse_failure(const char *path,
                                                          const json_t *exc,
                                                          const char *fallback);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_CONFIGREAD_1D9212_H */
