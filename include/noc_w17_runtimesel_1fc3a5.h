#ifndef NOC_W17_RUNTIMSEL_1FC3A5_H
#define NOC_W17_RUNTIMSEL_1FC3A5_H

#include "hermes_json.h"

/* Pure helpers from agent/runtime_self_protection.py. */

/* _version_spec_matches(spec, version, uv_root_name) */
int w17_runtimesel_1fc3a5_version_spec_matches(const char *spec, const char *version,
                                              const char *uv_root_name);
/* _overlaps(a, b) -- uses os.sep; on this platform that is '\\'. */
int w17_runtimesel_1fc3a5_overlaps(const char *a, const char *b);
/* _strip_prefixes(words) -> new JSON array of the remaining words; caller owns it. */
json_t *w17_runtimesel_1fc3a5_strip_prefixes(json_t *words);

#endif
