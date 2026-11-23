#ifndef NOC_W5_BUILDENV_1EB2C8_H
#define NOC_W5_BUILDENV_1EB2C8_H

/* C home for upstream Hermes pm/build_env.py — gate w5_buildenv_1eb2c8. */
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_buildenv_1eb2c8_parse_requirements @ pm/build_env.py:main
 * Mirrors the requirements-file filter in main():
 *   [line.strip() for line in text.splitlines()
 *    if line.strip() and not line.lstrip().startswith("#")]
 * Returns a JSON array of strings; caller frees. */
json_t *w5_buildenv_1eb2c8_parse_requirements(const char *text);

/* PoP: w5_buildenv_1eb2c8_record_selection_invalid @ pm/build_env.py:_check_record_selection
 * 1 when --record-selection is rejected for this argument shape, else 0. */
int w5_buildenv_1eb2c8_record_selection_invalid(int record_selection, int build,
        int requirements_count, int manager_runtime, int all_extras,
        int groups_count, int extras_count);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_BUILDENV_1EB2C8_H */
